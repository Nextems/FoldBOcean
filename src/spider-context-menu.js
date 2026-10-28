import {menuCapacity} from './radial-layout.js';
import {SYSTEM_ID} from './config.js';
export function createSpiderContextMenu(container,{getConfig,edit}){
 const menu=document.createElement('div');menu.className='spider-context-menu';menu.setAttribute('role','menu');menu.hidden=true;container.append(menu);let events=null;
 function close(){menu.hidden=true;events?.abort();events=null;}
 function show(event,parent){
  close();const config=getConfig(),group=config.groups.find(g=>g.id===parent),children=id=>[...config.groups.filter(g=>g.parentId===id),...config.apps.filter(a=>a.groupId===id)],root=parent==='@root',full=children(root?null:parent).length>=menuCapacity(config,parent),personal=config.groups.find(g=>g.parentId===SYSTEM_ID);
  menu.replaceChildren();const caption=document.createElement('div');caption.className='context-menu-caption';caption.textContent=group?group.name:'Основные папки';menu.append(caption);
  const add=(label,type,target,disabled=false,hint='')=>{const button=document.createElement('button');button.type='button';button.setAttribute('role','menuitem');button.textContent=label;button.disabled=disabled;button.title=hint;button.onclick=()=>{close();edit(type,undefined,target);};menu.append(button);};
  const system=parent===SYSTEM_ID;
  add(root?'Создать основную группу':'Добавить группу в «'+group.name+'»','group',parent,full||system&&!!personal,system&&personal?'В системной группе разрешена одна своя подгруппа.':full?'В группе нет свободных позиций.':'');
  const appParent=system?personal?.id:parent,appFull=system&&personal?children(personal.id).length>=menuCapacity(config,personal.id):root?false:full;
  add(system&&personal?'Добавить программу в «'+personal.name+'»':root?'Добавить программу…':'Добавить программу в «'+group.name+'»','app',appParent,appFull||system&&!personal,system&&!personal?'Сначала создайте свою подгруппу.':appFull?'В группе нет свободных позиций.':'');
  menu.hidden=false;menu.style.left=event.clientX+'px';menu.style.top=event.clientY+'px';const bounds=menu.getBoundingClientRect();menu.style.left=Math.max(8,Math.min(event.clientX,window.innerWidth-bounds.width-8))+'px';menu.style.top=Math.max(8,Math.min(event.clientY,window.innerHeight-bounds.height-8))+'px';
  events=new AbortController();const options={signal:events.signal,capture:true};window.addEventListener('pointerdown',e=>{if(!menu.contains(e.target))close();},options);window.addEventListener('keydown',e=>{if(e.key==='Escape'){e.preventDefault();e.stopPropagation();close();container.querySelector('svg')?.focus();}},options);window.addEventListener('blur',close,{signal:events.signal});window.addEventListener('resize',close,{signal:events.signal});
  menu.querySelector('button:not(:disabled)')?.focus({preventScroll:true});
 }
 menu.onkeydown=event=>{if(!['ArrowDown','ArrowUp'].includes(event.key))return;event.preventDefault();event.stopPropagation();const buttons=[...menu.querySelectorAll('button:not(:disabled)')],index=buttons.indexOf(document.activeElement);buttons[(index+(event.key==='ArrowDown'?1:-1)+buttons.length)%buttons.length]?.focus();};
 return {show,close};
}
