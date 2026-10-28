import {invoke,isTauri} from '@tauri-apps/api/core';
import {compactName,isFixed} from './config.js';
export const fallbackAppName=path=>compactName(String(path).trim().replace(/^"|"$/g,'').split(/[\\/]/).pop().replace(/\.[^.]+$/,''))||'Приложение';
export async function lookupAppName(path){if(isTauri())try {return await invoke('app_metadata',{path});}catch{}return {name:fallbackAppName(path),source:'filename'};}
export function attachAppNames(form,{existing,lookup=lookupAppName}={}){
 let source=existing?.nameSource||(existing?'custom':'exe'),request=0;
 form.elements.name.addEventListener('input',()=>{source='custom';request++;});
 async function refresh(force=false){if(!force&&source==='custom')return;const path=form.elements.path.value.trim();if(!path)return;const token=++request,before=form.elements.name.value;const value=await lookup(path);if(token!==request||form.elements.path.value.trim()!==path||form.elements.name.value!==before||!form.isConnected)return;if(value?.name){form.elements.name.value=compactName(value.name);source='exe';}}
 form.elements.path.addEventListener('change',()=>refresh());form.querySelector('#detect-app-name').onclick=()=>refresh(true);
 return {refresh,source:()=>source};
}
export async function migrateAppNames(config,lookup=lookupAppName){let changed=false;for(const app of config.apps){if(isFixed(app)||app.nameSource)continue;const previous=app.name,path=app.path;const result=await lookup(path);if(!app.nameSource&&app.name===previous&&app.path===path&&result?.name){app.name=compactName(result.name);app.nameSource='exe';changed=true;}}return changed;}
