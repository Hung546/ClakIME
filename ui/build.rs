use std::process::Command;

fn main() {
    let hash = Command::new("git")
        .args(["rev-parse", "--short", "HEAD"])
        .output()
        .ok()
        .and_then(|o| String::from_utf8(o.stdout).ok())
        .map(|s| s.trim().to_string())
        .unwrap_or_else(|| "unknown".to_string());
    println!("cargo:rustc-env=GIT_HASH={}", hash);

    let note = Command::new("git")
        .args(["log", "-1", "--format=%s"])
        .output()
        .ok()
        .and_then(|o| String::from_utf8(o.stdout).ok())
        .map(|s| s.trim().to_string())
        .unwrap_or_else(|| "N/A".to_string());
    println!("cargo:rustc-env=GIT_NOTE={}", note);

    let date = Command::new("git")
        .args([
            "log",
            "-1",
            "--format=%cd",
            "--date=format:%d/%m/%Y, %H:%M:%S",
        ])
        .output()
        .ok()
        .and_then(|o| String::from_utf8(o.stdout).ok())
        .map(|s| s.trim().to_string())
        .unwrap_or_else(|| "N/A".to_string());
    println!("cargo:rustc-env=GIT_DATE={}", date);

    slint_build::compile("src/appwindow.slint").unwrap();
}
