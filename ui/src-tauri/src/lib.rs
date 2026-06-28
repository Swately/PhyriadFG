// PhyriadFG launcher — Tauri 2 backend.
//
// This is a LAUNCHER ONLY: it assembles CLI flags, spawns phyriad_fg.exe, streams its
// stdout/stderr line-by-line to the frontend (live status), and can stop the process.
// It NEVER writes into the running FG (the FG's Config is startup-set + read unsynchronized);
// all flags apply at launch.

use std::io::{BufRead, BufReader, Read};
use std::process::{Child, Command, Stdio};
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Arc, Mutex};
use std::time::Duration;

use tauri::{AppHandle, Emitter, State};

/// Default location of the frame-gen executable (overridable from the UI via `exe_path`).
const DEFAULT_EXE: &str = "G:\\PhyriadFG\\build\\phyriad_fg.exe";

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

/// One enumerated top-level window, surfaced to the UI's target-window selector.
#[derive(serde::Serialize, Clone)]
struct WindowInfo {
    /// Visible window title (the substring the FG's `--window` flag matches against).
    title: String,
    /// File name of the owning process's executable (e.g. "bf6.exe").
    exe: String,
    /// Owning process id.
    pid: u32,
}

fn resolve_exe(exe_path: Option<String>) -> String {
    match exe_path {
        Some(p) if !p.trim().is_empty() => p,
        _ => DEFAULT_EXE.to_string(),
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
) -> Result<(), String> {
    let exe = resolve_exe(exe_path);
    let mut cmd = Command::new(&exe);
    cmd.args(&args).stdout(Stdio::piped()).stderr(Stdio::piped());
    no_window(&mut cmd);

    let mut child = cmd
        .spawn()
        .map_err(|e| format!("Failed to start '{}': {}", exe, e))?;

    let stdout = child.stdout.take();
    let stderr = child.stderr.take();

    // FIX A — atar el hijo al Job Object (kill-on-close) mientras todavía tenemos el `child`.
    // El handle crudo sigue siendo válido tras moverlo a FgState, pero lo hacemos aquí, antes
    // de almacenarlo, para no tener que volver a pedir prestado el child. Si falla, avisamos
    // pero NO abortamos el lanzamiento (degradación: el hijo podría quedar huérfano).
    // Asignar tras el spawn funciona en Win8+ (se permiten jobs anidados).
    #[cfg(windows)]
    {
        use std::os::windows::io::AsRawHandle;
        use tauri::Manager;
        use windows::Win32::Foundation::HANDLE;
        use windows::Win32::System::JobObjects::AssignProcessToJobObject;
        if let Some(job) = app.try_state::<JobState>() {
            let res = unsafe {
                AssignProcessToJobObject(
                    HANDLE(job.handle as _),
                    HANDLE(child.as_raw_handle() as _),
                )
            };
            match res {
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
                let _ = app_o.emit("fg-exit", code);
            }
        });
    }

    Ok(())
}

/// Spawn the FG with `args`, pipe stdout+stderr, stream each line as a `fg-log` event,
/// and on process exit emit `fg-exit` with the exit code (or null). Rechaza un segundo
/// lanzamiento si ya hay uno vivo; el spawn real lo hace el helper compartido `spawn_fg`.
#[tauri::command]
fn launch(
    app: AppHandle,
    state: State<'_, FgState>,
    args: Vec<String>,
    exe_path: Option<String>,
) -> Result<(), String> {
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
) -> Result<(), String> {
    // Quitar y matar al hijo actual (si lo hay). Ignoramos errores de kill/wait (puede haber
    // muerto solo). El lector de ese hijo seguirá drenándolo hasta EOF; el guard de epoch lo
    // hará salir sin cosechar nada nuevo.
    let old = {
        let mut guard = state.child.lock().map_err(|e| e.to_string())?;
        guard.take()
    };
    if let Some(mut ch) = old {
        let _ = ch.kill();
        let _ = ch.wait();
    }

    // Relanzar EXACTAMENTE como `launch`: spawn + atar al job + guardar + epoch nuevo + lectores.
    spawn_fg(&app, state.inner(), args, exe_path)?;

    let _ = app.emit(
        "fg-log",
        "[ui] config changed → FG restarted with the new config".to_string(),
    );
    Ok(())
}

/// Kill the running FG (if any). The stdout reader thread will then hit EOF and emit fg-exit.
#[tauri::command]
fn stop(state: State<'_, FgState>) -> Result<(), String> {
    let child = {
        let mut guard = state.child.lock().map_err(|e| e.to_string())?;
        guard.take()
    };
    if let Some(mut ch) = child {
        let _ = ch.kill();
        let _ = ch.wait();
    }
    Ok(())
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
fn list_monitors(exe_path: Option<String>) -> Result<String, String> {
    let exe = resolve_exe(exe_path);
    let mut cmd = Command::new(&exe);
    cmd.arg("--list-monitors")
        .stdout(Stdio::piped())
        .stderr(Stdio::null());
    no_window(&mut cmd);

    let mut child = cmd
        .spawn()
        .map_err(|e| format!("Failed to run '{}': {}", exe, e))?;

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

/// Enumerate visible top-level windows that carry a non-empty title, resolving each to its
/// owning executable's file name. Used by the UI's target-window selector to fill `--window`.
///
/// Filtering: invisible windows are skipped (`IsWindowVisible`), as are empty/whitespace
/// titles, the shell's "Program Manager", and this launcher's own window (ui.exe / PhyriadFG.exe).
/// Results are de-duplicated and sorted (by exe, then title) for a stable list.
#[cfg(windows)]
#[tauri::command]
fn list_windows() -> Result<Vec<WindowInfo>, String> {
    use windows::core::{BOOL, PWSTR};
    use windows::Win32::Foundation::{CloseHandle, HWND, LPARAM, MAX_PATH, TRUE};
    use windows::Win32::System::Threading::{
        OpenProcess, QueryFullProcessImageNameW, PROCESS_NAME_WIN32,
        PROCESS_QUERY_LIMITED_INFORMATION,
    };
    use windows::Win32::UI::WindowsAndMessaging::{
        EnumWindows, GetWindowTextLengthW, GetWindowTextW, GetWindowThreadProcessId,
        IsWindowVisible,
    };

    // EnumWindows callback: pushes each qualifying window into the Vec passed via `lparam`.
    // Returning TRUE keeps the enumeration going.
    unsafe extern "system" fn enum_proc(hwnd: HWND, lparam: LPARAM) -> BOOL {
        let out = &mut *(lparam.0 as *mut Vec<WindowInfo>);

        if !IsWindowVisible(hwnd).as_bool() {
            return TRUE;
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

        // Skip our own launcher window.
        if exe.eq_ignore_ascii_case("ui.exe") || exe.eq_ignore_ascii_case("PhyriadFG.exe") {
            return TRUE;
        }

        out.push(WindowInfo { title, exe, pid });
        TRUE
    }

    let mut found: Vec<WindowInfo> = Vec::new();
    unsafe {
        EnumWindows(Some(enum_proc), LPARAM(&mut found as *mut _ as isize))
            .map_err(|e| e.to_string())?;
    }

    found.sort_by(|a, b| {
        a.exe
            .to_lowercase()
            .cmp(&b.exe.to_lowercase())
            .then_with(|| a.title.to_lowercase().cmp(&b.title.to_lowercase()))
    });
    found.dedup_by(|a, b| a.title == b.title && a.exe == b.exe);

    Ok(found)
}

/// Non-Windows fallback: no window enumeration available.
#[cfg(not(windows))]
#[tauri::command]
fn list_windows() -> Result<Vec<WindowInfo>, String> {
    Ok(Vec::new())
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    let builder = tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .manage(FgState {
            child: Arc::new(Mutex::new(None)),
            epoch: Arc::new(AtomicU64::new(0)),
        });

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
            list_windows
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}

// Made with my soul - Swately <3
