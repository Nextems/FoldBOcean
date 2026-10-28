import {rootLinks} from './root-links.js';
import presets from './system-presets.json';
import {validateEntries,validateHierarchy,capacityAtDepth} from './config-validation.js';
export const SYSTEM_ID = presets.group.id;
export const isFixed = item => item?.id === SYSTEM_ID || presets.apps.some(app=>app.id===item?.id);
// Built-in metadata is protected; every node can be placed freely.
export const isPositionFixed = () => false;
const legacyNames = {'sys-settings':'sett','sys-cmd':'cmd','sys-regedit':'regedit','sys-disks':'disk','sys-devices':'Устр','sys-control':'управ'};
export function compactName(name,limit=256) { return Array.from(String(name).trim()).slice(0,limit).join(''); }
export function menuName(name,limit=12){const chars=Array.from(String(name).trim());return chars.length>limit?chars.slice(0,limit-3).join('')+'...':chars.join('');}
export function normalizeConfig(source) {
  validateEntries(source);
  const config=structuredClone(source);
  delete config.explorer;
  config.appearance={web:100,radial:100,ringStep:6,openMs:450,...config.appearance};
  for(const [key,min,max] of [['web',50,180],['radial',50,180],['ringStep',0,12],['openMs',100,1500]])if(!Number.isFinite(config.appearance[key])||config.appearance[key]<min||config.appearance[key]>max)throw new Error('Некорректный размер меню');
  config.rgb={radial:false,blocks:false,branches:false,speed:1,...config.rgb};
  if(['radial','blocks','branches'].some(key=>typeof config.rgb[key]!=='boolean')||!Number.isFinite(config.rgb.speed)||config.rgb.speed<0.2||config.rgb.speed>3)throw new Error('Некорректный режим RGB');
  config.webStyle={color:'#70818b',opacity:100,blur:0,...config.webStyle};
  if(!/^#[a-f\d]{6}$/i.test(config.webStyle.color)||!Number.isFinite(config.webStyle.opacity)||config.webStyle.opacity<0||config.webStyle.opacity>100||!Number.isFinite(config.webStyle.blur)||config.webStyle.blur<0||config.webStyle.blur>24)throw new Error('Некорректный вид паутины');
  const savedOptions=new Map([...config.groups,...config.apps].filter(i=>Object.hasOwn(i,'noIcon')).map(i=>[i.id,{noIcon:i.noIcon}]));
  const savedNames=new Map([...config.groups,...config.apps].map(item=>[item.id,item.name]));
  const builtinName=item=>compactName(savedNames.get(item.id)&&savedNames.get(item.id)!==legacyNames[item.id]&&savedNames.get(item.id)!==compactName(item.name,12)?savedNames.get(item.id):item.name);
  for(const item of [...config.groups,...config.apps])item.name=compactName(item.name);
  config.launcher = {view:'radial',monitor:'primary',mode:'hold',trigger:{type:'keyboard',key:192,modifiers:2,label:'`'},hoverDelayMs:180,closeDelayMs:350,corridorWidth:32,branchLength:220,...config.launcher};
  const savedPositions=new Map([...config.groups,...config.apps].map(i=>[i.id,i.position]));
  config.groups=config.groups.filter(g=>!isFixed(g));
  config.apps=config.apps.filter(a=>!isFixed(a));
  config.groups.unshift({...structuredClone(presets.group),...savedOptions.get(presets.group.id),name:builtinName(presets.group),position:savedPositions.get(presets.group.id)||presets.group.position});
  config.apps.unshift(...presets.apps.map(item=>({...structuredClone(item),...savedOptions.get(item.id),name:builtinName(item),position:savedPositions.get(item.id)||item.position})));
  const roots=config.groups.filter(g=>g.parentId===null&&g.id!==SYSTEM_ID).sort((a,b)=>a.position-b.position);
  if(roots.length>=capacityAtDepth(0)) throw new Error('В корне максимум 5 групп, включая sys prog.');

  const children=config.groups.filter(g=>g.parentId===SYSTEM_ID);
  if(children.length>1||config.apps.some(a=>a.groupId===SYSTEM_ID&&!isFixed(a))) throw new Error('В sys prog разрешена только одна своя подгруппа.');

  config.radial??={color:'#70818b',colors:{},rotation:{}};
  config.radial.color??='#70818b';config.radial.colors??={};config.radial.rotation??={};
  config.layout ??= {nodes:{}};
  config.layout.branches??={};
  if(config.layout.rootLinks!==undefined){if(!Array.isArray(config.layout.rootLinks))throw new Error('Некорректные связи папок');config.layout.rootLinks=rootLinks(config);}
  const groups=new Set(['@root',...config.groups.map(g=>g.id)]);
  config.layout.branches=Object.fromEntries(Object.entries(config.layout.branches).filter(([id])=>groups.has(id)));
  for(const branch of Object.values(config.layout.branches))for(const [key,min,max] of [['length',0,800],['radius',40,800],['angle',-Math.PI,Math.PI]]) {
    if(key in branch&&(!Number.isFinite(branch[key])||branch[key]<min||branch[key]>max))throw new Error('Некорректная геометрия ветви');
  }
  const ids=new Set([...config.groups.map(item=>item.id),...config.apps.map(item=>item.id)]);
  config.layout.nodes=Object.fromEntries(Object.entries(config.layout.nodes||{}).filter(([id])=>ids.has(id)&&!isPositionFixed({id})));
  for(const point of Object.values(config.layout.nodes)) {
    if(!Number.isFinite(point.x)||!Number.isFinite(point.y)||Math.abs(point.x)>2400||Math.abs(point.y)>2400)throw new Error('Некорректные координаты элемента паутины');
  }
  validateHierarchy(config);
  return config;
}
export function initialConfig() {
  return normalizeConfig({version:1,groups:[{id:'sys-personal',name:'Свои программы',parentId:SYSTEM_ID,position:7}],apps:[{id:'personal-taskmgr',name:'Диспетчер задач',groupId:'sys-personal',path:'taskmgr.exe',position:1}]});
}
