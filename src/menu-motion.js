import {ringRadius,ringInner,ringWidth} from './radial-layout.js';
export const OPEN_MS=450,SINK_MS=400,RETURN_MS=350;
const ease=t=>t*t*(3-2*t);
export const isSunk=(depth,count)=>depth>0&&depth<=count-4;
export function compactShift(depth,count,step=6){const hidden=Math.max(0,count-4);if(depth<=hidden||!hidden)return 0;let inner=ringRadius(0,step)+10;for(let d=hidden+1;d<depth;d++)inner+=ringWidth(d,step)+10;return inner-ringInner(depth,step);}
export const parentKeep=(sector,path)=>sector.depth+(sector.group&&path.includes(sector.item.id)?1:0);
export function motionValue(state,now){const t=ease(Math.max(0,Math.min(1,(now-state.started)/state.duration))),a=ease(Math.max(0,Math.min(1,(now-state.started)/(state.alphaDuration||state.duration))));return {zoom:state.from.zoom+(state.to.zoom-state.from.zoom)*t,alpha:state.from.alpha+(state.to.alpha-state.from.alpha)*a,shift:(state.from.shift||0)+((state.to.shift||0)-(state.from.shift||0))*t};}
export function retargetRing(state,submerged,sinkZoom,now,shift=0){
 if(state.submerged===submerged&&Math.abs((state.to.shift||0)-shift)<.001&&Math.abs(state.to.zoom-(submerged?sinkZoom:1))<.001)return state;
 const inward=submerged||(!state.submerged&&shift<(state.to.shift||0));
 return {from:motionValue(state,now),to:{zoom:submerged?sinkZoom:1,alpha:submerged?0:1,shift},started:now,duration:inward?SINK_MS:RETURN_MS,submerged};
}
export function ringReveals(svg,getDuration=()=>OPEN_MS,paintShift=()=>{},getStep=()=>6){
 const states=new Map();let raf=0;
 function update(scene){
  cancelAnimationFrame(raf);
  const now=performance.now(),present=new Set(),count=scene.panels.length,base=scene.panels[0]?.radius||130;
  for(const panel of scene.panels){
   const key=panel.id,submerged=isSunk(panel.depth,count),sinkZoom=base*.98/panel.radius,shift=compactShift(panel.depth,count,getStep());present.add(key);
   const previous=states.get(key);
   states.set(key,previous?retargetRing(previous,submerged,sinkZoom,now,shift):{from:{zoom:.94,alpha:0,shift:shift+(shift<0?ringWidth(Math.max(1,count-4),getStep())+10:0)},to:{zoom:submerged?sinkZoom:1,alpha:submerged?0:1,shift},started:now,duration:shift<0?Math.max(getDuration(),SINK_MS):getDuration(),alphaDuration:getDuration(),submerged});
  }
  for(const key of states.keys())if(!present.has(key))states.delete(key);
  for(const el of svg.querySelectorAll('[data-parent]')){
   const state=states.get(el.dataset.parent);if(!state)continue;
   el.dataset.sunk=String(state.submerged);el.style.pointerEvents=state.submerged?'none':'';el.style.opacity=state.to.alpha;el.style.transform=`scale(${state.to.zoom})`;
   const remaining=state.duration-(now-state.started);if(remaining<=0)continue;
   // Samples preserve continuous motion when an SVG rebuild or reversal interrupts it.
   const frames=Array.from({length:31},(_,i)=>{const value=motionValue(state,now+remaining*i/30);return {offset:i/30,opacity:value.alpha,transform:`scale(${value.zoom})`};});
   el.animate(frames,{duration:remaining,easing:'linear'});
  }
  const sectors=new Map(scene.sectors.map(s=>[s.item.id,s]));
  const reflow=()=>{if(!svg.isConnected)return;const time=performance.now();let moving=false;for(const el of svg.querySelectorAll('[data-parent]')){const state=states.get(el.dataset.parent),sector=sectors.get(el.dataset.sectorId);if(!state||!sector)continue;paintShift(el,sector,motionValue(state,time).shift);if(Math.abs((state.to.shift||0)-(state.from.shift||0))>.001&&time-state.started<state.duration)moving=true;}if(moving)raf=requestAnimationFrame(reflow);};reflow();
 }
 update.interactive=parent=>{const state=states.get(parent);return !state||!state.submerged&&motionValue(state,performance.now()).alpha>=.5;};
 update.restart=()=>states.clear();
 update.value=parent=>{const state=states.get(parent);return state?motionValue(state,performance.now()):{zoom:1,alpha:1,shift:0};};
 return update;
}
export function revealWeb(svg,duration=OPEN_MS){const drawing=svg.querySelector('#web-drawing');if(!drawing)return;drawing.getAnimations().forEach(animation=>animation.cancel());drawing.animate([{opacity:0},{opacity:1}],{duration,easing:'cubic-bezier(0.4, 0, 0.2, 1)'});}

