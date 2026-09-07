// build.rs — tauri's own build step, plus the embedded frame-generator payload.
//
// WHY THE FG IS EMBEDDED. The launcher and the FG are two PROCESSES on purpose: the launcher spawns
// the FG, streams its stdout, ties it to a kill-on-close Job Object, and kills/respawns it on a
// config change. The whole epoch-scoped restart contract lives in that separation, so merging them
// into one process is not on the table. Shipping TWO FILES to a user is a different question, and it
// has a different answer: carry the FG as a payload and write it out on first run. One download,
// still two processes.
//
// WHAT THIS DOES. Copies the release FG binary into OUT_DIR so `include_bytes!` can reach it at a
// stable path, and emits its FNV-1a hash so the extracted copy can be named by CONTENT — a new build
// lands beside the old one instead of racing to overwrite a file that may be running.
//
// WHEN THE BINARY IS ABSENT the payload is EMPTY and the launcher simply falls back to finding
// phyriad_fg.exe on disk, exactly as it did before. `cargo build` in a tree that has never built the
// C++ must not fail — a developer building only the launcher is a normal thing to do.
//
// Made with my soul - Swately <3
use std::path::{Path, PathBuf};

fn fnv1a64(bytes: &[u8]) -> u64 {
    let mut h: u64 = 0xcbf2_9ce4_8422_2325;
    for b in bytes {
        h ^= *b as u64;
        h = h.wrapping_mul(0x0000_0100_0000_01b3);
    }
    h
}

fn main() {
    let manifest = PathBuf::from(std::env::var("CARGO_MANIFEST_DIR").unwrap());
    let root = manifest.join("..").join("..");

    // Release first: that is what ships. The debug build is accepted as a fallback so a developer
    // who has only run build.bat still gets a self-contained launcher, and the console line below
    // says which one went in — a launcher that silently embedded a debug FG would be a trap.
    let candidates = [
        root.join("build-release").join("phyriad_fg.exe"),
        root.join("build").join("phyriad_fg.exe"),
    ];
    for c in &candidates {
        println!("cargo:rerun-if-changed={}", c.display());
    }

    let out = PathBuf::from(std::env::var("OUT_DIR").unwrap()).join("phyriad_fg_embedded.bin");
    let picked: Option<&Path> = candidates.iter().map(|p| p.as_path()).find(|p| p.is_file());

    let (bytes, note) = match picked {
        Some(p) => match std::fs::read(p) {
            // NO PATH IN THIS STRING. A cargo:warning value is truncated at a colon, so a Windows
            // path silently cut this whole line down to nothing — and a build diagnostic that says
            // nothing is worse than no diagnostic, because it reads as "checked, fine". What a
            // reader actually needs is the byte count (did a payload go in?) and WHICH build won,
            // since a launcher that quietly shipped a debug FG would be a genuine trap. Both fit
            // without a colon.
            Ok(b) => {
                let n = b.len();
                let which = if p.starts_with(root.join("build-release")) { "build-release" } else { "build (DEBUG)" };
                (b, format!("{n} bytes from {which}"))
            }
            Err(e) => (Vec::new(), format!("UNREADABLE {}: {e}", p.display())),
        },
        None => (Vec::new(), "ABSENT — the launcher will look for phyriad_fg.exe on disk".to_string()),
    };

    // The hash is over the payload, so an empty payload hashes to the FNV offset basis and the
    // runtime's `is_empty()` check — not the hash — is what decides whether extraction happens.
    println!("cargo:rustc-env=PFG_EMBEDDED_HASH={:016x}", fnv1a64(&bytes));
    println!("cargo:warning=embedded frame generator: {note}");
    std::fs::write(&out, &bytes).expect("write embedded FG payload");

    tauri_build::build()
}
