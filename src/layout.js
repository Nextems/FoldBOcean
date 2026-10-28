import {rootLinks} from './root-links.js';
import { isFixed, isPositionFixed } from './config.js';

export const CARD = { width: 120, height: 36 };
export const interactionDefaults = { hoverDelayMs: 180, closeDelayMs: 350, corridorWidth: 32, branchLength: 220 };
export function polar(position, radius) {
  const angle = -Math.PI / 2 + (position - 1) * Math.PI * 2 / 7;
  return { x: Math.cos(angle) * radius, y: Math.sin(angle) * radius };
}
export function localPosition(config, item, root = false,index = 0,count = 7) {
  const point = !isPositionFixed(item) && config.layout?.nodes?.[item.id];
  const parent=root?'@root':('path' in item?item.groupId:item.parentId);
  const radius=config.layout?.branches?.[parent]?.radius??(root?185:175);
  const spread=config.layout?.branches?.[parent]?.radius??Math.max(radius,count>1?(CARD.width+16)/(2*Math.sin(Math.PI/count)):radius);
  const angle=-Math.PI/2+index*Math.PI*2/Math.max(1,count);
  return point && Number.isFinite(point.x) && Number.isFinite(point.y) ? { ...point } : {x:Math.cos(angle)*spread,y:Math.sin(angle)*spread};
}
export function makeScene(config, expanded = null) {
  const nodes = [], edges = [], hubs=[];
  const preferences = { ...interactionDefaults, ...config.launcher };
  function children(parent, hub, ancestors) {
    const items = [...config.groups.filter(g => g.parentId === parent), ...config.apps.filter(a => a.groupId === parent)].sort((a, b) => a.position - b.position);
    for (const [index,item] of items.entries()) {
      const group = !('path' in item), local = localPosition(config, item, parent === null,index,items.length);
      const point = { x: hub.x + local.x, y: hub.y + local.y };
      const chain = group ? [...ancestors, item.id] : ancestors;
      nodes.push({ ...item, ...point, local, group, fixed: isPositionFixed(item), protected: isFixed(item), parent, chain });
      const origin=group&&parent!==null?nodes.find(n=>n.id===parent)||hub:hub;
      if(parent!==null)edges.push({ a:{x:origin.x,y:origin.y}, b: point, chain });
      if (group && (expanded === null || expanded.has(item.id))) {
        const settings=config.layout?.branches?.[item.id]||{};
        const direction=Math.hypot(local.x,local.y)>1?local:polar(item.position,1);
        const angle=settings.angle??Math.atan2(direction.y,direction.x);
        const length=settings.length??preferences.branchLength;
        const branch = { x: point.x + Math.cos(angle)*length, y: point.y + Math.sin(angle)*length };
        hubs.push({id:item.id,...branch,origin:point,angle,length});
        if(config.apps.some(a=>a.groupId===item.id))edges.push({ a: point, b: branch, chain });
        children(item.id, branch, chain);
      }
    }
  }
  children(null, { x: 0, y: 0 }, []);
  for(const [a,b] of rootLinks(config)){const from=nodes.find(n=>n.id===a),to=nodes.find(n=>n.id===b);if(from&&to)edges.push({a:{x:from.x,y:from.y},b:{x:to.x,y:to.y},chain:[],rootLink:true});}
  return { nodes, edges, hubs };
}
export function sceneBounds(scene, padding = 45) {
  const points = [...scene.nodes.map(n=>({x:n.x,y:n.y})),...scene.edges.flatMap(e => [e.a, e.b])];if(!points.length)points.push({x:0,y:0});
  const xs = points.map(p => p.x), ys = points.map(p => p.y);
  return { x: Math.min(...xs) - CARD.width / 2 - padding, y: Math.min(...ys) - CARD.height / 2 - padding,
    width: Math.max(...xs) - Math.min(...xs) + CARD.width + padding * 2,
    height: Math.max(...ys) - Math.min(...ys) + CARD.height + padding * 2 };
}
export function segmentDistance(point, a, b) {
  const dx = b.x - a.x, dy = b.y - a.y;
  const t = Math.max(0, Math.min(1, ((point.x - a.x) * dx + (point.y - a.y) * dy) / (dx * dx + dy * dy || 1)));
  return Math.hypot(point.x - a.x - t * dx, point.y - a.y - t * dy);
}
