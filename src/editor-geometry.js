import {CARD,makeScene,polar} from './layout.js';

export const clamp=value=>Math.max(-2400,Math.min(2400,value));
export function branchSettings(config,id) {
  return {length:config.launcher?.branchLength??220,radius:id==='@root'?185:175,...config.layout.branches?.[id]};
}
export function childrenOf(config,parent) {
  return [...config.groups.filter(g=>g.parentId===(parent==='@root'?null:parent)),...config.apps.filter(a=>a.groupId===parent)].sort((a,b)=>a.position-b.position);
}
export function freezeDirection(config,node) {
  if(!node.group)return;
  const branch=config.layout.branches[node.id]??={};
  const direction=Math.hypot(node.local.x,node.local.y)>1?node.local:polar(node.position,1);
  branch.angle??=Math.atan2(direction.y,direction.x);
}
export function moveHub(config,scene,id,target,moveChildren=false) {
  const hub=scene.hubs.find(h=>h.id===id);if(!hub)return false;
  const dx=target.x-hub.origin.x,dy=target.y-hub.origin.y,length=Math.min(800,Math.hypot(dx,dy)),angle=Math.atan2(dy,dx);
  const next={x:hub.origin.x+Math.cos(angle)*length,y:hub.origin.y+Math.sin(angle)*length};
  const children=scene.nodes.filter(n=>n.parent===id);
  if(!moveChildren&&children.some(n=>Math.abs(n.local.x+hub.x-next.x)>2400||Math.abs(n.local.y+hub.y-next.y)>2400))return false;
  if(!moveChildren)for(const node of children){freezeDirection(config,node);config.layout.nodes[node.id]={x:node.local.x+hub.x-next.x,y:node.local.y+hub.y-next.y};}
  Object.assign(config.layout.branches[id]??={}, {length,angle});return true;
}
export function autoRoute(config) {
  if(config.launcher?.autoRoute===false)return;
  const groups=makeScene(config).nodes.filter(n=>n.group).reverse();
  for(const group of groups){
    if(config.layout.branches[group.id]?.manual)continue;
    const scene=makeScene(config),hub=scene.hubs.find(h=>h.id===group.id),children=scene.nodes.filter(n=>n.parent===group.id);
    if(!hub||!children.length)continue;
    const center={x:children.reduce((s,n)=>s+n.x,0)/children.length,y:children.reduce((s,n)=>s+n.y,0)/children.length};
    const target={x:hub.origin.x+(center.x-hub.origin.x)*0.6,y:hub.origin.y+(center.y-hub.origin.y)*0.6};
    if(Math.hypot(target.x-hub.x,target.y-hub.y)>0.01)moveHub(config,scene,group.id,target);
  }
}
export function movementRoots(scene,ids) {
  return scene.nodes.filter(n=>ids.has(n.id)&&!n.chain.some(id=>id!==n.id&&ids.has(id)));
}
export function snapDelta(scene,ids,delta,tolerance,enabled=true) {
  const roots=movementRoots(scene,ids),rootIds=new Set(roots.map(n=>n.id));
  const moving=scene.nodes.filter(n=>ids.has(n.id));
  const stationary=scene.nodes.filter(n=>!rootIds.has(n.id)&&!n.chain.some(id=>rootIds.has(id)));
  const result={...delta,guides:[]};
  for(const axis of ['x','y']) {
    let best=null;
    for(const a of moving)for(const b of stationary){const distance=b[axis]-(a[axis]+delta[axis]);if(Math.abs(distance)<(best?Math.abs(best.distance):tolerance))best={a,b,distance};}
    if(best&&(enabled||Math.abs(best.distance)<0.5)) {
      if(enabled)result[axis]+=best.distance;
      const other=axis==='x'?'y':'x';
      result.guides.push({axis,value:best.b[axis],start:Math.min(best.a[other]+delta[other],best.b[other])-50,end:Math.max(best.a[other]+delta[other],best.b[other])+50});
    }
  }
  for(const axis of ['x','y']) {
    const lower=Math.max(...roots.map(n=>-2400-n.local[axis])),upper=Math.min(...roots.map(n=>2400-n.local[axis]));
    const aligned=result[axis];result[axis]=Math.max(lower,Math.min(upper,result[axis]));
    if(Math.abs(aligned-result[axis])>0.001)result.guides=result.guides.filter(g=>g.axis!==axis);
  }
  return result;
}
export function moveRoots(config,roots,delta) {
  if(!roots.length)return;
  const safe={...delta};
  for(const axis of ['x','y'])safe[axis]=Math.max(Math.max(...roots.map(n=>-2400-n.local[axis])),Math.min(Math.min(...roots.map(n=>2400-n.local[axis])),safe[axis]));
  for(const node of roots){freezeDirection(config,node);config.layout.nodes[node.id]={x:node.local.x+safe.x,y:node.local.y+safe.y};}
}
export function placeWorld(config,targets) {
  const order=makeScene(config).nodes.filter(n=>targets.has(n.id)).map(n=>n.id);
  for(const id of order){const node=makeScene(config).nodes.find(n=>n.id===id),point=targets.get(id);freezeDirection(config,node);config.layout.nodes[id]={x:clamp(point.x-node.x+node.local.x),y:clamp(point.y-node.y+node.local.y)};}
}
export function presetPositions(count,pattern,spacing) {
  return Array.from({length:count},(_,index)=>{
    if(pattern==='row')return{x:(index-(count-1)/2)*spacing,y:0};
    if(pattern==='column')return{x:0,y:(index-(count-1)/2)*spacing};
    const angle=pattern==='arc'?(count===1?-Math.PI/2:-Math.PI+index*Math.PI/(count-1)):(count===2?Math.PI+index*Math.PI:-Math.PI/2+index*Math.PI*2/count);
    return{x:Math.round(Math.cos(angle)*spacing),y:Math.round(Math.sin(angle)*spacing)};
  });
}
export function branchPresetPositions(count,pattern,spacing,angle) {
  const actual=pattern==='auto'?'star':pattern;
  if(actual==='star'&&count===1)return [{x:Math.cos(angle)*spacing,y:Math.sin(angle)*spacing}];
  const rotation=angle+(actual==='arc'||actual==='star'&&count>2?Math.PI/2:-Math.PI/2);
  const cos=Math.cos(rotation),sin=Math.sin(rotation);
  return presetPositions(count,actual,spacing).map(p=>({x:p.x*cos-p.y*sin,y:p.x*sin+p.y*cos}));
}
export function suggestedSpacing(count,pattern) {
  if(pattern==='row')return CARD.width+24;
  if(pattern==='column')return CARD.height+24;
  if(count<=1)return 80;
  const angle=pattern==='arc'?Math.PI/(2*Math.max(1,count-1)):Math.PI/count;
  return Math.ceil(Math.max(80,(CARD.width+16)/(2*Math.sin(angle)))/10)*10;
}
export function setRadius(config,parent,radius) {
  const previous=branchSettings(config,parent).radius;
  const scene=makeScene(config);
  for(const node of scene.nodes.filter(n=>(n.parent??'@root')===parent)) {
    freezeDirection(config,node);
    config.layout.nodes[node.id]={x:clamp(node.local.x*radius/previous),y:clamp(node.local.y*radius/previous)};
  }
  (config.layout.branches[parent]??={}).radius=radius;
}
