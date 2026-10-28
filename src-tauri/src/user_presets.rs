use serde_json::{json,Value};
use std::fs;
use std::path::{Path,PathBuf};
use std::sync::atomic::{AtomicU64,Ordering};
use std::time::{SystemTime,UNIX_EPOCH};

static SERIAL:AtomicU64=AtomicU64::new(0);
fn now()->u64 {SystemTime::now().duration_since(UNIX_EPOCH).unwrap_or_default().as_millis() as u64}
fn path(directory:&Path,id:&str)->Result<PathBuf,String>{
    if id.is_empty()||id.len()>100||!id.bytes().all(|c|c.is_ascii_alphanumeric()||c==b'-'||c==b'_'){
        return Err("Некорректный id пресета".into());
    }
    Ok(directory.join(format!("{id}.json")))
}
fn name(value:&str)->Result<String,String>{
    let value=value.trim();
    if value.is_empty()||value.chars().count()>80||value.chars().any(char::is_control){return Err("Имя пресета — от 1 до 80 символов без управляющих знаков".into());}
    Ok(value.to_owned())
}
fn checked(mut record:Value,id:&str)->Result<Value,String>{
    if record["version"]!=1||record["id"].as_str()!=Some(id)||record["createdAt"].as_u64().is_none()||record["updatedAt"].as_u64().is_none(){return Err("Некорректный файл пресета".into());}
    name(record["name"].as_str().ok_or("Нет имени пресета")?)?;
    super::prepare_config(&mut record["config"])?;
    Ok(record)
}
fn metadata(record:&Value)->Value{
    json!({"id":record["id"],"name":record["name"],"createdAt":record["createdAt"],"updatedAt":record["updatedAt"],
        "groups":record["config"]["groups"].as_array().map_or(0,Vec::len),"apps":record["config"]["apps"].as_array().map_or(0,Vec::len)})
}
pub fn load(directory:&Path,id:&str)->Result<Value,String>{
    let data=fs::read(path(directory,id)?).map_err(|e|format!("Не удалось открыть пресет: {e}"))?;
    checked(serde_json::from_slice(&data).map_err(|e|format!("Повреждённый JSON пресета: {e}"))?,id)
}
pub fn list(directory:&Path)->Result<Vec<Value>,String>{
    let files=match fs::read_dir(directory){Ok(files)=>files,Err(e) if e.kind()==std::io::ErrorKind::NotFound=>return Ok(vec![]),Err(e)=>return Err(e.to_string())};
    let mut result=Vec::new();
    // Keep only summaries in memory; there is no application limit on file count.
    for file in files {
        let file=file.map_err(|e|e.to_string())?;let file_path=file.path();
        if !file.file_type().map_err(|e|e.to_string())?.is_file()||file_path.extension().and_then(|s|s.to_str())!=Some("json"){continue;}
        let Some(id)=file_path.file_stem().and_then(|s|s.to_str()) else {continue;};
        if path(directory,id).is_err(){continue;}
        match load(directory,id){
            Ok(record)=>result.push(metadata(&record)),
            Err(error)=>result.push(json!({"id":id,"name":id,"error":error})),
        }
    }
    result.sort_by(|a,b|b["updatedAt"].as_u64().unwrap_or(0).cmp(&a["updatedAt"].as_u64().unwrap_or(0)).then_with(||a["id"].as_str().cmp(&b["id"].as_str())));
    Ok(result)
}
pub fn save(directory:&Path,title:&str,mut config:Value,id:Option<&str>)->Result<Value,String>{
    let title=name(title)?;super::prepare_config(&mut config)?;
    fs::create_dir_all(directory).map_err(|e|e.to_string())?;
    let generated=format!("{:x}-{:x}-{:x}",SystemTime::now().duration_since(UNIX_EPOCH).unwrap_or_default().as_nanos(),std::process::id(),SERIAL.fetch_add(1,Ordering::Relaxed));
    let id=id.unwrap_or(&generated);let destination=path(directory,id)?;
    let created=if id==generated{now()}else{load(directory,id)?["createdAt"].as_u64().ok_or("Нет даты пресета")?};
    let record=json!({"version":1,"id":id,"name":title,"createdAt":created,"updatedAt":now(),"config":config});
    let bytes=serde_json::to_vec_pretty(&record).map_err(|e|e.to_string())?;
    super::config_storage::save(&destination,&bytes,|old|checked(old.clone(),id).is_ok())?;
    Ok(metadata(&record))
}
pub fn rename(directory:&Path,id:&str,title:&str)->Result<Value,String>{
    let record=load(directory,id)?;save(directory,title,record["config"].clone(),Some(id))
}
pub fn delete(directory:&Path,id:&str)->Result<(),String>{
    let file=path(directory,id)?;
    fs::remove_file(&file).map_err(|e|format!("Не удалось удалить пресет: {e}"))?;
    for extra in [file.with_extension("json.bak"),file.with_extension("json.lock")]{let _=fs::remove_file(extra);}
    Ok(())
}
