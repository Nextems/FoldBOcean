use super::*;
use std::fs;
use std::path::PathBuf;
use std::sync::atomic::{AtomicUsize, Ordering};
static SERIAL: AtomicUsize=AtomicUsize::new(0);
struct Directory(PathBuf);
impl Directory { fn new()->Self {let path=std::env::temp_dir().join(format!("foldbocean-test-{}-{}",std::process::id(),SERIAL.fetch_add(1,Ordering::Relaxed)));fs::create_dir(&path).unwrap();Self(path)} }
impl Drop for Directory {fn drop(&mut self){let _=fs::remove_dir_all(&self.0);}}
fn preset_config()->serde_json::Value{
    let fixtures:serde_json::Value=serde_json::from_str(include_str!("../../tests/config-cases.json")).unwrap();
    fixtures[0]["config"].clone()
}
#[test]
fn no_icon_option_survives_builtin_protection_and_preset_roundtrip(){
    let dir=Directory::new();let mut config=preset_config();
    config["groups"][0]["noIcon"]=true.into();config["apps"][0]["noIcon"]=true.into();
    prepare_config(&mut config).unwrap();
    assert!(config["groups"].as_array().unwrap().iter().any(|g|g["id"]=="sys-prog"&&g["noIcon"]==true));
    assert!(config["apps"].as_array().unwrap().iter().any(|a|a["id"]=="sys-settings"&&a["noIcon"]==true));
    let record=user_presets::save(&dir.0,"Без значков",config,None).unwrap();
    let loaded=user_presets::load(&dir.0,record["id"].as_str().unwrap()).unwrap();
    assert!(loaded["config"]["apps"].as_array().unwrap().iter().any(|a|a["id"]=="sys-settings"&&a["noIcon"]==true));
}
#[test]
fn user_can_keep_more_than_two_hundred_presets_across_sessions(){
    let dir=Directory::new();assert!(user_presets::list(&dir.0).unwrap().is_empty());
    let config=preset_config();let original=config.clone();let mut ids=std::collections::HashSet::new();
    for index in 0..205{let metadata=user_presets::save(&dir.0,&format!("Пресет {index}"),config.clone(),None).unwrap();assert!(ids.insert(metadata["id"].as_str().unwrap().to_owned()));}
    let all=user_presets::list(&dir.0).unwrap();assert_eq!(all.len(),205);
    assert!(all.iter().all(|record|record.get("config").is_none()&&record.get("error").is_none()));
    for record in all.iter().take(3){let loaded=user_presets::load(&dir.0,record["id"].as_str().unwrap()).unwrap();assert_eq!(loaded["config"]["groups"],original["groups"]);}
    assert_eq!(config,original);
}
#[test]
fn presets_update_rename_delete_and_corruption_are_isolated(){
    let dir=Directory::new();let config=preset_config();
    let metadata=user_presets::save(&dir.0,"Работа",config.clone(),None).unwrap();let id=metadata["id"].as_str().unwrap();
    let renamed=user_presets::rename(&dir.0,id,"Игры").unwrap();assert_eq!(renamed["createdAt"],metadata["createdAt"]);
    let mut changed=config.clone();changed["appearance"]=serde_json::json!({"web":130,"radial":110,"ringStep":8});
    user_presets::save(&dir.0,"Игры",changed,Some(id)).unwrap();
    assert_eq!(user_presets::load(&dir.0,id).unwrap()["config"]["appearance"]["web"],130);
    let backup:serde_json::Value=serde_json::from_slice(&fs::read(dir.0.join(format!("{id}.json.bak"))).unwrap()).unwrap();
    assert_eq!(backup["config"]["groups"],config["groups"]);
    fs::write(dir.0.join("broken.json"),b"bad JSON").unwrap();
    assert_eq!(user_presets::list(&dir.0).unwrap().len(),2);
    assert!(user_presets::load(&dir.0,"broken").is_err());
    assert!(user_presets::list(&dir.0).unwrap().iter().any(|record|record["id"]=="broken"&&record["error"].is_string()));
    user_presets::delete(&dir.0,id).unwrap();user_presets::delete(&dir.0,"broken").unwrap();
    assert!(user_presets::list(&dir.0).unwrap().is_empty());assert!(!dir.0.join(format!("{id}.json.bak")).exists());
}
#[test]
fn preset_names_ids_and_configs_cannot_escape_the_store(){
    let dir=Directory::new();let config=preset_config();
    for id in ["../config","..\\config","C:\\config","", "../presets/a"]{
        assert!(user_presets::load(&dir.0,id).is_err());assert!(user_presets::delete(&dir.0,id).is_err());
        assert!(user_presets::save(&dir.0,"Name",config.clone(),Some(id)).is_err());
    }
    for name in ["", "   ","bad\nname"]{assert!(user_presets::save(&dir.0,name,config.clone(),None).is_err());}
    let mut bad=config;bad["apps"][0]["path"]=42.into();assert!(user_presets::save(&dir.0,"Invalid",bad,None).is_err());
    assert!(user_presets::list(&dir.0).unwrap().is_empty());
}
#[test]
fn shared_config_regressions(){
    let fixtures:serde_json::Value=serde_json::from_str(include_str!("../../tests/config-cases.json")).unwrap();
    for fixture in fixtures.as_array().unwrap(){
        let result=prepare_config(&mut fixture["config"].clone());
        assert_eq!(result.is_ok(),fixture["valid"].as_bool().unwrap(),"{}: {:?}",fixture["name"],result);
    }
}
#[test]
fn backup_contains_previous_valid_config_and_corruption_cannot_overwrite_it(){
    let dir=Directory::new();let path=dir.0.join("config.json");
    config_storage::save(&path,b"{\"version\":1}",|_|true).unwrap();
    config_storage::save(&path,b"{\"version\":2}",|_|true).unwrap();
    assert_eq!(fs::read(&path).unwrap(),b"{\"version\":2}");
    let backup=path.with_extension("json.bak");
    assert_eq!(fs::read(&backup).unwrap(),b"{\"version\":1}");
    fs::write(&path,b"broken").unwrap();
    assert!(config_storage::save(&path,b"{}",|_|true).is_err());
    assert_eq!(fs::read(&path).unwrap(),b"broken");
    assert_eq!(fs::read(&backup).unwrap(),b"{\"version\":1}");
    assert!(!fs::read_dir(&dir.0).unwrap().any(|entry|entry.unwrap().path().extension().map(|x|x=="tmp").unwrap_or(false)));
}
#[test]
fn concurrent_writers_leave_two_complete_documents(){
    let dir=Directory::new();let path=dir.0.join("config.json");
    config_storage::save(&path,b"{\"value\":0}",|_|true).unwrap();
    let threads:Vec<_>=(1..=8).map(|value|{let path=path.clone();std::thread::spawn(move||config_storage::save(&path,format!("{{\"value\":{value}}}").as_bytes(),|_|true).unwrap())}).collect();
    for thread in threads{thread.join().unwrap();}
    let current:serde_json::Value=serde_json::from_slice(&fs::read(&path).unwrap()).unwrap();
    let backup:serde_json::Value=serde_json::from_slice(&fs::read(path.with_extension("json.bak")).unwrap()).unwrap();
    assert_ne!(current,backup);
    assert!((1..=8).contains(&current["value"].as_i64().unwrap()));
}
