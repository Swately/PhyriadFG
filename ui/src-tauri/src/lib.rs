// PhyriadFG launcher — Tauri 2 backend.
//
// This is a LAUNCHER ONLY: it assembles CLI flags, spawns phyriad_fg.exe, streams its
// stdout/stderr line-by-line to the frontend (live status), and can stop the process.
// It NEVER writes into the running FG (the FG's Config is startup-set + read unsynchronized);
// all flags apply at launch.

use std::io::{BufRead, BufReader, Read, Write};
use std::process::{Child, Command, Stdio};
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Arc, Mutex};
use std::time::Duration;

use tauri::{AppHandle, Emitter, Manager, State};

/// The frame generator, carried inside this binary (see build.rs). EMPTY when the launcher was
/// built in a tree whose C++ had never been built — the on-disk lookup below then behaves exactly
/// as it always did, which is what keeps a launcher-only `cargo build` useful.
static EMBEDDED_FG: &[u8] = include_bytes!(concat!(env!("OUT_DIR"), "/phyriad_fg_embedded.bin"));
/// FNV-1a of that payload. The extracted file is NAMED by it, so a new build writes a NEW file
/// instead of racing to overwrite one that an older launcher may still be running.
static EMBEDDED_FG_HASH: &str = env!("PFG_EMBEDDED_HASH");

/// Write the embedded frame generator out, once, and return where it landed.
///
/// `%LOCALAPPDATA%\\PhyriadFG\\bin\\phyriad_fg-<hash>.exe`, and NOT the temp directory: temp is swept by
/// Windows and by cleaners, and re-extracting 1.3 MB on every launch to a path that may vanish
/// mid-run is worse than owning a stable one. Re-extraction is skipped when the file is already
/// there at the right size, so the cost is paid once per version.
///
/// EVERY failure returns None rather than propagating: the caller's next move is the on-disk lookup,
/// which is a perfectly good answer. A launcher that refuses to start because it could not write a
/// cache file would be worse than one that just asks where the FG is.
#[cfg(windows)]
fn extract_embedded_fg() -> Option<std::path::PathBuf> {
    if EMBEDDED_FG.is_empty() {
        return None;
    }
    let base = std::env::var_os("LOCALAPPDATA").map(std::path::PathBuf::from)?;
    let dir = base.join("PhyriadFG").join("bin");
    std::fs::create_dir_all(&dir).ok()?;
    let exe = dir.join(format!("phyriad_fg-{EMBEDDED_FG_HASH}.exe"));

    // Already extracted at the right size? Then it is this exact payload — the name carries the
    // content hash, so a same-name/same-size file cannot be a different build.
    let need_write = match std::fs::metadata(&exe) {
        Ok(m) => m.len() != EMBEDDED_FG.len() as u64,
        Err(_) => true,
    };
    if need_write {
        // Write to a temp name and rename, so a half-written exe is never left behind under the
        // real name if the launcher dies mid-write.
        let tmp = dir.join(format!("phyriad_fg-{EMBEDDED_FG_HASH}.part"));
        std::fs::write(&tmp, EMBEDDED_FG).ok()?;
        // rename() over an existing file fails on Windows; remove first, best-effort.
        let _ = std::fs::remove_file(&exe);
        std::fs::rename(&tmp, &exe).ok()?;
    }

    // Best-effort sweep of payloads from OTHER versions. Ignore every error: one of them may be a
    // running process from another launcher instance, and failing to delete it is not our problem.
    if let Ok(rd) = std::fs::read_dir(&dir) {
        for e in rd.flatten() {
            let n = e.file_name();
            let n = n.to_string_lossy();
            if n.starts_with("phyriad_fg-") && n.ends_with(".exe") && e.path() != exe {
                let _ = std::fs::remove_file(e.path());
            }
        }
    }
    Some(exe)
}

#[cfg(not(windows))]
fn extract_embedded_fg() -> Option<std::path::PathBuf> {
    None
}

/// Where the FG lives, in resolution order:
///   1. `phyriad_fg.exe` NEXT TO THE LAUNCHER — the portable/dev layout. It wins on purpose: a
///      developer who drops a freshly built binary beside the launcher expects to run THAT one, and
///      an embedded copy silently taking precedence would make testing a new build impossible.
///   2. the embedded payload, extracted to LOCALAPPDATA — the single-file download path.
///   3. the bare name, left for the OS to resolve, which is also the honest thing to show in an
///      error message when neither of the above exists.
/// Overridable at any time from the UI's executable-path field, which beats all three.
fn default_exe() -> String {
    if let Some(beside) = std::env::current_exe()
        .ok()
        .and_then(|p| p.parent().map(|d| d.join("phyriad_fg.exe")))
    {
        if beside.is_file() {
            if let Some(s) = beside.to_str() {
                return s.to_string();
            }
        }
    }
    if let Some(p) = extract_embedded_fg() {
        if let Some(s) = p.to_str() {
            return s.to_string();
        }
    }
    "phyriad_fg.exe".to_string()
}

/// Managed state: the single child process handle, behind a mutex so `stop`/`is_running`
/// and the stdout-reaper thread can race for it safely (whoever takes it first reaps it).
///
/// MULTI-FG (DESIGN ONLY — NOT IMPLEMENTED): parallel multi-instance FG would replace this
/// single `Option<Child>` with a list (`Vec<Child>` / `HashMap<id, Child>`), one child per
/// captured window. See the matching note at `launch()` for how the spawn side would fan out.
struct FgState {
    child: Arc<Mutex<Option<Child>>>,
    /// Epoch / generación: cada hijo almacenado recibe un epoch único y creciente. El hilo
    /// lector de stdout de un hijo recuerda SU epoch (`my_epoch`); al llegar a EOF solo
    /// cosecha (take + wait) el hijo y emite `fg-exit` si el epoch GLOBAL sigue siendo el suyo.
    /// Si un `restart()` ya colocó un hijo nuevo (epoch avanzado), el lector del hijo VIEJO
    /// ve un epoch desfasado y sale en silencio sin tocar el slot — así el lector del hijo
    /// MUERTO nunca cosecha al hijo NUEVO ni dispara un `fg-exit` falso. El guard se cierra de
    /// forma atómica en `spawn_fg` (store + bump del epoch bajo el MISMO lock del slot).
    epoch: Arc<AtomicU64>,
}

/// Estado gestionado del Job Object de Windows (FIX A — kill-on-close).
///
/// Se crea EXACTAMENTE UN job al arrancar la app y su handle se conserva durante toda
/// la vida del proceso. Cada hijo del FG se ata a este job; cuando ui.exe termina por
/// CUALQUIER motivo (cierre limpio O crash), el último handle del job se libera y el SO
/// — gracias a `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE` — mata a todos los hijos atados.
/// Así dejan de quedar huérfanos.
///
/// El `HANDLE` crudo de Windows no es `Send`/`Sync`, pero el estado gestionado de Tauri
/// lo exige. Por eso guardamos el handle como `isize` y reconstruimos el `HANDLE` cuando
/// hace falta, marcando el newtype como `Send`+`Sync` de forma explícita (el job se crea
/// una vez y nunca se muta tras el arranque, así que compartirlo entre hilos es seguro).
/// `handle == 0` significa que la creación del job falló (se degrada a "sin job").
#[cfg(windows)]
struct JobState {
    handle: isize,
}
#[cfg(windows)]
unsafe impl Send for JobState {}
#[cfg(windows)]
unsafe impl Sync for JobState {}

/// Crea el Job Object con `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`. Se llama una sola vez al
/// arrancar; su handle vive en `JobState` durante toda la app. Si algo falla, devuelve un
/// `JobState { handle: 0 }` (la app sigue funcionando, solo sin la garantía de kill-on-close).
#[cfg(windows)]
fn create_job() -> JobState {
    use windows::core::PCWSTR;
    use windows::Win32::System::JobObjects::{
        CreateJobObjectW, SetInformationJobObject, JobObjectExtendedLimitInformation,
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION, JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE,
    };
    unsafe {
        match CreateJobObjectW(None, PCWSTR::null()) {
            Ok(handle) => {
                // Configurar la política: matar a los hijos cuando se cierre el último handle.
                let mut info = JOBOBJECT_EXTENDED_LIMIT_INFORMATION::default();
                info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
                let _ = SetInformationJobObject(
                    handle,
                    JobObjectExtendedLimitInformation,
                    &info as *const _ as *const core::ffi::c_void,
                    std::mem::size_of::<JOBOBJECT_EXTENDED_LIMIT_INFORMATION>() as u32,
                );
                JobState { handle: handle.0 as isize }
            }
            // Sin job: los hijos podrían quedar huérfanos, pero la app no debe caerse por esto.
            Err(_) => JobState { handle: 0 },
        }
    }
}

/// Observer log — espejo a DISCO de todo lo que ve la UI: el argv exacto de cada
/// lanzamiento/reinicio, cada línea de stdout/stderr del FG, los eventos de ciclo de vida
/// (exit/stop) y las NOTAS del operador, todo con timestamp relativo al arranque de la app.
/// Propósito: que un observador externo (otro humano o un LLM con acceso al filesystem)
/// pueda seguir una sesión de pruebas en tiempo real con un `tail -f` del archivo, con el
/// operador marcando momentos ("aquí vibra") desde la propia UI. El archivo vive junto a
/// ui.exe (`observer-live.log`, append entre sesiones); si no se puede abrir, todo degrada
/// a no-op y la UI sigue funcionando igual.
struct ObserverLog {
    start: std::time::Instant,
    path: String,
    file: Mutex<Option<std::fs::File>>,
}

impl ObserverLog {
    fn create() -> Arc<ObserverLog> {
        let path = std::env::current_exe()
            .ok()
            .and_then(|p| p.parent().map(|d| d.join("observer-live.log")))
            .and_then(|p| p.to_str().map(|s| s.to_string()))
            .unwrap_or_else(|| "observer-live.log".to_string());
        let file = std::fs::OpenOptions::new()
            .create(true)
            .append(true)
            .open(&path)
            .ok();
        let log = Arc::new(ObserverLog {
            start: std::time::Instant::now(),
            path,
            file: Mutex::new(file),
        });
        let unix_ms = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .map(|d| d.as_millis())
            .unwrap_or(0);
        log.write("session", &format!("=== UI session start (unix_ms {}) ===", unix_ms));
        log
    }

    /// Append `[+  12.345s] [tag] line`. Best-effort: sin archivo, no-op. Cada línea es un
    /// write directo al SO (File no bufferea en Rust), así el tail externo la ve al instante.
    fn write(&self, tag: &str, line: &str) {
        if let Ok(mut g) = self.file.lock() {
            if let Some(f) = g.as_mut() {
                let t = self.start.elapsed().as_secs_f64();
                let _ = writeln!(f, "[+{:>10.3}s] [{}] {}", t, tag, line);
            }
        }
    }
}

/// Snapshot del observer del `AppHandle` (None si el estado no está gestionado — imposible en
/// la práctica, pero el acceso degrada limpio en vez de hacer panic).
fn observer(app: &AppHandle) -> Option<Arc<ObserverLog>> {
    app.try_state::<Arc<ObserverLog>>().map(|s| s.inner().clone())
}

/// One enumerated top-level window, surfaced to the UI's target-window selector.
///
/// `hwnd` is the STABLE identity of the row: titles are volatile and duplicated across windows
/// (L-4), so the UI keys its dropdown on `hwnd` and only uses `title` as the value it copies
/// into `--window`. Typed `u64`, NOT `isize`: the UI serialises it into `--hwnd <decimal>` and
/// the FG parses that with `std::strtoull`, which would not round-trip a negative decimal.
/// Win32 window handles are documented as sign-extended 32-bit values, so the value also fits
/// exactly in a JavaScript number.
#[derive(serde::Serialize, Clone)]
struct WindowInfo {
    /// Visible window title (the substring the FG's `--window` flag matches against).
    title: String,
    /// File name of the owning process's executable (e.g. "bf6.exe").
    exe: String,
    /// Owning process id.
    pid: u32,
    /// Win32 window handle — the stable key for this row (L-4).
    hwnd: u64,
    /// True when the window is MINIMIZED. Reported, not filtered: a minimized target gives WGC
    /// a degenerate client rect and the run silently captures the whole monitor instead (B-3),
    /// so the UI must keep these rows out of the selectable set (or disable them) and say why.
    iconic: bool,
}

/// Payload of the `fg-exit` event.
///
/// BREAKING vs the previous shape (a bare `Option<i32>` exit code): the frontend's restart guard
/// was time-based and swallowed the NEW child's fatal exit (L-2 / E-7), so the epoch of the child
/// that died now travels WITH the event. `launch`/`restart` return the epoch they created, so the
/// UI can discard exactly the exits that are OLDER than the restart in flight and act on every
/// other one. Any consumer reading `e.payload` as a number must be updated in the same change.
#[derive(serde::Serialize, Clone)]
struct FgExit {
    /// Process exit code, or `null` (unknown — e.g. the child was already reaped by `stop`).
    code: Option<i32>,
    /// Epoch of the child that exited (see `FgState::epoch`).
    epoch: u64,
}

fn resolve_exe(exe_path: Option<String>) -> String {
    match exe_path {
        Some(p) if !p.trim().is_empty() => p,
        _ => default_exe(),
    }
}

/// Apply Windows-only spawn flags: no extra console window for the child.
#[cfg(windows)]
fn no_window(cmd: &mut Command) {
    use std::os::windows::process::CommandExt;
    const CREATE_NO_WINDOW: u32 = 0x0800_0000;
    cmd.creation_flags(CREATE_NO_WINDOW);
}
#[cfg(not(windows))]
fn no_window(_cmd: &mut Command) {}

/// Same as `no_window`, plus `CREATE_NEW_PROCESS_GROUP` — used ONLY for the FG child (C-7).
///
/// The new group makes the child the ROOT of its own process group, whose group id is its pid;
/// that is what lets `try_graceful_stop` address `GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pid)`
/// at this child alone and not at the launcher. Note `creation_flags` REPLACES the flag word, so
/// both constants are set in one call. Side effect of the new group, documented by Win32: CTRL+C
/// is disabled for it — irrelevant here, since the FG's handler (src/core/globals.cpp:46-49) also
/// accepts CTRL_BREAK_EVENT and that is the event we send.
#[cfg(windows)]
fn no_window_new_group(cmd: &mut Command) {
    use std::os::windows::process::CommandExt;
    const CREATE_NO_WINDOW: u32 = 0x0800_0000;
    const CREATE_NEW_PROCESS_GROUP: u32 = 0x0000_0200;
    cmd.creation_flags(CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP);
}
#[cfg(not(windows))]
fn no_window_new_group(_cmd: &mut Command) {}

/// C-9 — bind one spawned child to the app's kill-on-close Job Object.
///
/// Factored out of `spawn_fg` so the `--list-monitors` / `--layer-model-json` PROBE children get
/// the same guarantee: without it a probe that hangs inside the driver outlives ui.exe as an
/// orphan. Returns the Win32 error text instead of logging, so each caller keeps its own voice
/// (`spawn_fg` announces success, the probes stay quiet unless it fails). A missing `JobState`
/// (job creation failed at startup) degrades to `Ok(())` — same "run without the guarantee"
/// posture the original inline block had.
#[cfg(windows)]
fn bind_to_job(app: &AppHandle, child: &Child) -> Result<(), String> {
    use std::os::windows::io::AsRawHandle;
    use windows::Win32::Foundation::HANDLE;
    use windows::Win32::System::JobObjects::AssignProcessToJobObject;
    let job = match app.try_state::<JobState>() {
        Some(j) => j,
        None => return Ok(()),
    };
    unsafe {
        AssignProcessToJobObject(HANDLE(job.handle as _), HANDLE(child.as_raw_handle() as _))
    }
    .map_err(|e| e.to_string())
}
#[cfg(not(windows))]
fn bind_to_job(_app: &AppHandle, _child: &Child) -> Result<(), String> {
    Ok(())
}

/// C-7 — ask the FG to shut down CLEANLY, and report whether it did.
///
/// Why this exists: `stop` used to be TerminateProcess only, so the FG's teardown never ran and
/// its `-stats.csv` (written at the tail of the telemetry drain loop) was unreachable from the
/// launcher. The FG already has the receiving half — `console_ctrl_handler` in
/// src/core/globals.cpp:46-49 sets `g_quit` on CTRL_BREAK_EVENT, registered at
/// src/core/main.cpp:219 — so all that is missing is a sender.
///
/// Why the console dance: `GenerateConsoleCtrlEvent` only reaches processes attached to the
/// CALLER's console, and release ui.exe is `windows_subsystem = "windows"` — it has no console.
/// So we temporarily attach to the CHILD's console (it owns one; CREATE_NO_WINDOW gives a console
/// with no window), disable the event for ourselves while attached, signal the child's process
/// group, and detach. The group id is the child's pid because it was spawned with
/// CREATE_NEW_PROCESS_GROUP, so the launcher — which is in a different group — is not signalled.
///
/// HONESTY: whether AttachConsole succeeds against a CREATE_NO_WINDOW child was NOT executed when
/// this was written. It does not have to be true for this code to be safe: every step is checked
/// and any failure returns `Err`, at which point the caller does exactly what it did before
/// (kill + wait). The caller logs which branch ran, so one Stop click settles the question.
///
/// Returns `Ok(())` only if the child actually EXITED within `timeout` after the event; every
/// other path returns `Err` carrying the reason, which the caller prints verbatim.
#[cfg(windows)]
fn try_graceful_stop(child: &mut Child, timeout: Duration) -> Result<(), String> {
    use windows::Win32::System::Console::{
        AttachConsole, FreeConsole, GenerateConsoleCtrlEvent, SetConsoleCtrlHandler,
        CTRL_BREAK_EVENT,
    };
    // AttachConsole/FreeConsole are PROCESS-wide; serialize so two commands never interleave.
    static CONSOLE_LOCK: Mutex<()> = Mutex::new(());
    let _g = CONSOLE_LOCK.lock().map_err(|e| e.to_string())?;

    let pid = child.id();
    unsafe {
        let _ = FreeConsole(); // no-op when we have none; required if we somehow do
        AttachConsole(pid).map_err(|e| format!("AttachConsole failed ({})", e))?;
        // Ignore the event in THIS process while attached (belt-and-suspenders: the child is in
        // its own group, so it should not reach us anyway).
        let _ = SetConsoleCtrlHandler(None, true);
        let sent = GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pid);
        let _ = FreeConsole();
        let _ = SetConsoleCtrlHandler(None, false);
        sent.map_err(|e| format!("GenerateConsoleCtrlEvent failed ({})", e))?;
    }

    let deadline = std::time::Instant::now() + timeout;
    loop {
        match child.try_wait() {
            Ok(Some(_)) => return Ok(()),
            Ok(None) => {}
            Err(e) => return Err(format!("try_wait failed ({})", e)),
        }
        if std::time::Instant::now() >= deadline {
            return Err(format!("no exit within {} ms", timeout.as_millis()));
        }
        std::thread::sleep(Duration::from_millis(25));
    }
}
#[cfg(not(windows))]
fn try_graceful_stop(_child: &mut Child, _timeout: Duration) -> Result<(), String> {
    Err("not supported on this platform".to_string())
}

/// L-6 — cheap pre-flight so `restart` never destroys a live run for a path that cannot spawn.
/// Catches the dominant real case (a half-typed executable path arriving through the 1 s
/// auto-restart debounce). It is NOT a spawn: a path that exists can still fail to launch, which
/// is why `restart` still reports a failed `spawn_fg` to the frontend.
fn validate_exe(exe: &str) -> Result<(), String> {
    if std::path::Path::new(exe).is_file() {
        Ok(())
    } else {
        Err(format!("Executable not found: '{}'", exe))
    }
}

/// Lanza el FG (`exe_path` o el default) con `args`, lo ata al Job Object (kill-on-close), lo
/// guarda en el slot ASIGNÁNDOLE un epoch nuevo (de forma ATÓMICA bajo el mismo lock del slot)
/// y arranca los dos hilos lectores (stdout/stderr) que reenvían cada línea como evento
/// `fg-log`. El lector de stdout, al EOF (proceso saliendo), cosecha el hijo y emite `fg-exit`
/// con el código de salida SOLO si su epoch sigue vigente (ver `FgState::epoch`).
///
/// Helper COMPARTIDO por `launch` y `restart`: misma ruta de spawn + mismos lectores, sin
/// duplicar lógica. No hace el chequeo de "ya hay uno vivo" (eso es responsabilidad de
/// `launch`; `restart` precisamente reemplaza el que hay).
fn spawn_fg(
    app: &AppHandle,
    state: &FgState,
    args: Vec<String>,
    exe_path: Option<String>,
) -> Result<u64, String> {
    let exe = resolve_exe(exe_path);
    let obs = observer(app);
    if let Some(o) = &obs {
        o.write("launch", &format!("exe={} argv: {}", exe, args.join(" ")));
    }
    let mut cmd = Command::new(&exe);
    cmd.args(&args).stdout(Stdio::piped()).stderr(Stdio::piped());
    // C-7: CREATE_NEW_PROCESS_GROUP as well, so `stop` can address CTRL_BREAK at this child.
    no_window_new_group(&mut cmd);

    let mut child = cmd.spawn().map_err(|e| {
        let msg = format!("Failed to start '{}': {}", exe, e);
        if let Some(o) = &obs {
            o.write("error", &msg);
        }
        msg
    })?;

    let stdout = child.stdout.take();
    let stderr = child.stderr.take();

    // FIX A — atar el hijo al Job Object (kill-on-close) mientras todavía tenemos el `child`.
    // El handle crudo sigue siendo válido tras moverlo a FgState, pero lo hacemos aquí, antes
    // de almacenarlo, para no tener que volver a pedir prestado el child. Si falla, avisamos
    // pero NO abortamos el lanzamiento (degradación: el hijo podría quedar huérfano).
    // Asignar tras el spawn funciona en Win8+ (se permiten jobs anidados).
    match bind_to_job(app, &child) {
        Ok(()) => {
            let _ = app.emit("fg-log", "[ui] FG bound to job (kill-on-close)".to_string());
        }
        Err(e) => {
            let _ = app.emit(
                "fg-log",
                format!("[ui] WARN: failed to bind FG to job ({}); it may be orphaned.", e),
            );
        }
    }

    // Guardar el hijo en el slot Y avanzar el epoch bajo el MISMO lock. Esto es la CLAVE del
    // guard de epoch: hacerlo atómico impide que un lector viejo (de un restart) observe el
    // estado intermedio "slot = hijo NUEVO, epoch = todavía el viejo" y cosechara por error al
    // hijo nuevo. `fetch_add` devuelve el valor PREVIO, así que el epoch de ESTE hijo = prev+1.
    let my_epoch = {
        let mut guard = state.child.lock().map_err(|e| e.to_string())?;
        *guard = Some(child);
        state.epoch.fetch_add(1, Ordering::SeqCst) + 1
    };

    // stderr reader thread — just forwards lines to the log.
    // FIX C — lectura orientada a bytes: `read_until(b'\n')` no termina el bucle ante un byte
    // no-UTF8 (al contrario que `lines()`, que devolvía `Err` y cortaba el stream). Los bytes
    // inválidos se convierten en U+FFFD vía `from_utf8_lossy`; solo se corta en EOF real (Ok(0))
    // o en un error de E/S real.
    if let Some(err) = stderr {
        let app_e = app.clone();
        let obs_e = obs.clone();
        std::thread::spawn(move || {
            let mut reader = BufReader::new(err);
            let mut buf: Vec<u8> = Vec::new();
            loop {
                buf.clear();
                match reader.read_until(b'\n', &mut buf) {
                    Ok(0) => break, // EOF real
                    Ok(_) => {
                        // Recortar el '\n'/'\r' final; los bytes inválidos pasan a U+FFFD.
                        while matches!(buf.last(), Some(b'\n') | Some(b'\r')) {
                            buf.pop();
                        }
                        let line = String::from_utf8_lossy(&buf).into_owned();
                        if let Some(o) = &obs_e {
                            o.write("fg-err", &line);
                        }
                        let _ = app_e.emit("fg-log", line);
                    }
                    Err(_) => break, // error de E/S real
                }
            }
        });
    }

    // stdout reader thread — forwards lines AND, on EOF (process exiting), reaps the child
    // and emits fg-exit. The FG sets setvbuf(stdout, _IONBF), so lines stream in real time.
    if let Some(out) = stdout {
        let app_o = app.clone();
        let store = state.child.clone();
        let epoch = state.epoch.clone();
        let obs_o = obs.clone();
        std::thread::spawn(move || {
            // FIX C — misma lectura tolerante a UTF-8 inválido que en stderr. SOLO el EOF real
            // (Ok(0)) o un error de E/S real terminan el bucle y disparan el reap + fg-exit; un
            // byte no-UTF8 ya NO provoca un "fg-exit" falso con el FG todavía vivo.
            let mut reader = BufReader::new(out);
            let mut buf: Vec<u8> = Vec::new();
            loop {
                buf.clear();
                match reader.read_until(b'\n', &mut buf) {
                    Ok(0) => break, // EOF real => el proceso se ha ido
                    Ok(_) => {
                        while matches!(buf.last(), Some(b'\n') | Some(b'\r')) {
                            buf.pop();
                        }
                        let line = String::from_utf8_lossy(&buf).into_owned();
                        if let Some(o) = &obs_o {
                            o.write("fg", &line);
                        }
                        let _ = app_o.emit("fg-log", line);
                    }
                    Err(_) => break, // error de E/S real
                }
            }
            // stdout cerrado => el proceso se ha ido. GUARD DE EPOCH: comprobar el epoch BAJO el
            // lock del slot. Solo el lector del hijo VIGENTE cosecha (take + wait) y reporta la
            // salida. Si un `restart()` ya avanzó el epoch (colocó un hijo nuevo), este lector
            // —que todavía drenaba al hijo MUERTO— ve un epoch desfasado, deja el slot + el hijo
            // nuevo intactos y sale en silencio (NO emite `fg-exit`), de modo que NUNCA cosecha
            // al hijo NUEVO. (stop() puede haber tomado+matado ya el hijo vigente; entonces el
            // slot está vacío pero el epoch coincide y reportamos la salida igualmente.)
            let mut emit_exit = false;
            let mut code: Option<i32> = None;
            if let Ok(mut g) = store.lock() {
                if epoch.load(Ordering::SeqCst) == my_epoch {
                    if let Some(mut ch) = g.take() {
                        code = ch.wait().ok().and_then(|s| s.code());
                    }
                    emit_exit = true;
                }
            }
            if emit_exit {
                if let Some(o) = &obs_o {
                    o.write("exit", &format!("code={:?} epoch={}", code, my_epoch));
                }
                // L-2 / E-7: the epoch travels WITH the exit so the UI's restart guard can be
                // child-scoped instead of time-scoped — a NEW child that dies in tens of ms
                // (bad flag → main.cpp:208 exit 2) is no longer mistaken for the old child's
                // death and swallowed, which is what wedged the launcher at "running".
                let _ = app_o.emit("fg-exit", FgExit { code, epoch: my_epoch });
            }
        });
    }

    Ok(my_epoch)
}

/// Spawn the FG with `args`, pipe stdout+stderr, stream each line as a `fg-log` event,
/// and on process exit emit `fg-exit` with `{code, epoch}`. Rechaza un segundo
/// lanzamiento si ya hay uno vivo; el spawn real lo hace el helper compartido `spawn_fg`.
///
/// Devuelve el EPOCH del hijo lanzado (L-2 / E-7): el frontend lo guarda para poder distinguir,
/// en un `fg-exit`, al hijo que acaba de crear del que acaba de matar.
#[tauri::command]
fn launch(
    app: AppHandle,
    state: State<'_, FgState>,
    args: Vec<String>,
    exe_path: Option<String>,
) -> Result<u64, String> {
    // Reject a second launch while one is live.
    {
        let guard = state.child.lock().map_err(|e| e.to_string())?;
        if guard.is_some() {
            return Err("A PhyriadFG process is already running.".into());
        }
    }

    // MULTI-FG (DESIGN ONLY — DO NOT IMPLEMENT HERE): for parallel multi-instance frame-gen
    // we would loop over N targets and spawn one child per target, each receiving its own
    // `--window <title>` (plus a distinct `--monitor`/`--present-monitor` as needed). The args
    // here would become a per-target arg list, the reaper threads would tag their `fg-log`/
    // `fg-exit` events with a child id, and `FgState.child` would become a list of children
    // (see the note on `FgState`). Today this launcher drives exactly one child.
    spawn_fg(&app, state.inner(), args, exe_path)
}

/// Reinicia el FG con `args` nuevos. El FG lee su Config SOLO al arrancar, así que la única
/// forma de aplicar flags nuevos = matar al hijo actual y relanzarlo (esto es del lado del
/// LAUNCHER; NO live-togglea nada dentro del FG). La usa el frontend cuando el usuario cambia
/// un control con "auto-reiniciar" activado y hay un proceso vivo.
///
/// Carrera del reaper: el lector de stdout del hijo VIEJO sigue vivo drenándolo; cuando el
/// hijo muere y llega a EOF, el GUARD DE EPOCH (ver `spawn_fg` / `FgState::epoch`) hace que vea
/// su epoch desfasado y salga sin tocar el slot — así NO cosecha al hijo NUEVO. Queda una
/// ventana mínima en la que ese lector podría emitir un `fg-exit` espurio si gana el lock del
/// slot DESPUÉS de que aquí tomemos el hijo viejo (slot vacío) pero ANTES de que `spawn_fg`
/// coloque al nuevo + avance el epoch; el frontend lo absorbe con su guard `restarting`.
#[tauri::command]
fn restart(
    app: AppHandle,
    state: State<'_, FgState>,
    args: Vec<String>,
    exe_path: Option<String>,
) -> Result<u64, String> {
    if let Some(o) = observer(&app) {
        o.write("ui", "restart requested (config change)");
    }

    // L-6 — VALIDAR ANTES DE DESTRUIR. Antes se mataba al hijo vivo y solo DESPUÉS se intentaba
    // el spawn, así que una ruta a medio teclear (el debounce de 1 s dispara con el campo a
    // medias) dejaba al operador SIN NADA corriendo. El chequeo es barato y ataca justo ese caso.
    // No es un spawn: una ruta que existe todavía puede fallar al lanzarse, y ese fallo se sigue
    // reportando abajo — pero entonces ya no se ha tirado una sesión buena por una ruta inválida.
    let exe = resolve_exe(exe_path.clone());
    if let Err(e) = validate_exe(&exe) {
        if let Some(o) = observer(&app) {
            o.write("error", &format!("restart aborted: {}", e));
        }
        let _ = app.emit(
            "fg-log",
            format!("[ui] restart aborted: {} — the running FG was left untouched.", e),
        );
        return Err(e);
    }

    // Quitar y matar al hijo actual (si lo hay). Ignoramos errores de kill/wait (puede haber
    // muerto solo). El lector de ese hijo seguirá drenándolo hasta EOF; el guard de epoch lo
    // hará salir sin cosechar nada nuevo.
    // C-7: aquí se mata DURO a propósito. El reinicio existe para aplicar flags nuevos rápido, y
    // el hijo nuevo va a truncar el mismo CSV de todas formas (C-8), así que esperar un cierre
    // limpio solo añadiría latencia a cada reinicio automático. El cierre limpio vive en `stop`.
    let old = {
        let mut guard = state.child.lock().map_err(|e| e.to_string())?;
        guard.take()
    };
    if let Some(mut ch) = old {
        let _ = ch.kill();
        let _ = ch.wait();
    }

    // Relanzar EXACTAMENTE como `launch`: spawn + atar al job + guardar + epoch nuevo + lectores.
    let my_epoch = spawn_fg(&app, state.inner(), args, exe_path)?;

    let _ = app.emit(
        "fg-log",
        "[ui] config changed → FG restarted with the new config".to_string(),
    );
    Ok(my_epoch)
}

/// Stop the running FG (if any). The stdout reader thread will then hit EOF and emit fg-exit.
///
/// C-7 — TWO-STAGE stop. First ask for a CLEAN exit (CTRL_BREAK → the FG's
/// `console_ctrl_handler` sets `g_quit` → the main loop joins the workers and the telemetry
/// destructor finalizes `run.csv` and writes `-stats.csv`). Only if that cannot be delivered, or
/// the FG does not exit inside `GRACEFUL_STOP_MS`, fall back to `kill()` — i.e. to exactly the
/// old behaviour, so this can only ever add a clean exit, never remove a working one. Which
/// branch ran is printed, so the operator can see whether the clean path works on his machine.
#[tauri::command]
fn stop(app: AppHandle, state: State<'_, FgState>) -> Result<(), String> {
    /// Budget for the FG's teardown. Sized above the 2 s `vk_wait_live` abandon deadline in
    /// src/core/globals.cpp so a stuck fence cannot make us give up before the FG does.
    const GRACEFUL_STOP_MS: u64 = 4000;

    if let Some(o) = observer(&app) {
        o.write("ui", "stop requested");
    }
    let child = {
        let mut guard = state.child.lock().map_err(|e| e.to_string())?;
        guard.take()
    };
    if let Some(mut ch) = child {
        match try_graceful_stop(&mut ch, Duration::from_millis(GRACEFUL_STOP_MS)) {
            Ok(()) => {
                if let Some(o) = observer(&app) {
                    o.write("ui", "stop: clean shutdown (CTRL_BREAK)");
                }
                let _ = app.emit(
                    "fg-log",
                    "[ui] stop: clean shutdown (CTRL_BREAK) — telemetry finalized".to_string(),
                );
            }
            Err(e) => {
                if let Some(o) = observer(&app) {
                    o.write("ui", &format!("stop: CTRL_BREAK {} -- hard kill", e));
                }
                let _ = app.emit(
                    "fg-log",
                    format!("[ui] stop: CTRL_BREAK {} — hard kill (no -stats.csv)", e),
                );
                let _ = ch.kill();
                let _ = ch.wait();
            }
        }
    }
    Ok(())
}

/// Nota del operador → el observer log + el console de la UI. El canal "ojo → intérprete":
/// el operador marca el instante de lo que VE ("aquí vibra") y la marca queda timestampeada
/// en el mismo stream que la telemetría del FG, para correlarla después.
#[tauri::command]
fn observer_note(app: AppHandle, note: String) -> Result<(), String> {
    let text = note.trim().to_string();
    if text.is_empty() {
        return Ok(());
    }
    if let Some(o) = observer(&app) {
        o.write("operator", &text);
    }
    let _ = app.emit("fg-log", format!("[nota] {}", text));
    Ok(())
}

/// Ruta absoluta del observer log (para mostrarla en la UI y que el observador sepa qué tailear).
#[tauri::command]
fn observer_path(app: AppHandle) -> String {
    observer(&app).map(|o| o.path.clone()).unwrap_or_default()
}

/// True while a child is stored. The reaper thread clears it on exit.
#[tauri::command]
fn is_running(state: State<'_, FgState>) -> bool {
    state
        .child
        .lock()
        .map(|g| g.is_some())
        .unwrap_or(false)
}

/// Run the FG with `--list-monitors`, capture+return stdout. Capped with a short timeout
/// so a hung probe never wedges the UI.
#[tauri::command]
fn list_monitors(app: AppHandle, exe_path: Option<String>) -> Result<String, String> {
    let exe = resolve_exe(exe_path);
    let mut cmd = Command::new(&exe);
    cmd.arg("--list-monitors")
        .stdout(Stdio::piped())
        .stderr(Stdio::null());
    no_window(&mut cmd);

    let mut child = cmd
        .spawn()
        .map_err(|e| format!("Failed to run '{}': {}", exe, e))?;
    // C-9 — la sonda también entra en el Job Object: si se cuelga dentro del driver y ui.exe
    // muere, el kill-on-close se la lleva en vez de dejar un huérfano.
    if let Err(e) = bind_to_job(&app, &child) {
        let _ = app.emit(
            "fg-log",
            format!("[ui] WARN: --list-monitors probe not bound to job ({}).", e),
        );
    }

    let stdout = child
        .stdout
        .take()
        .ok_or_else(|| "No stdout from process".to_string())?;

    let (tx, rx) = std::sync::mpsc::channel::<String>();
    std::thread::spawn(move || {
        let mut s = String::new();
        let mut reader = BufReader::new(stdout);
        let _ = reader.read_to_string(&mut s);
        let _ = tx.send(s);
    });

    match rx.recv_timeout(Duration::from_secs(6)) {
        Ok(s) => {
            let _ = child.wait();
            Ok(s)
        }
        Err(_) => {
            let _ = child.kill();
            let _ = child.wait();
            Err("--list-monitors timed out (6 s).".into())
        }
    }
}

/// R0 (the layer registry): run the FG with `--layer-model-json` and return its stdout. The UI
/// renders its LAYERS section from this model — the binary is the single source of the layer
/// flags (name, default, range, help); no layer literal lives in main.js. Same guard as
/// `list_monitors` (piped stdout, 6 s timeout, kill on hang).
#[tauri::command]
fn layer_model(app: AppHandle, exe_path: Option<String>) -> Result<String, String> {
    let exe = resolve_exe(exe_path);
    let mut cmd = Command::new(&exe);
    cmd.arg("--layer-model-json")
        .stdout(Stdio::piped())
        .stderr(Stdio::null());
    no_window(&mut cmd);
    let mut child = cmd
        .spawn()
        .map_err(|e| format!("Failed to run '{}': {}", exe, e))?;
    // C-9 — misma atadura al job que la sonda de --list-monitors.
    if let Err(e) = bind_to_job(&app, &child) {
        let _ = app.emit(
            "fg-log",
            format!("[ui] WARN: --layer-model-json probe not bound to job ({}).", e),
        );
    }
    let stdout = child
        .stdout
        .take()
        .ok_or_else(|| "No stdout from process".to_string())?;
    let (tx, rx) = std::sync::mpsc::channel::<String>();
    std::thread::spawn(move || {
        let mut s = String::new();
        let mut reader = BufReader::new(stdout);
        let _ = reader.read_to_string(&mut s);
        let _ = tx.send(s);
    });
    match rx.recv_timeout(Duration::from_secs(6)) {
        Ok(s) => {
            let _ = child.wait();
            Ok(s)
        }
        Err(_) => {
            let _ = child.kill();
            let _ = child.wait();
            Err("--layer-model-json timed out (6 s).".into())
        }
    }
}

/// Enumerate visible top-level windows that carry a non-empty title, resolving each to its
/// owning executable's file name. Used by the UI's target-window selector to fill `--window`.
///
/// Filtering: invisible windows are skipped (`IsWindowVisible`), as are empty/whitespace titles,
/// the shell's "Program Manager", and OUR OWN windows. That last filter is now three-layered
/// (I-7 / L-7): the exe-name test compared against "PhyriadFG.exe" while the binary is
/// `phyriad_fg.exe`, so it never matched anything and the FG's own present overlay was offered as
/// a capture target. Corrected literal, PLUS a pid test (the launcher's own pid and the currently
/// spawned child's — `FgState` holds the `Child`), PLUS a window-class test for
/// `phyriad_present_overlay`, which is the only one of the three that survives the operator
/// renaming the executable or running the FG outside this launcher.
///
/// Each row carries `hwnd` (the stable key — captions collide, L-4) and `iconic` (a minimized
/// target makes WGC size its pool to the whole monitor, B-3). Neither is filtered here: the UI
/// decides how to present them. Sorted by (exe, title, hwnd) for a stable order; no de-duplication
/// — EnumWindows never yields the same handle twice, and the old caption-keyed `dedup_by` was
/// silently DROPPING real, distinct windows that happened to share a title (L-4).
#[cfg(windows)]
#[tauri::command]
fn list_windows(state: State<'_, FgState>) -> Result<Vec<WindowInfo>, String> {
    use windows::core::{BOOL, PWSTR};
    use windows::Win32::Foundation::{CloseHandle, HWND, LPARAM, MAX_PATH, TRUE};
    use windows::Win32::System::Threading::{
        OpenProcess, QueryFullProcessImageNameW, PROCESS_NAME_WIN32,
        PROCESS_QUERY_LIMITED_INFORMATION,
    };
    use windows::Win32::UI::WindowsAndMessaging::{
        EnumWindows, GetClassNameW, GetWindowTextLengthW, GetWindowTextW,
        GetWindowThreadProcessId, IsIconic, IsWindowVisible,
    };

    /// What the EnumWindows callback carries through `lparam`: the accumulator plus the two pids
    /// that identify "us" (I-7 / L-7).
    struct EnumCtx {
        out: Vec<WindowInfo>,
        self_pid: u32,
        child_pid: Option<u32>,
    }

    // EnumWindows callback: pushes each qualifying window into the ctx passed via `lparam`.
    // Returning TRUE keeps the enumeration going.
    unsafe extern "system" fn enum_proc(hwnd: HWND, lparam: LPARAM) -> BOOL {
        let ctx = &mut *(lparam.0 as *mut EnumCtx);

        if !IsWindowVisible(hwnd).as_bool() {
            return TRUE;
        }

        // L-7 — the FG's present overlay passes every other filter: PresentSurface passes its
        // class name as the window TEXT too, so it has a non-empty caption, and it is shown with
        // SW_SHOWNOACTIVATE so it is visible. Match on the class prefix, which holds even when
        // the exe has been renamed or the FG was not started by this launcher.
        {
            let mut cbuf = [0u16; 128];
            let clen = GetClassNameW(hwnd, &mut cbuf);
            if clen > 0 {
                let cls = String::from_utf16_lossy(&cbuf[..clen as usize]);
                if cls.starts_with("phyriad_present_overlay") {
                    return TRUE;
                }
            }
        }

        // Title (skip windows with no caption).
        let len = GetWindowTextLengthW(hwnd);
        if len <= 0 {
            return TRUE;
        }
        let mut tbuf = vec![0u16; (len + 1) as usize];
        let copied = GetWindowTextW(hwnd, &mut tbuf);
        if copied <= 0 {
            return TRUE;
        }
        let title = String::from_utf16_lossy(&tbuf[..copied as usize])
            .trim()
            .to_string();
        if title.is_empty() || title == "Program Manager" {
            return TRUE;
        }

        // Owning process -> executable file name.
        let mut pid: u32 = 0;
        GetWindowThreadProcessId(hwnd, Some(&mut pid));
        if pid == 0 {
            return TRUE;
        }

        let mut exe = String::new();
        if let Ok(handle) = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, pid) {
            let mut pbuf = vec![0u16; MAX_PATH as usize];
            let mut size = pbuf.len() as u32;
            if QueryFullProcessImageNameW(handle, PROCESS_NAME_WIN32, PWSTR(pbuf.as_mut_ptr()), &mut size)
                .is_ok()
            {
                let full = String::from_utf16_lossy(&pbuf[..size as usize]);
                exe = full
                    .rsplit(|c| c == '\\' || c == '/')
                    .next()
                    .unwrap_or(&full)
                    .to_string();
            }
            let _ = CloseHandle(handle);
        }

        // I-7 — skip OUR OWN windows. By pid first (exact, rename-proof: the launcher itself and
        // the FG child this launcher spawned), then by exe name for an FG started outside the
        // launcher. The old literal was "PhyriadFG.exe"; the binary is `phyriad_fg.exe`
        // (CMakeLists.txt `add_executable(phyriad_fg`), and the underscore defeated
        // eq_ignore_ascii_case, so the whole test was dead.
        if pid == ctx.self_pid || Some(pid) == ctx.child_pid {
            return TRUE;
        }
        if exe.eq_ignore_ascii_case("ui.exe") || exe.eq_ignore_ascii_case("phyriad_fg.exe") {
            return TRUE;
        }

        ctx.out.push(WindowInfo {
            title,
            exe,
            pid,
            hwnd: hwnd.0 as usize as u64,
            // B-3: reported, not filtered. GetClientRect on a minimized window is degenerate, and
            // capture_init's size guard has no else branch, so the run silently captures the
            // whole monitor. The UI must not let one be selected without saying so.
            iconic: IsIconic(hwnd).as_bool(),
        });
        TRUE
    }

    // The live child's pid, if any — read once, before the enumeration (I-7). The lock is
    // released immediately; a failure to lock degrades to "no child pid known", which just means
    // the exe-name filter carries the case alone.
    let child_pid: Option<u32> = state
        .child
        .lock()
        .ok()
        .and_then(|g| g.as_ref().map(|c| c.id()));

    let mut ctx = EnumCtx {
        out: Vec::new(),
        self_pid: std::process::id(),
        child_pid,
    };
    unsafe {
        EnumWindows(Some(enum_proc), LPARAM(&mut ctx as *mut _ as isize))
            .map_err(|e| e.to_string())?;
    }
    let mut found = ctx.out;

    // L-4 — sort for a stable list, but do NOT de-duplicate. The old `dedup_by` keyed on
    // (title, exe) and therefore DELETED the second of two genuinely distinct windows that share
    // a caption — exactly the case the operator needs to tell apart. `hwnd` is the tie-break so
    // the order is deterministic across refreshes; the UI disambiguates the label with the pid.
    found.sort_by(|a, b| {
        a.exe
            .to_lowercase()
            .cmp(&b.exe.to_lowercase())
            .then_with(|| a.title.to_lowercase().cmp(&b.title.to_lowercase()))
            .then_with(|| a.hwnd.cmp(&b.hwnd))
    });

    Ok(found)
}

/// Non-Windows fallback: no window enumeration available. Takes the same `State` as the Windows
/// version so the invoke signature the frontend sees does not depend on the target.
#[cfg(not(windows))]
#[tauri::command]
fn list_windows(_state: State<'_, FgState>) -> Result<Vec<WindowInfo>, String> {
    Ok(Vec::new())
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    let builder = tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .manage(FgState {
            child: Arc::new(Mutex::new(None)),
            epoch: Arc::new(AtomicU64::new(0)),
        })
        .manage(ObserverLog::create());

    // FIX A — crear el ÚNICO Job Object al arrancar y conservar su handle en estado gestionado
    // durante toda la vida de la app (windows-only). Que viva aquí garantiza la semántica de
    // "el último handle se cierra al morir el proceso" → kill-on-close de los hijos.
    #[cfg(windows)]
    let builder = builder.manage(create_job());

    builder
        .invoke_handler(tauri::generate_handler![
            launch,
            restart,
            stop,
            is_running,
            list_monitors,
            layer_model,
            list_windows,
            observer_note,
            observer_path
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}

// Made with my soul - Swately <3
