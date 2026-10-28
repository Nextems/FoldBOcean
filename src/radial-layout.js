import {menuName} from './config.js';
import {capacityAtDepth} from './config-validation.js';
export const TAU=Math.PI*2,RING_WIDTH=60;
export const ringScale=(depth,step=6)=>1-step/100*Math.min(depth,2);
export const ringWidth=(depth,step=6)=>RING_WIDTH*ringScale(depth,step);
export const ringInner=(depth,step=6)=>70+Array.from({length:depth},(_,d)=>ringWidth(d,step)+10).reduce((a,b)=>a+b,0);
export const ringRadius=(depth,step=6)=>ringInner(depth,step)+ringWidth(depth,step);
export const labelLimit=depth=>[8,12,10][Math.min(depth,2)];
export const labelFont=depth=>[16,15,14.5][Math.min(depth,2)];
export const ringCapacity=capacityAtDepth;
export function menuDepth(config,parent){return parent==='@root'||parent===null?0:groupPath(config,parent).length;}
export const menuCapacity=(config,parent)=>ringCapacity(menuDepth(config,parent));
export const iconSize=depth=>[28,24,20][Math.min(depth,2)];
export function iconBox(sector){const size=iconSize(sector.depth)*(sector.layerFactor??1),half=size/2,radius=Math.sqrt((sector.radius-4)**2-half**2)-half;return {size,x:sector.center.x+Math.cos(sector.angle)*radius,y:sector.center.y+Math.sin(sector.angle)*radius};}
export const ringItems=(config,parent)=>[...config.groups.filter(g=>g.parentId===(parent==='@root'?null:parent)),...config.apps.filter(a=>a.groupId===parent)].sort((a,b)=>a.position-b.position);
export function radialConfig(config){config.radial??={};config.radial.color??='#70818b';config.radial.colors??={};config.radial.rotation??={};config.radial.rounded??=true;config.radial.roundness??=6;config.radial.blur??=12;return config.radial;}
export function groupPath(config,id){const path=[],seen=new Set();while(id&&id!=='@root'&&!seen.has(id)){seen.add(id);const group=config.groups.find(g=>g.id===id);if(!group)break;path.unshift(id);id=group.parentId;}return path;}
export function makeRadialScene(config,path=[],width=1200,height=800){
  const radial=radialConfig(config),panels=[],sectors=[];
  let parent='@root',center={x:0,y:0};
  for(let depth=0;depth<=path.length;depth++){
    const layerStep=config.appearance?.ringStep??6,radius=ringRadius(depth,layerStep),inner=ringInner(depth,layerStep),items=ringItems(config,parent),rotation=radial.rotation[parent]||0;
    const panel={id:parent,depth,...center,radius,inner,scale:ringScale(depth,layerStep),items};panels.push(panel);
    const step=items.length?TAU/items.length:TAU,start=-Math.PI/2-step/2+rotation;
    const current=items.map((item,index)=>{const angle=start+(index+0.5)*step;const sector={item,index,parent,depth,layerFactor:ringScale(depth,layerStep)/ringScale(depth),center:{...center},radius,inner,start:start+index*step,sweep:step,angle,group:!('path' in item),x:center.x+Math.cos(angle)*(radius-ringWidth(depth,layerStep)/2),y:center.y+Math.sin(angle)*(radius-ringWidth(depth)/2),color:radial.colors[item.id]||radial.color};sectors.push(sector);return sector;});
    const selected=current.find(s=>s.item.id===path[depth]&&s.group);if(!selected)break;
    center={x:0,y:0};parent=selected.item.id;
  }
  return {panels,sectors};
}
export function hitSector(scene,point){for(let i=scene.sectors.length-1;i>=0;i--){const s=scene.sectors[i],x=point.x-s.center.x,y=point.y-s.center.y,r=Math.hypot(x,y);if(r<s.inner||r>s.radius)continue;const angle=((Math.atan2(y,x)-s.start)%TAU+TAU)%TAU;if(angle<s.sweep)return i;}return -1;}
export function sectorPath(s,local=false,roundness=6){
  const cx=local?s.radius:s.center.x,cy=local?s.radius:s.center.y,p=(r,a)=>`${cx+Math.cos(a)*r} ${cy+Math.sin(a)*r}`;
  if(s.sweep>=TAU-0.001)return `M ${p(s.radius,0)} A ${s.radius} ${s.radius} 0 1 1 ${p(s.radius,Math.PI)} A ${s.radius} ${s.radius} 0 1 1 ${p(s.radius,0)} Z M ${p(s.inner,0)} A ${s.inner} ${s.inner} 0 1 0 ${p(s.inner,Math.PI)} A ${s.inner} ${s.inner} 0 1 0 ${p(s.inner,0)} Z`;
  const gap=Math.min(0.018,s.sweep*0.04),a=s.start+gap,b=s.start+s.sweep-gap,large=b-a>Math.PI?1:0;
  const r=Math.max(0,Math.min(roundness,(s.radius-s.inner)/3,(b-a)*s.inner/4));
  if(r>0.01){const outer=r/s.radius,inner=r/s.inner;return `M ${p(s.radius,a+outer)} A ${s.radius} ${s.radius} 0 ${large} 1 ${p(s.radius,b-outer)} Q ${p(s.radius,b)} ${p(s.radius-r,b)} L ${p(s.inner+r,b)} Q ${p(s.inner,b)} ${p(s.inner,b-inner)} A ${s.inner} ${s.inner} 0 ${large} 0 ${p(s.inner,a+inner)} Q ${p(s.inner,a)} ${p(s.inner+r,a)} L ${p(s.radius-r,a)} Q ${p(s.radius,a)} ${p(s.radius,a+outer)} Z`;}
  return `M ${p(s.radius,a)} A ${s.radius} ${s.radius} 0 ${large} 1 ${p(s.radius,b)} L ${p(s.inner,b)} A ${s.inner} ${s.inner} 0 ${large} 0 ${p(s.inner,a)} Z`;
}
export function arcLabel(s){
  const text=menuName(s.item.name,labelLimit(s.depth)),font=labelFont(s.depth)*(s.layerFactor??1),lower=Math.sin(s.angle)>0.1,radius=s.inner+(lower?font+8:12),sign=lower?-1:1,span=Math.min(s.sweep-0.09,Math.PI*0.95),a=s.angle-sign*span/2,b=s.angle+sign*span/2;
  const point=angle=>`${s.center.x+Math.cos(angle)*radius} ${s.center.y+Math.sin(angle)*radius}`;
  return {text,font,d:`M ${point(a)} A ${radius} ${radius} 0 0 ${sign>0?1:0} ${point(b)}`,available:radius*span};
}

