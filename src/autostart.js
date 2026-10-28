import {invoke,isTauri} from '@tauri-apps/api/core';
export function mountAutostart(properties,{status,supported=isTauri(),read=()=>invoke('autostart_status'),set=enabled=>invoke('set_autostart',{enabled})}){
 const section=document.createElement('section');section.className='rgb-controls';section.innerHTML='<h3>Запуск с Windows</h3><label class="rgb-toggle"><input id="autostart-toggle" type="checkbox" disabled>Запускать вместе с Windows</label><p id="autostart-hint">'+(supported?'Проверка автозапуска…':'Доступно в настольном приложении.')+'</p>';properties.append(section);
 if(!supported)return;
 const toggle=section.querySelector('input'),hint=section.querySelector('p');let current=false;
 const describe=()=>{hint.textContent=current?'При входе в Windows лаунчер запустится в трее.':'Автозапуск выключен. Окно настроек автоматически не открывается.';};
 read().then(value=>{if(!section.isConnected)return;current=Boolean(value.enabled);toggle.checked=current;toggle.disabled=false;describe();}).catch(error=>{if(section.isConnected)hint.textContent='Не удалось проверить автозапуск: '+String(error);});
 toggle.onchange=async()=>{toggle.disabled=true;try {const value=await set(toggle.checked);current=Boolean(value.enabled);status(current?'Автозапуск включён':'Автозапуск выключен');}catch(error){status('Не удалось изменить автозапуск: '+String(error),true);}finally {if(section.isConnected){toggle.checked=current;toggle.disabled=false;describe();}}};
}
