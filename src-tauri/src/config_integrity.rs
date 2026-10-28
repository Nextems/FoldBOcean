use serde_json::Value;
use std::collections::HashSet;
use std::sync::OnceLock;

fn limits() -> &'static Value {
    static LIMITS: OnceLock<Value> = OnceLock::new();
    LIMITS.get_or_init(|| serde_json::from_str(include_str!("../../src/menu-limits.json")).expect("embedded menu limits"))
}
pub fn capacity(depth: usize) -> u64 { limits()["capacities"][depth.min(2)].as_u64().unwrap() }
pub fn validate_entries(config: &Value) -> Result<(), String> {
    if config["version"].as_u64() != Some(1) { return Err("Неподдерживаемая версия конфигурации".into()); }
    for key in ["launcher", "radial", "webStyle", "rgb", "appearance", "layout"] {
        if config.get(key).is_some_and(|v| !v.is_object()) { return Err(format!("Неверный формат {key}")); }
    }
    let mut ids = HashSet::new();
    for (collection, parent) in [("groups", "parentId"), ("apps", "groupId")] {
        for item in config[collection].as_array().ok_or("Неверный формат элементов")? {
            if item.get("noIcon").is_some_and(|value|!value.is_boolean()){return Err("Некорректный режим подписи".into());}
            let id = item["id"].as_str().ok_or("Не указан id элемента")?;
            if id.is_empty() || id.len() > limits()["maxId"].as_u64().unwrap() as usize ||
                !id.as_bytes()[0].is_ascii_alphanumeric() || !id.bytes().all(|c| c.is_ascii_alphanumeric() || c == b'-' || c == b'_') {
                return Err("Некорректный id элемента".into());
            }
            if !ids.insert(id) { return Err(format!("Повторяющийся id: {id}")); }
            let name = item["name"].as_str().ok_or("Не указано имя")?;
            if name.trim().is_empty() || name.contains('\0') || name.chars().count() > limits()["maxName"].as_u64().unwrap() as usize {
                return Err("Имя должно содержать от 1 до 256 символов".into());
            }
            if !(collection == "groups" && item.get(parent) == Some(&Value::Null)) && !item[parent].is_string() {
                return Err("Не указана группа элемента".into());
            }
            if item["position"].as_u64().is_none() { return Err("Позиция должна быть целым положительным числом".into()); }
            if collection == "apps" {
                let path = item["path"].as_str().ok_or("Некорректный путь приложения")?;
                if path.trim().is_empty() || path.contains('\0') || path.encode_utf16().count() > limits()["maxPath"].as_u64().unwrap() as usize {
                    return Err("Некорректный путь приложения".into());
                }
            }
        }
    }
    Ok(())
}
