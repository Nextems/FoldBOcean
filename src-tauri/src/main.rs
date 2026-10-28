#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]
use tauri::Manager;
mod config_integrity;
mod config_storage;
mod user_presets;

// A process-scoped shutdown signal also works when the settings window is minimized.
#[cfg(windows)]
fn listen_for_shutdown() {
    use std::ffi::c_void;
    #[link(name = "kernel32")]
    extern "system" {
        fn CreateEventW(attributes: *const c_void, manual_reset: i32, initial_state: i32, name: *const u16) -> *mut c_void;
        fn WaitForSingleObject(handle: *mut c_void, milliseconds: u32) -> u32;
        fn CloseHandle(handle: *mut c_void) -> i32;
    }
    let name: Vec<u16> = format!("Local\\SpiderWeb.Settings.Exit.{}", std::process::id()).encode_utf16().chain(Some(0)).collect();
    let event = unsafe { CreateEventW(std::ptr::null(), 0, 0, name.as_ptr()) } as usize;
    if event == 0 { return; }
    std::thread::spawn(move || {
        let result = unsafe { WaitForSingleObject(event as *mut c_void, u32::MAX) };
        unsafe { CloseHandle(event as *mut c_void); }
        if result == 0 { std::process::exit(0); }
    });
}
fn finish_settings(app: &tauri::AppHandle) {
    app.exit(0);
    // Bound WebView/COM teardown: the closed editor must never linger in memory.
    std::thread::spawn(|| {
        std::thread::sleep(std::time::Duration::from_secs(1));
        std::process::exit(0);
    });
}

fn launcher_path() -> Result<std::path::PathBuf, String> {
    let executable = std::env::current_exe().map_err(|e| e.to_string())?;
    let packaged = executable.parent().unwrap().join("FoldBOcean.exe");
    if packaged.exists() { return Ok(packaged); }
    #[cfg(debug_assertions)] {
        let development = std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../release/FoldBOcean.exe");
        if development.exists() { return Ok(development); }
    }
    Err("FoldBOcean.exe не найден. Соберите native/build.ps1 или поместите его рядом с настройками.".into())
}
#[tauri::command]
async fn app_icons() -> Result<serde_json::Value, String> {
    let launcher = launcher_path()?;
    tauri::async_runtime::spawn_blocking(move || {
        let output = std::process::Command::new(launcher).arg("--export-icons").output().map_err(|e|e.to_string())?;
        if !output.status.success() { return Err("Не удалось получить значки приложений".into()); }
        serde_json::from_slice(&output.stdout).map_err(|e|e.to_string())
    }).await.map_err(|e|e.to_string())?
}
#[tauri::command]
async fn app_metadata(path:String)->Result<serde_json::Value,String>{
    if path.trim().is_empty()||path.len()>32767{return Err("Некорректный путь к приложению".into());}
    let launcher=launcher_path()?;
    tauri::async_runtime::spawn_blocking(move || {
        let output=std::process::Command::new(launcher).arg("--app-name").arg(path).output().map_err(|e|e.to_string())?;
        let value:serde_json::Value=serde_json::from_slice(&output.stdout).map_err(|e|e.to_string())?;
        if !output.status.success(){return Err(value["error"].as_str().unwrap_or("Не удалось прочитать название приложения").to_string());}Ok(value)
    }).await.map_err(|e|e.to_string())?
}
#[tauri::command]
async fn launcher_monitors() -> Result<serde_json::Value, String> {
    let launcher = launcher_path()?;
    tauri::async_runtime::spawn_blocking(move || {
        let output = std::process::Command::new(launcher).arg("--export-monitors").output().map_err(|e|e.to_string())?;
        if !output.status.success() { return Err("Не удалось получить список мониторов".into()); }
        serde_json::from_slice(&output.stdout).map_err(|e|e.to_string())
    }).await.map_err(|e|e.to_string())?
}
#[tauri::command]
fn launcher_action(action: String) -> Result<(), String> {
    let argument = match action.as_str() {
        "start" => "--background", "demo" => "--demo", "stop" => "--quit",
        _ => return Err("Неизвестная команда".into()),
    };
    std::process::Command::new(launcher_path()?).arg(argument).spawn().map_err(|e| e.to_string())?;
    Ok(())
}
#[tauri::command]
fn exit_all(app: tauri::AppHandle) -> Result<(), String> {
    launcher_action("stop".into())?;
    finish_settings(&app);
    Ok(())
}

async fn autostart_command(argument: &'static str) -> Result<serde_json::Value,String> {
    let launcher=launcher_path()?;
    tauri::async_runtime::spawn_blocking(move || {
        let output=std::process::Command::new(launcher).arg(argument).output().map_err(|e|e.to_string())?;
        let result:serde_json::Value=serde_json::from_slice(&output.stdout).map_err(|e|e.to_string())?;
        if !output.status.success(){return Err(result["error"].as_str().unwrap_or("Не удалось изменить автозапуск").to_string());}
        Ok(result)
    }).await.map_err(|e|e.to_string())?
}
#[tauri::command]
async fn autostart_status()->Result<serde_json::Value,String>{autostart_command("--autostart-status").await}
#[tauri::command]
async fn set_autostart(enabled:bool)->Result<serde_json::Value,String>{autostart_command(if enabled{"--autostart-on"}else{"--autostart-off"}).await}

fn config_path(app: &tauri::AppHandle) -> Result<std::path::PathBuf, String> {
    Ok(app.path().app_config_dir().map_err(|e| e.to_string())?.join("config.json"))
}
fn presets_path(app:&tauri::AppHandle)->Result<std::path::PathBuf,String>{
    Ok(app.path().app_config_dir().map_err(|e|e.to_string())?.join("presets"))
}
#[tauri::command]
async fn list_presets(app:tauri::AppHandle)->Result<Vec<serde_json::Value>,String>{
    let directory=presets_path(&app)?;
    tauri::async_runtime::spawn_blocking(move||user_presets::list(&directory)).await.map_err(|e|e.to_string())?
}
#[tauri::command]
async fn save_preset(app:tauri::AppHandle,name:String,config:serde_json::Value,id:Option<String>)->Result<serde_json::Value,String>{
    let directory=presets_path(&app)?;
    tauri::async_runtime::spawn_blocking(move||user_presets::save(&directory,&name,config,id.as_deref())).await.map_err(|e|e.to_string())?
}
#[tauri::command]
async fn load_preset(app:tauri::AppHandle,id:String)->Result<serde_json::Value,String>{
    let directory=presets_path(&app)?;
    tauri::async_runtime::spawn_blocking(move||user_presets::load(&directory,&id)).await.map_err(|e|e.to_string())?
}
#[tauri::command]
async fn rename_preset(app:tauri::AppHandle,id:String,name:String)->Result<serde_json::Value,String>{
    let directory=presets_path(&app)?;
    tauri::async_runtime::spawn_blocking(move||user_presets::rename(&directory,&id,&name)).await.map_err(|e|e.to_string())?
}
#[tauri::command]
async fn delete_preset(app:tauri::AppHandle,id:String)->Result<(),String>{
    let directory=presets_path(&app)?;
    tauri::async_runtime::spawn_blocking(move||user_presets::delete(&directory,&id)).await.map_err(|e|e.to_string())?
}
#[tauri::command]
fn load_config(app: tauri::AppHandle) -> Result<Option<serde_json::Value>, String> {
    let path = config_path(&app)?;
    if !path.exists() { return Ok(None); }
    let text = std::fs::read_to_string(path).map_err(|e| e.to_string())?;
    serde_json::from_str(&text).map(Some).map_err(|e| e.to_string())
}
#[tauri::command]
fn save_config(app: tauri::AppHandle, mut config: serde_json::Value) -> Result<(), String> {
    prepare_config(&mut config)?;
    let path = config_path(&app)?;
    let bytes = serde_json::to_vec_pretty(&config).map_err(|e| e.to_string())?;
    config_storage::save(&path, &bytes, |previous| prepare_config(&mut previous.clone()).is_ok())
}
fn prepare_config(config: &mut serde_json::Value) -> Result<(), String> {
    config_integrity::validate_entries(config)?;
    config.as_object_mut().ok_or("Неверный формат конфигурации")?.remove("explorer");
    validate_names(&config)?;
    protect_system_group(config)?;
    validate_positions(&config)?;
    validate_launcher(&config)?;
    validate_radial(&config)?;
    validate_web_style(&config)?;
    validate_rgb(&config)?;
    validate_appearance(&config)?;
    protect_layout(config)?;
    Ok(())
}
fn validate_appearance(config:&serde_json::Value)->Result<(),String>{
    if let Some(settings)=config.get("appearance") {
        if !settings.is_object(){return Err("Некорректный размер меню".into());}
        for (key,min,max) in [("web",50.0,180.0),("radial",50.0,180.0),("ringStep",0.0,12.0),("openMs",100.0,1500.0)]{if let Some(value)=settings.get(key){if !value.as_f64().map(|n|n.is_finite()&&n>=min&&n<=max).unwrap_or(false){return Err("Некорректный размер меню".into());}}}
    } Ok(())
}
fn validate_rgb(config:&serde_json::Value)->Result<(),String>{
    if let Some(rgb)=config.get("rgb") {
        if !rgb.is_object(){return Err("Некорректный режим RGB".into());}
        for key in ["radial","blocks","branches"]{if let Some(value)=rgb.get(key){if !value.is_boolean(){return Err("Некорректный переключатель RGB".into());}}}
        if let Some(speed)=rgb.get("speed"){if !speed.as_f64().map(|n|n.is_finite()&&n>=0.2&&n<=3.0).unwrap_or(false){return Err("Некорректная скорость RGB".into());}}
    } Ok(())
}
fn validate_web_style(config:&serde_json::Value)->Result<(),String>{
    if let Some(style)=config.get("webStyle") {
        if !style.is_object(){return Err("Некорректный вид паутины".into());}
        for (key,max) in [("opacity",100.0),("blur",24.0)]{if let Some(value)=style.get(key){if !value.as_f64().map(|n|n.is_finite()&&n>=0.0&&n<=max).unwrap_or(false){return Err(format!("Некорректный параметр паутины: {key}"));}}}
        if let Some(color)=style.get("color"){if !color.as_str().map(|s|s.len()==7&&s.starts_with('#')&&s[1..].bytes().all(|c|c.is_ascii_hexdigit())).unwrap_or(false){return Err("Некорректный цвет паутины".into());}}
    } Ok(())
}
fn validate_radial(config:&serde_json::Value)->Result<(),String>{
    if let Some(style)=config.get("radial") {
        if let Some(enabled)=style.get("rounded"){if !enabled.is_boolean(){return Err("Некорректный режим скругления".into());}}
        for (key,max) in [("roundness",18.0),("blur",24.0)]{if let Some(value)=style.get(key){if !value.as_f64().map(|n|n.is_finite()&&n>=0.0&&n<=max).unwrap_or(false){return Err(format!("Некорректный параметр {key}"));}}}
        let valid=|v:&serde_json::Value|v.as_str().map(|s|s.len()==7&&s.starts_with('#')&&s[1..].bytes().all(|c|c.is_ascii_hexdigit())).unwrap_or(false);
        if let Some(color)=style.get("color"){if !valid(color){return Err("Некорректный цвет".into());}}
        if let Some(colors)=style.get("colors"){for color in colors.as_object().ok_or("Некорректные цвета")?.values(){if !valid(color){return Err("Некорректный цвет сектора".into());}}}
        if let Some(rotation)=style.get("rotation"){for angle in rotation.as_object().ok_or("Некорректный поворот")?.values(){if !angle.as_f64().map(|n|n.is_finite()&&n.abs()<=std::f64::consts::PI).unwrap_or(false){return Err("Некорректный поворот кольца".into());}}}
    } Ok(())
}
fn validate_launcher(config: &serde_json::Value) -> Result<(), String> {
    let Some(launcher) = config.get("launcher") else { return Ok(()); };
    if let Some(monitor) = launcher.get("monitor") {
        let monitor = monitor.as_str().ok_or("Некорректный монитор")?;
        if monitor.is_empty() || monitor.len()>128 { return Err("Некорректный монитор".into()); }
    }
    if let Some(view)=launcher.get("view"){if !matches!(view.as_str(),Some("radial"|"web")){return Err("Неизвестный вид меню".into());}}
    for (key, min, max) in [("hoverDelayMs",0.0,1500.0),("closeDelayMs",0.0,2000.0),("corridorWidth",8.0,120.0),("branchLength",0.0,800.0)] {
        if let Some(value) = launcher.get(key) {
            let value = value.as_f64().ok_or("Некорректный параметр взаимодействия")?;
            if !value.is_finite() || value < min || value > max { return Err(format!("Параметр {key} вне допустимого диапазона")); }
        }
    }
    if !matches!(launcher.get("mode").unwrap_or(&serde_json::json!("hold")).as_str(), Some("hold" | "toggle")) {
        return Err("Неизвестный режим вызова паутины".into());
    }
    let default_trigger=serde_json::json!({"type":"keyboard","key":192,"modifiers":2,"label":"`"});
    let trigger = launcher.get("trigger").unwrap_or(&default_trigger);
    if !trigger.is_object(){return Err("Некорректный бинд".into());}
    match trigger.get("type").map(|v|v.as_str()).unwrap_or(Some("mouse")) {
        Some("mouse") if matches!(trigger["button"].as_str(), Some("middle" | "x1" | "x2")) => Ok(()),
        Some("keyboard") => {
            let key = trigger["key"].as_u64().ok_or("Не указана клавиша")?;
            let mods = trigger.get("modifiers").map(|v|v.as_u64()).unwrap_or(Some(0)).ok_or("Не указаны модификаторы")?;
            if !(8..=254).contains(&key) || matches!(key, 16..=18 | 27 | 91 | 92) || mods > 7 {
                return Err("Недопустимое сочетание клавиш".into());
            }
            Ok(())
        },
        _ => Err("Неизвестный бинд паутины".into()),
    }
}
fn protect_layout(config: &mut serde_json::Value) -> Result<(), String> {
    let roots:std::collections::HashSet<String>=config["groups"].as_array().ok_or("Некорректные группы")?.iter().filter(|g|g["parentId"].is_null()).filter_map(|g|g["id"].as_str().map(str::to_owned)).collect();
    let mut movable = std::collections::HashSet::new();
    let mut groups=std::collections::HashSet::from(["@root".to_owned()]);
    for collection in ["groups","apps"] {
        for item in config[collection].as_array().ok_or("Некорректная конфигурация")? {
            let id = item["id"].as_str().ok_or("Не указан id элемента")?;
            movable.insert(id.to_owned());
            if collection=="groups" { groups.insert(id.to_owned()); }
        }
    }
    if let Some(layout) = config.get_mut("layout") {
        if let Some(links)=layout.get_mut("rootLinks") {
            let links=links.as_array_mut().ok_or("Некорректные связи основных папок")?;
            for link in links.iter(){let pair=link.as_array().filter(|a|a.len()==2).ok_or("Связь должна содержать две папки")?;let a=pair[0].as_str().ok_or("Некорректная папка связи")?;let b=pair[1].as_str().ok_or("Некорректная папка связи")?;if a==b{return Err("Папку нельзя связать с самой собой".into());}}
            let mut unique=std::collections::HashSet::new();links.retain(|link|{let a=link[0].as_str().unwrap();let b=link[1].as_str().unwrap();roots.contains(a)&&roots.contains(b)&&unique.insert(if a<b{(a.to_owned(),b.to_owned())}else{(b.to_owned(),a.to_owned())})});
        }
        let nodes = layout.get_mut("nodes").and_then(|value| value.as_object_mut()).ok_or("Некорректное расположение паутины")?;
        nodes.retain(|id,_| movable.contains(id));
        for point in nodes.values() {
            for axis in ["x","y"] {
                let value = point[axis].as_f64().ok_or("Некорректные координаты")?;
                if !value.is_finite() || value.abs() > 2400.0 { return Err("Координаты выходят за допустимые границы".into()); }
            }
        }
        if let Some(branches)=layout.get_mut("branches") {
            let branches=branches.as_object_mut().ok_or("Некорректная геометрия ветвей")?;
            branches.retain(|id,_|groups.contains(id));
            for branch in branches.values() {
                if !branch.is_object(){return Err("Некорректная геометрия ветви".into());}
                for (key,min,max) in [("length",0.0,800.0),("radius",40.0,800.0),("angle",-std::f64::consts::PI,std::f64::consts::PI)] {
                    if let Some(value)=branch.get(key) {
                        let value=value.as_f64().ok_or("Некорректный размер ветви")?;
                        if !value.is_finite()||value<min||value>max {return Err("Размер ветви вне допустимого диапазона".into());}
                    }
                }
            }
        }
    }
    Ok(())
}
fn validate_names(config: &serde_json::Value) -> Result<(), String> {
    for key in ["groups","apps"] {
        for item in config[key].as_array().ok_or("Неверный формат конфигурации")? {
            let name=item["name"].as_str().ok_or("Не указано имя")?;
            if name.trim().is_empty() || name.chars().count()>256 { return Err("Имя должно содержать от 1 до 256 символов".into()); }
        }
    }
    Ok(())
}
fn protect_system_group(config: &mut serde_json::Value) -> Result<(), String> {
    let presets: serde_json::Value = serde_json::from_str(include_str!("../../src/system-presets.json")).map_err(|e| e.to_string())?;
    let names: std::collections::HashMap<String,String> = ["groups","apps"].into_iter().filter_map(|key|config[key].as_array()).flatten().filter_map(|item|Some((item["id"].as_str()?.to_owned(),item["name"].as_str()?.to_owned()))).collect();
    let positions:std::collections::HashMap<String,u64>=["groups","apps"].into_iter().filter_map(|k|config[k].as_array()).flatten().filter_map(|i|Some((i["id"].as_str()?.to_owned(),i["position"].as_u64()?))).collect();
    let mut presets = presets;
    let options:std::collections::HashMap<String,serde_json::Value>=["groups","apps"].into_iter().filter_map(|key|config[key].as_array()).flatten().filter_map(|item|Some((item["id"].as_str()?.to_owned(),item.get("noIcon")?.clone()))).collect();
    if let Some(value)=options.get("sys-prog"){presets["group"]["noIcon"]=value.clone();}
    for item in presets["apps"].as_array_mut().unwrap(){if let Some(value)=options.get(item["id"].as_str().unwrap()){item["noIcon"]=value.clone();}}
    let group_id=presets["group"]["id"].as_str().unwrap().to_owned();
    if let Some(name)=names.get(&group_id) { presets["group"]["name"]=name.clone().into(); }
    for item in presets["apps"].as_array_mut().unwrap() {if let Some(name)=names.get(item["id"].as_str().unwrap()) {item["name"]=name.clone().into();}}
    if let Some(p)=positions.get(&group_id){presets["group"]["position"]=(*p).into();}
    for item in presets["apps"].as_array_mut().unwrap(){if let Some(p)=positions.get(item["id"].as_str().unwrap()){item["position"]=(*p).into();}}
    let is_fixed = |item: &serde_json::Value| item["id"] == presets["group"]["id"] || presets["apps"].as_array().unwrap().iter().any(|a| a["id"] == item["id"]);
    let groups = config["groups"].as_array_mut().ok_or("Неверный формат групп")?;
    groups.retain(|g| !is_fixed(g));
    if groups.iter().filter(|g| g["parentId"] == "sys-prog").count() > 1 {
        return Err("В sys prog разрешена только одна своя подгруппа".into());
    }
    groups.insert(0, presets["group"].clone());
    let apps = config["apps"].as_array_mut().ok_or("Неверный формат приложений")?;
    apps.retain(|a| !is_fixed(a));
    if apps.iter().any(|a| a["groupId"] == "sys-prog") {
        return Err("Свои приложения нужно добавлять в подгруппу sys prog".into());
    }
    apps.extend(presets["apps"].as_array().unwrap().iter().cloned());
    Ok(())
}
fn validate_positions(config: &serde_json::Value) -> Result<(), String> {
    let groups=config["groups"].as_array().ok_or("Неверные группы")?;
    let parents:std::collections::HashMap<&str,Option<&str>>=groups.iter().filter_map(|g|Some((g["id"].as_str()?,g["parentId"].as_str()))).collect();
    if parents.len()!=groups.len(){return Err("Повторяющиеся группы".into());}
    for id in parents.keys(){let mut seen=std::collections::HashSet::new();let mut next=Some(*id);while let Some(node)=next{if !seen.insert(node){return Err("Циклическая вложенность".into());}next=*parents.get(node).ok_or("Родительская группа не найдена")?;}}
    for app in config["apps"].as_array().ok_or("Неверные приложения")? {if !app["groupId"].as_str().map(|id|parents.contains_key(id)).unwrap_or(false){return Err("Группа приложения не найдена".into());}}
    let mut menus: std::collections::HashMap<String, std::collections::HashSet<u64>> = std::collections::HashMap::new();
    for (collection, parent_key) in [("groups", "parentId"), ("apps", "groupId")] {
        let entries = config[collection].as_array().ok_or("Неверный формат конфигурации")?;
        for entry in entries {
            let mut depth=0usize;let mut ancestor=entry[parent_key].as_str();while let Some(id)=ancestor{depth+=1;ancestor=*parents.get(id).ok_or("Группа не найдена")?;}
            let capacity=config_integrity::capacity(depth);
            let position = entry["position"].as_u64().filter(|p| (1..=capacity).contains(p))
                .ok_or("Превышена вместимость кольца")?;
            let parent = match entry.get(parent_key) {
                Some(serde_json::Value::String(id)) => format!("group:{id}"),
                Some(serde_json::Value::Null) if collection == "groups" => "root".into(),
                _ => return Err("Не указана группа элемента".into()),
            };
            if !menus.entry(parent).or_default().insert(position) {
                return Err("В одном меню позиции не должны повторяться".into());
            }
        }
    }
    Ok(())
}
#[cfg(test)]
mod regression_tests;
fn main() {
    #[cfg(windows)]
    listen_for_shutdown();
    tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .on_window_event(|window, event| {
            if matches!(event, tauri::WindowEvent::CloseRequested { .. }) {
                finish_settings(window.app_handle());
            }
        })
        .invoke_handler(tauri::generate_handler![load_config, save_config, app_icons, app_metadata, launcher_monitors, launcher_action, exit_all, autostart_status, set_autostart, list_presets, save_preset, load_preset, rename_preset, delete_preset])
        .run(tauri::generate_context!()).expect("Не удалось открыть настройки FoldBOcean");
}

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::json;
    #[test]
    fn builtins_are_restored_and_valid() {
        let mut config = json!({"groups":[],"apps":[]});
        protect_system_group(&mut config).unwrap();
        validate_positions(&config).unwrap();
        assert_eq!(config["groups"][0]["name"], "sys prog");
        assert_eq!(config["apps"].as_array().unwrap().len(), 6);
        config["apps"][0]["path"] = "changed.exe".into();
        protect_system_group(&mut config).unwrap();
        assert_eq!(config["apps"][0]["path"], "ms-settings:");
    }
    #[test]
    fn rejects_second_personal_group_and_direct_apps() {
        let mut config = json!({"groups":[{"id":"a","parentId":"sys-prog"},{"id":"b","parentId":"sys-prog"}],"apps":[]});
        assert!(protect_system_group(&mut config).is_err());
        let mut config = json!({"groups":[],"apps":[{"id":"custom","groupId":"sys-prog"}]});
        assert!(protect_system_group(&mut config).is_err());
    }
    #[test]
    fn menu_size_ranges_are_validated() {
        assert!(validate_appearance(&json!({"appearance":{"web":50,"radial":180,"ringStep":12,"openMs":1500}})).is_ok());
        for appearance in [json!({"web":49}),json!({"radial":181}),json!({"ringStep":13}),json!({"ringStep":-1}),json!({"openMs":99}),json!({"openMs":1501}),json!(null)] { assert!(validate_appearance(&json!({"appearance":appearance})).is_err()); }
    }
    #[test]
    fn rgb_switches_and_speed_are_validated() {
        assert!(validate_rgb(&json!({"rgb":{"radial":true,"blocks":false,"branches":true,"speed":0.2}})).is_ok());
        for rgb in [json!({"speed":0}),json!({"speed":3.1}),json!({"speed":"1"}),json!({"blocks":1}),json!(null)] { assert!(validate_rgb(&json!({"rgb":rgb})).is_err()); }
    }
    #[test]
    fn web_material_validates_color_opacity_and_blur() {
        for opacity in [0,50,100] { assert!(validate_web_style(&json!({"webStyle":{"color":"#37abed","opacity":opacity,"blur":24}})).is_ok()); }
        for style in [json!({"color":"red"}),json!({"opacity":101}),json!({"opacity":-1}),json!({"blur":25}),json!({"blur":"12"}),json!(null)] { assert!(validate_web_style(&json!({"webStyle":style})).is_err()); }
    }
    #[test]
    fn system_ring_preserves_custom_order_and_seven_slot_limit() {
        let mut config = json!({"groups":[{"id":"mine","name":"Mine","parentId":"sys-prog","position":2}],"apps":[{"id":"sys-cmd","name":"cmd","position":7}]});
        protect_system_group(&mut config).unwrap();
        assert_eq!(config["groups"][1]["position"], 2);
        validate_positions(&config).unwrap();
        config["groups"][1]["position"] = 8.into();
        assert!(validate_positions(&config).is_err());
    }
    #[test]
    fn radial_styles_and_nested_groups_are_validated() {
        assert!(validate_radial(&json!({"radial":{"color":"#70818b","colors":{"a":"#aabbcc"},"rotation":{"@root":0.5}}})).is_ok());
        assert!(validate_radial(&json!({"radial":{"color":"red"}})).is_err());
        assert!(validate_radial(&json!({"radial":{"rotation":{"@root":9}}})).is_err());
        let mut config=json!({"groups":[{"id":"a","parentId":null,"position":1},{"id":"b","parentId":"a","position":1},{"id":"c","parentId":"b","position":1},{"id":"d","parentId":"c","position":1}],"apps":[]});
        assert!(validate_positions(&config).is_ok());config["groups"][0]["parentId"]=json!("d");assert!(validate_positions(&config).is_err());
    }
    #[test]
    fn readable_ring_capacities_and_material_settings() {
        let config=json!({"groups":[{"id":"a","parentId":null,"position":1},{"id":"b","parentId":"a","position":1}],"apps":[{"id":"x","groupId":"b","position":13}]});assert!(validate_positions(&config).is_ok());
        let mut bad=config.clone();bad["groups"][0]["position"]=json!(6);assert!(validate_positions(&bad).is_err());bad=config.clone();bad["apps"][0]["position"]=json!(14);assert!(validate_positions(&bad).is_err());
        assert!(validate_radial(&json!({"radial":{"rounded":false,"roundness":18,"blur":0}})).is_ok());assert!(validate_radial(&json!({"radial":{"blur":25}})).is_err());assert!(validate_radial(&json!({"radial":{"roundness":19}})).is_err());
    }
    #[test]
    fn validates_both_launcher_views() {
        let mut config=json!({"launcher":{"mode":"hold","trigger":{"type":"mouse","button":"middle"},"view":"web"}});assert!(validate_launcher(&config).is_ok());config["launcher"]["view"]=json!("radial");assert!(validate_launcher(&config).is_ok());config["launcher"]["view"]=json!("unknown");assert!(validate_launcher(&config).is_err());
    }
    #[test]
    fn root_links_preserve_choices_and_remove_stale_ids() {
        let mut config=json!({"groups":[{"id":"a","parentId":null},{"id":"b","parentId":null},{"id":"c","parentId":null}],"apps":[],"layout":{"nodes":{},"rootLinks":[["a","c"],["c","b"],["b","c"],["a","deleted"]]}});
        protect_layout(&mut config).unwrap();assert_eq!(config["layout"]["rootLinks"],json!([["a","c"],["c","b"]]));
        config["layout"]["rootLinks"]=json!([["a","a"]]);assert!(protect_layout(&mut config).is_err());config["layout"]["rootLinks"]=json!([["a"]]);assert!(protect_layout(&mut config).is_err());
    }
    #[test]
    fn validates_monitor_preference() {
        let mut config=json!({"launcher":{"mode":"hold","trigger":{"type":"mouse","button":"middle"},"monitor":"primary"}});
        assert!(validate_launcher(&config).is_ok());
        config["launcher"]["monitor"]=json!("disconnected-screen");assert!(validate_launcher(&config).is_ok());
        for value in [json!(42),json!(""),json!("x".repeat(129))] {config["launcher"]["monitor"]=value;assert!(validate_launcher(&config).is_err());}
    }
    #[test]
    fn validates_launcher_preferences() {
        assert!(validate_launcher(&json!({})).is_ok());
        assert!(validate_launcher(&json!({"launcher":{"mode":"hold","trigger":{"type":"mouse","button":"middle"}}})).is_ok());
        assert!(validate_launcher(&json!({"launcher":{"mode":"toggle","trigger":{"type":"keyboard","key":32,"modifiers":2}}})).is_ok());
        assert!(validate_launcher(&json!({"launcher":{"mode":"toggle","trigger":{"type":"keyboard","key":27,"modifiers":0}}})).is_err());
        assert!(validate_launcher(&json!({"launcher":{"mode":"toggle","trigger":{"type":"mouse","button":"left"}}})).is_err());
        assert!(validate_launcher(&json!({"launcher":{"mode":"hold","trigger":{"type":"mouse","button":"middle"},"corridorWidth":0}})).is_err());
        assert!(validate_launcher(&json!({"launcher":{"mode":"hold","trigger":{"type":"mouse","button":"middle"},"hoverDelayMs":100,"closeDelayMs":500,"corridorWidth":40,"branchLength":200}})).is_ok());
    }
    #[test]
    fn layout_protects_system_nodes_but_allows_personal_subgroup() {
        let mut config = json!({"groups":[{"id":"sys-prog"},{"id":"personal"}],"apps":[{"id":"sys-cmd"}],"layout":{"nodes":{"sys-prog":{"x":999,"y":999},"sys-cmd":{"x":9,"y":9},"personal":{"x":320,"y":40},"deleted":{"x":10,"y":20}}}});
        protect_layout(&mut config).unwrap();
        assert_eq!(config["layout"]["nodes"],json!({"sys-prog":{"x":999,"y":999},"sys-cmd":{"x":9,"y":9},"personal":{"x":320,"y":40}}));
        config["layout"]["nodes"]["personal"]["x"] = 3000.into();
        assert!(protect_layout(&mut config).is_err());
    }
    #[test]
    fn names_are_full_length_including_builtins_and_limited_to_256_characters() {
        let mut config=json!({"groups":[],"apps":[]});
        protect_system_group(&mut config).unwrap();
        config["apps"][1]["name"]="Консоль".into();
        config["apps"][1]["path"]="changed.exe".into();
        validate_names(&config).unwrap();protect_system_group(&mut config).unwrap();
        let cmd=config["apps"].as_array().unwrap().iter().find(|a|a["id"]=="sys-cmd").unwrap();
        assert_eq!(cmd["name"],"Консоль");assert_eq!(cmd["path"],"cmd.exe");
        config["groups"][0]["name"]="123456789".into();assert!(validate_names(&config).is_ok());
        config["groups"][0]["name"]="123456789012".into();assert!(validate_names(&config).is_ok());config["groups"][0]["name"]="Полное название программы с пробелами".into();assert!(validate_names(&config).is_ok());config["groups"][0]["name"]="x".repeat(257).into();assert!(validate_names(&config).is_err());
    }
    #[test]
    fn validates_and_preserves_individual_branch_geometry() {
        let mut config=json!({"groups":[{"id":"sys-prog"}],"apps":[],"layout":{"nodes":{},"branches":{"sys-prog":{"length":70,"radius":120,"angle":0.5},"@root":{"radius":150},"deleted":{"length":80}}}});
        protect_layout(&mut config).unwrap();
        assert_eq!(config["layout"]["branches"]["sys-prog"]["length"],70);
        assert!(config["layout"]["branches"].get("deleted").is_none());
        config["layout"]["branches"]["sys-prog"]["length"] = (-1).into();
        assert!(protect_layout(&mut config).is_err());
    }
}
