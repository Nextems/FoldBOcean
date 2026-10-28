import {normalizeConfig} from './config.js';
const prefix='foldbocean-preset:';
const validId=id=>typeof id==='string'&&/^[a-zA-Z0-9_-]{1,100}$/.test(id);
const title=value=>{
  if(typeof value!=='string'||!value.trim()||Array.from(value.trim()).length>80||/[\u0000-\u001f\u007f-\u009f]/u.test(value))throw Error('Имя пресета — от 1 до 80 символов без управляющих знаков');
  return value.trim();
};
function summary(record){return {id:record.id,name:record.name,createdAt:record.createdAt,updatedAt:record.updatedAt,groups:record.config.groups.length,apps:record.config.apps.length};}
export function createPresetRepository({native=false,invoke,storage=globalThis.localStorage}={}){
  const key=id=>{if(!validId(id))throw Error('Некорректный id пресета');return prefix+id;};
  const loadLocal=id=>{
    const raw=storage.getItem(key(id));if(!raw)throw Error('Пресет не найден');const record=JSON.parse(raw);
    if(record.version!==1||record.id!==id||!Number.isFinite(record.createdAt)||!Number.isFinite(record.updatedAt))throw Error('Некорректный файл пресета');
    title(record.name);record.config=normalizeConfig(record.config);return record;
  };
  const saveLocal=(name,config,id)=>{
    name=title(name);config=normalizeConfig(config);const old=id?loadLocal(id):null;id??=crypto.randomUUID();
    const record={version:1,id,name,createdAt:old?.createdAt??Date.now(),updatedAt:Date.now(),config};
    storage.setItem(key(id),JSON.stringify(record));return summary(record);
  };
  return {
    async list(){
      if(native)return invoke('list_presets');const records=[];
      for(let index=0;index<storage.length;index++){
        const name=storage.key(index);if(!name?.startsWith(prefix))continue;const id=name.slice(prefix.length);if(!validId(id))continue;
        try{records.push(summary(loadLocal(id)));}catch(error){records.push({id,name:id,error:String(error.message||error)});}
      }
      return records.sort((a,b)=>(b.updatedAt||0)-(a.updatedAt||0)||a.id.localeCompare(b.id));
    },
    async save(name,config,id=null){return native?invoke('save_preset',{name,config,id}):saveLocal(name,config,id);},
    async load(id){return native?invoke('load_preset',{id}):loadLocal(id);},
    async rename(id,name){if(native)return invoke('rename_preset',{id,name});return saveLocal(name,loadLocal(id).config,id);},
    async delete(id){if(native)return invoke('delete_preset',{id});storage.removeItem(key(id));}
  };
}
