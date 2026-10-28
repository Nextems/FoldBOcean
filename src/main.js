import {createPresetRepository} from './preset-repository.js';
import {mountPresetManager} from './preset-manager.js';
import {attachAppNames,migrateAppNames} from './app-names.js';
import {validateHierarchy} from './config-validation.js';
import {createConfigStore} from './config-store.js';
import './style.css';
import {menuCapacity} from './radial-layout.js';
import { mountWebEditor } from './web-editor.js';
import { compactName, SYSTEM_ID, isFixed, normalizeConfig, initialConfig } from './config.js';
import { invoke, isTauri } from '@tauri-apps/api/core';
import { open } from '@tauri-apps/plugin-dialog';

const presetRepository=createPresetRepository({native:isTauri(),invoke});
let config = initialConfig();
const capacity=parent=>menuCapacity(config,parent??'@root');
let selected = null;
let currentTab = 'library';
let interactionRequested = false;
let saving = Promise.resolve();
let readOnly = false;
let restoring = false;
const configStore=createConfigStore({initial:config,validate:validateHierarchy,
  write:async snapshot=>{if(isTauri())await invoke('save_config',{config:snapshot});else localStorage.setItem('spiderweb-preview',JSON.stringify(snapshot));},
  onRollback:(snapshot,error)=>{config=snapshot;restoring=true;try{render();}finally{restoring=false;}status(`Не сохранено, изменения отменены: ${error}`,true);}
});
const root = document.querySelector('#app');
const esc = value => String(value).replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const ordered = items => [...items].sort((a,b) => a.position-b.position);
const group = id => config.groups.find(g => g.id === id);
const siblings = parent => ordered(config.groups.filter(g => g.parentId === parent));
const items = id => ordered([...config.groups.filter(g => g.parentId === id), ...config.apps.filter(a => a.groupId === id)]);
function reindex(list) {
  list.forEach((item,index)=>item.position=index+1);
}
function status(text, error = false) { const el = document.querySelector('#status'); el.textContent = text; el.classList.toggle('error', error); }
async function persist() {
  if(readOnly||restoring)return false;
  status('Сохранение…');
  saving=configStore.save(config);const saved=await saving;
  if(saved)status(isTauri()?'Изменения сохранены в JSON':'Предпросмотр · сохранено в браузере');
  return saved;
}
function treeNode(g, nested = false) {
  const children = siblings(g.id);
  return `<button class="tree-item ${selected === g.id ? 'selected' : ''} ${nested ? 'nested' : ''}" data-select="${esc(g.id)}"><span class="folder">${nested ? '◇' : '▱'}</span><span>${esc(g.name)}</span><small>${items(g.id).length}</small></button>${children.map(c => treeNode(c,true)).join('')}`;
}
function render() {
  const current = group(selected);
  const list = current ? items(current.id) : siblings(null);
  root.innerHTML = `<aside><div class="brand"><span class="brand-icon">◌</span><div>FoldBOcean<small>НАСТРОЙКИ ЛАУНЧЕРА</small></div></div><div class="section-label">РАБОЧЕЕ ПРОСТРАНСТВО</div><button class="tree-item ${!current?'selected':''}" data-home><span>⌘</span> Все группы <small>${config.groups.length}</small></button><button class="tree-item" id="web-tab"><span>⌘</span> Круговая панель</button><button class="tree-item" id="presets-tab"><span>▤</span> Пресеты</button><div class="section-label tree-label">ГРУППЫ</div><nav>${siblings(null).map(g=>treeNode(g)).join('') || '<p class="tree-empty">Здесь появятся ваши группы</p>'}</nav><div class="aside-bottom"><button id="launcher-options" class="launcher-options">⚙ Вызов панели</button><span class="dot"></span> Локальная конфигурация<small>FoldBOcean · 0.1</small></div></aside><main><header><span>Настройки <span class="slash">/</span> Приложения и группы</span><span class="badge">ЛИЧНОЕ ПРОСТРАНСТВО</span></header><section class="workspace"><div class="eyebrow">ВАША ПАНЕЛЬ</div><div class="heading"><div><h1>${current?esc(current.name):'Приложения и группы'}</h1><p>${current?.id===SYSTEM_ID?'Постоянные инструменты Windows и одна подгруппа для своих программ.':current?'Настройте содержимое и порядок элементов в этой группе.':'Соберите приложения в группы. Всё нужное — на своих местах.'}</p></div>${current?'<button class="icon-button" id="edit-current" title="Редактировать группу">✎</button>':''}</div><div class="toolbar"><button class="primary" id="create-group"><span>＋</span> Создать группу</button><button id="create-app"><span>＋</span> Добавить приложение</button></div><div class="list-heading"><span>${current?'СОДЕРЖИМОЕ ГРУППЫ':'КОРНЕВЫЕ ГРУППЫ'}</span><span>${list.length} / ${capacity(current?.id??null)} позиций</span></div><div class="items">${list.length ? list.map((item,index)=>`<article class="item"><span class="position">${String(item.position).padStart(2,'0')}</span><div class="item-icon ${item.path?'app-icon':''}">${item.path?'↗':'▱'}</div><div class="item-info"><button class="item-name" data-${item.path?(isFixed(item)?'fixed':'edit'):'select'}="${esc(item.id)}">${esc(item.name)}</button><small>${item.path?esc(item.path):`${items(item.id).length} элементов · ${item.parentId?'Вложенная группа':'Группа'}`}</small></div><div class="row-actions">${isFixed(item)?`<button data-edit="${esc(item.id)}" title="Переименовать">✎</button><span class="fixed-label">Системный</span>`:`<button data-move="${esc(item.id)}" data-direction="-1" ${index===0||isFixed(list[index-1])||current?.id===SYSTEM_ID?'disabled':''} title="Выше">↑</button><button data-move="${esc(item.id)}" data-direction="1" ${index===list.length-1||isFixed(list[index+1])||current?.id===SYSTEM_ID?'disabled':''} title="Ниже">↓</button><button data-edit="${esc(item.id)}" title="Редактировать">✎</button><button data-delete="${esc(item.id)}" title="Удалить">×</button>`}</div></article>`).join('') : `<div class="empty"><div class="empty-symbol">${current?'◇':'✳'}</div><h2>${current?'Здесь пока пусто':'Начните с первой группы'}</h2><p>${current?'Добавьте приложение или вложенную группу.':'Создайте группу, а затем добавьте в неё приложения.'}</p></div>`}</div><div class="hint"><span>ⓘ</span> Позиция задаёт порядок элементов в панели. Вместимость колец: 5 / 7 / 13 элементов. Вложенные группы раскрываются новыми кольцами.</div></section><footer><span id="status">${isTauri()?'Изменения сохраняются автоматически':'Предпросмотр в браузере'}</span><span><button class="footer-button" id="demo-web">Показать панель</button><button class="footer-button" id="exit-all">Завершить всё</button></span></footer></main><dialog id="editor"></dialog><dialog id="confirm"></dialog>`;
  document.querySelectorAll('[data-select]').forEach(el=>el.onclick=()=>{selected=el.dataset.select;currentTab='library';render();});
  document.querySelector('[data-home]').onclick=()=>{selected=null;currentTab='library';render();};
  document.querySelector('#create-group').onclick=()=>editor('group');
  document.querySelector('#launcher-options').onclick=launcherEditor;
  document.querySelector('#demo-web').onclick=()=>launcherAction('demo');
  document.querySelector('#exit-all').onclick=async()=>{if(!isTauri()){status('Доступно в настольном приложении.',true);return;}await saving;try{await invoke('exit_all');}catch(e){status(String(e),true);}};
  document.querySelector('#create-app').onclick=()=>editor('app');
  document.querySelector('#edit-current')?.addEventListener('click',()=>editor('group',current));
  document.querySelectorAll('[data-edit]').forEach(el=>el.onclick=()=>{const item=config.groups.find(g=>g.id===el.dataset.edit)||config.apps.find(a=>a.id===el.dataset.edit);editor(item.path?'app':'group',item);});
  document.querySelectorAll('[data-move]').forEach(el=>el.onclick=()=>{const i=list.findIndex(x=>x.id===el.dataset.move),j=i+Number(el.dataset.direction);[list[i],list[j]]=[list[j],list[i]];reindex(list);render();persist();});
  document.querySelectorAll('[data-delete]').forEach(el=>el.onclick=()=>remove(el.dataset.delete));
  document.querySelector('#web-tab').onclick=()=>{currentTab='web';render();};
  document.querySelector('#presets-tab').onclick=()=>{currentTab='presets';render();};
  if(currentTab==='presets') {
    document.querySelector('[data-home]').classList.remove('selected');
    document.querySelector('#presets-tab').classList.add('selected');
    document.querySelector('header > span').textContent='Настройки / Пресеты';
    mountPresetManager(document.querySelector('.workspace'),{repository:presetRepository,readOnly,native:isTauri(),status,
      getSnapshot:async()=>{await saving;return structuredClone(config);},
      apply:async snapshot=>{await saving;if(readOnly)return false;config=normalizeConfig(snapshot);selected=null;const saved=await persist();if(saved)render();return saved;}
    });
  }
  if(currentTab==='web') {
    document.querySelector('main').classList.add('web-view');
    document.querySelector('[data-home]').classList.remove('selected');
    document.querySelector('#web-tab').classList.add('selected');
    document.querySelector('header > span').textContent='Настройки / Круговая панель';
    const workspace=document.querySelector('.workspace');workspace.className='web-workspace';
    mountWebEditor(workspace,{getConfig:()=>config,getMonitors:async()=>isTauri()?invoke('launcher_monitors'):[],persist,launch:launcherAction,edit:editor,requestDelete:remove,status,getIcons:async()=>{if(!isTauri())return {};await saving;return invoke('app_icons');},initialInteraction:interactionRequested});
    interactionRequested=false;
  }
}
async function launcherAction(action) {
  if(!isTauri()){status('Круговая панель запускается из настольного приложения.',true);return;}
  try{await saving;await invoke('launcher_action',{action});status(action==='stop'?'Круговая панель завершена':'Круговая панель запущена · значок в трее');}
  catch(e){status(String(e),true);}
}
function launcherEditor() { currentTab='web'; interactionRequested=true; render(); }
function options(groups, value, empty = false) {
  return `${empty?'<option value="">Без родителя · корневая группа</option>':''}${groups.map(g=>`<option value="${esc(g.id)}" ${g.id===value?'selected':''}>${g.parentId?'↳ ':''}${esc(g.name)}</option>`).join('')}`;
}
const labelOption=item=>'<label class="block-label-option"><input type="checkbox" name="noIcon" '+(item?.noIcon?'checked':'')+'> Без значка — больше текста</label><p class="field-hint">До 15 символов в паутине, до 20 в круге, с переносами.</p>';
function renameBuiltin(item) {
  const modal=document.querySelector('#editor');
  modal.innerHTML=`<form><h2>Переименовать</h2><label>Имя · до 256 символов<input name="name" required maxlength="256" value="${esc(item.name)}" autofocus></label>${labelOption(item)}<p class="field-hint">Системная команда и её место в группе сохраняются.</p><p class="form-error" role="alert"></p><div class="modal-footer"><button type="button" class="cancel">Отмена</button><button class="primary" type="submit">Сохранить</button></div></form>`;
  modal.querySelector('.cancel').onclick=()=>modal.close();
  modal.querySelector('form').onsubmit=async event=>{event.preventDefault();const name=event.target.elements.name.value.trim();if(!name||Array.from(name).length>256){modal.querySelector('.form-error').textContent='Имя — от 1 до 256 символов.';return;}item.name=name;if(event.target.elements.noIcon.checked)item.noIcon=true;else delete item.noIcon;modal.close();render();await persist();};
  modal.showModal();
}
function editor(type, existing, preferredParent) {
  if(readOnly)return;
  const app = type==='app';
  if(isFixed(existing)) { renameBuiltin(existing); return; }
  const appGroups=config.groups.filter(g=>g.id!==SYSTEM_ID);
  if(app&&!appGroups.length){status('Создайте группу для своих приложений.',true);return;}
  if (app && !config.groups.length) { status('Сначала создайте группу для приложения.',true); return; }
  const modal=document.querySelector('#editor');
  const descendants=new Set(existing?[existing.id]:[]);let changed=true;while(changed){changed=false;for(const g of config.groups)if(descendants.has(g.parentId)&&!descendants.has(g.id)){descendants.add(g.id);changed=true;}}
  const possibleParents=config.groups.filter(g=>!descendants.has(g.id));
  if(preferredParent==='@root')preferredParent=app?undefined:'';
  if(app&&preferredParent===SYSTEM_ID)preferredParent=config.groups.find(g=>g.parentId===SYSTEM_ID)?.id;
  const parent=preferredParent??(app ? (existing?.groupId||(selected!==SYSTEM_ID?selected:null)||appGroups[0]?.id) : (existing?.parentId || (selected||''))); 
  const locked=false;
  modal.innerHTML=`<form><div class="modal-header"><div><div class="eyebrow">${app?'ПРИЛОЖЕНИЕ':'ГРУППА'}</div><h2>${existing?'Редактировать':app?'Добавить приложение':'Создать группу'}</h2></div><button type="button" class="close" aria-label="Закрыть">×</button></div><label>Название<input name="name" required maxlength="256" placeholder="${app?'Например, Obsidian':'Например, Работа'}" value="${esc(existing?.name||'')}" autofocus></label>${app?'<button type="button" id="detect-app-name">Название из EXE</button><p class="field-hint">Хранится полное название. В меню выводится короткая подпись, полное имя — в подсказке.</p>':''}${app?`<label>Путь к приложению<div class="path-field"><input name="path" required placeholder="C:\\Program Files\\…\\app.exe" value="${esc(existing?.path||'')}"><button type="button" id="browse" title="Выбрать файл">Обзор…</button></div></label>`:''}<label>${app?'Группа':'Родительская группа'}<select name="parent" ${locked?'disabled':''}>${options(app?appGroups:possibleParents,parent,!app)}</select></label>${locked?'<p class="field-hint">У этой группы есть подгруппы, поэтому она остаётся корневой.</p>':''}${labelOption(existing)}<label>Позиция<input name="position" type="number" min="1" max="${capacity(parent)}" step="1" required value="${existing?.position||Math.min(capacity(parent),((app?items(parent):parent?items(parent):siblings(null)).length+1))}"></label><p class="form-error" role="alert"></p><div class="modal-footer"><button type="button" class="cancel">Отмена</button><button class="primary" type="submit">${existing?'Сохранить':app?'Добавить приложение':'Создать группу'}</button></div></form>`;
  const form=modal.querySelector('form');
  const appNames=app?attachAppNames(form,{existing}):null;if(app)form.noValidate=true;
  const updatePosition=()=>{
    const systemChild=!app&&form.elements.parent.value===SYSTEM_ID;
    form.elements.position.disabled=false;form.elements.position.max=capacity(form.elements.parent.value||null);
    if(systemChild&&!existing){const used=new Set(items(SYSTEM_ID).map(i=>i.position));form.elements.position.value=[1,2,3,4,5,6,7].find(p=>!used.has(p))||7;}
  };
  form.elements.parent.onchange=updatePosition;
  updatePosition();
  modal.querySelector('.close').onclick=modal.querySelector('.cancel').onclick=()=>modal.close();
  modal.querySelector('#browse')?.addEventListener('click',async()=>{
    if (!isTauri()) { modal.querySelector('.form-error').textContent='Выбор файла доступен в настольном приложении. Здесь можно ввести путь.'; return; }
    try { const path=await open({multiple:false,filters:[{name:'Приложения и ярлыки',extensions:['exe','lnk','bat','cmd','com']} ]}); if(path){form.elements.path.value=path;await appNames.refresh();} } catch(e){modal.querySelector('.form-error').textContent=String(e);}
  });
  form.onsubmit=async event=>{
    event.preventDefault();
    if(app)await appNames.refresh();
    const name=form.elements.name.value.trim(), parentId=locked?null:form.elements.parent.value||null;
    if(!name || (app&&!form.elements.path.value.trim())) {modal.querySelector('.form-error').textContent='Заполните название и путь.';return;}
    if(Array.from(name).length>256){modal.querySelector('.form-error').textContent='Не более 256 символов в полном названии.';return;}
    const MAX_POSITIONS=capacity(parentId);
    const position=Number(form.elements.position.value);
    if (!Number.isInteger(position) || position < 1 || position > MAX_POSITIONS) {
      modal.querySelector('.form-error').textContent=`Позиция должна быть целым числом от 1 до ${MAX_POSITIONS}.`; return;
    }
    if(app&&parentId===SYSTEM_ID){modal.querySelector('.form-error').textContent='Добавляйте приложения в свою подгруппу.';return;}
    if(!app&&parentId===SYSTEM_ID&&config.groups.some(g=>g.parentId===SYSTEM_ID&&g.id!==existing?.id)){
      modal.querySelector('.form-error').textContent='В sys prog разрешена только одна своя подгруппа.';return;
    }
    const destination=parentId?items(parentId):siblings(null);
    if (destination.filter(x=>x.id!==existing?.id).length >= MAX_POSITIONS) {
      modal.querySelector('.form-error').textContent=`Все ${MAX_POSITIONS} позиций заняты. Выберите другую группу или освободите позицию.`; return;
    }
    const previous=config;config=structuredClone(config);
    const item=existing?config[app?'apps':'groups'].find(i=>i.id===existing.id):{id:crypto.randomUUID()};
    const oldParent=existing?(app?existing.groupId:existing.parentId):undefined;
    Object.assign(item,{name,position:Number(form.elements.position.value)},app?{groupId:parentId,path:form.elements.path.value.trim(),nameSource:appNames.source()}:{parentId});
    if(form.elements.noIcon.checked)item.noIcon=true;else delete item.noIcon;
    if(!existing)(app?config.apps:config.groups).push(item);
    const target=parentId?items(parentId):siblings(null);
    const rest=target.filter(x=>x.id!==item.id);
    rest.splice(Math.min(rest.length,Math.max(0,item.position-1)),0,item);
    reindex(rest);
    if(oldParent!==undefined&&oldParent!==parentId)reindex(oldParent?items(oldParent):siblings(null));
    try{validateHierarchy(config);}catch(error){config=previous;modal.querySelector('.form-error').textContent=String(error.message||error);return;}
    modal.close(); render(); await persist();
  };
  modal.showModal();
}
function remove(selection) {
  if(readOnly)return;
  const requested=new Set(Array.isArray(selection)?selection:[selection]),all=[...config.groups,...config.apps],chosen=all.filter(item=>requested.has(item.id)&&!isFixed(item)),protectedCount=all.filter(item=>requested.has(item.id)&&isFixed(item)).length;
  if(!chosen.length){if(protectedCount)status('Системные элементы нельзя удалять.',true);return;}
  const modal=document.querySelector('#confirm');if(modal.open||document.querySelector('#editor[open]'))return;
  const ids=new Set(chosen.map(item=>item.id));let changed=true;while(changed){changed=false;for(const g of config.groups)if(ids.has(g.parentId)&&!isFixed(g)&&!ids.has(g.id)){ids.add(g.id);changed=true;}}
  for(const app of config.apps)if(ids.has(app.groupId)&&!isFixed(app))ids.add(app.id);
  const groupCount=config.groups.filter(g=>ids.has(g.id)&&!isFixed(g)).length,appCount=config.apps.filter(a=>ids.has(a.id)&&!isFixed(a)).length;
  const title=chosen.length===1?'Удалить из меню «'+esc(chosen[0].name)+'»?':'Удалить выбранные элементы?';
  modal.returnValue='';modal.innerHTML=`<form method="dialog"><h2>${title}</h2><p>${groupCount?'Группы будут удалены вместе с вложенными группами и приложениями. ':''}Будет удалено: групп — ${groupCount}, программ — ${appCount}.</p>${protectedCount?'<p>Выбранные системные элементы останутся.</p>':''}<div class="modal-footer"><button value="cancel" autofocus>Отмена</button><button class="danger" value="delete">Удалить</button></div></form>`;
  modal.onclose=async()=>{if(modal.returnValue!=='delete')return;config.groups=config.groups.filter(g=>!ids.has(g.id)||isFixed(g));config.apps=config.apps.filter(a=>!ids.has(a.id)||isFixed(a));for(const id of ids){if(config.layout?.nodes)delete config.layout.nodes[id];if(config.layout?.branches)delete config.layout.branches[id];if(config.radial?.colors)delete config.radial.colors[id];if(config.radial?.rotation)delete config.radial.rotation[id];}config=normalizeConfig(config);if(ids.has(selected))selected=null;render();await persist();};modal.showModal();
}
async function init(){render();try{const loaded=isTauri()?await invoke('load_config'):JSON.parse(localStorage.getItem('spiderweb-preview')||'null');if(loaded){if(loaded.version!==1||!Array.isArray(loaded.groups)||!Array.isArray(loaded.apps))throw new Error('Неподдерживаемый формат конфигурации');config=normalizeConfig(loaded);configStore.confirm(config);render();}await persist();if(isTauri()&&await migrateAppNames(config)){if(!document.querySelector('dialog[open]'))render();await persist();}}catch(e){readOnly=true;status(`Ошибка чтения конфигурации: ${e}`,true);document.querySelector('#create-group').disabled=true;document.querySelector('#create-app').disabled=true;}}
init();
