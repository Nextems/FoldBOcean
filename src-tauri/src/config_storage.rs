use std::fs::{self, File, OpenOptions};
use std::io::Write;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicU64, Ordering};

static SERIAL: AtomicU64 = AtomicU64::new(0);
struct Temporary(PathBuf);
impl Drop for Temporary { fn drop(&mut self) { let _ = fs::remove_file(&self.0); } }

fn lock(path: &Path) -> Result<File, String> {
    let mut options = OpenOptions::new();
    options.write(true).create(true).truncate(false);
    #[cfg(windows)] {
        use std::os::windows::fs::OpenOptionsExt;
        options.share_mode(0);
    }
    let lock_path = path.with_extension("json.lock");
    for attempt in 0..40 {
        match options.open(&lock_path) {
            Ok(file) => return Ok(file),
            Err(error) if error.raw_os_error() == Some(32) && attempt < 39 => std::thread::sleep(std::time::Duration::from_millis(25)),
            Err(error) => return Err(format!("Не удалось заблокировать конфигурацию: {error}")),
        }
    }
    Err("Конфигурация занята другим окном настроек".into())
}
fn write_temporary(path: &Path, bytes: &[u8]) -> Result<Temporary, String> {
    let name = format!("{}.{}.{}.tmp", path.file_name().unwrap().to_string_lossy(), std::process::id(), SERIAL.fetch_add(1, Ordering::Relaxed));
    let temporary = Temporary(path.with_file_name(name));
    let mut file = OpenOptions::new().write(true).create_new(true).open(&temporary.0).map_err(|e| e.to_string())?;
    file.write_all(bytes).and_then(|_| file.sync_all()).map_err(|e| e.to_string())?;
    drop(file);
    Ok(temporary)
}
fn replace(source: &Path, destination: &Path) -> Result<(), String> {
    #[cfg(windows)] {
        use std::os::windows::ffi::OsStrExt;
        #[link(name="kernel32")]
        extern "system" { fn MoveFileExW(source: *const u16, destination: *const u16, flags: u32) -> i32; }
        let from: Vec<u16> = source.as_os_str().encode_wide().chain(Some(0)).collect();
        let to: Vec<u16> = destination.as_os_str().encode_wide().chain(Some(0)).collect();
        if unsafe { MoveFileExW(from.as_ptr(), to.as_ptr(), 1 | 8) } == 0 { return Err(std::io::Error::last_os_error().to_string()); }
        Ok(())
    }
    #[cfg(not(windows))] { fs::rename(source, destination).map_err(|e| e.to_string()) }
}
pub fn save(path: &Path, bytes: &[u8], valid: impl Fn(&serde_json::Value) -> bool) -> Result<(), String> {
    fs::create_dir_all(path.parent().ok_or("Не указана папка конфигурации")?).map_err(|e| e.to_string())?;
    let _lock = lock(path)?;
    let previous = match fs::read(path) {
        Ok(old) => {
            let value = serde_json::from_slice(&old).map_err(|_| "Текущий JSON повреждён. Восстановите config.json.bak, если резервная копия существует.")?;
            if !valid(&value) { return Err("Текущая конфигурация некорректна: восстановите config.json.bak, если резервная копия существует".into()); }
            if old == bytes { return Ok(()); }
            Some(old)
        },
        Err(error) if error.kind() == std::io::ErrorKind::NotFound => None,
        Err(error) => return Err(error.to_string()),
    };
    let next = write_temporary(path, bytes)?;
    if let Some(old) = previous {
        let backup = path.with_extension("json.bak");
        let temporary = write_temporary(&backup, &old)?;
        replace(&temporary.0, &backup)?;
    }
    replace(&next.0, path)
}
