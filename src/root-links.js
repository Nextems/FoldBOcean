export const rootGroups=config=>config.groups.filter(g=>g.parentId===null).sort((a,b)=>a.position-b.position);
export function allRootLinks(config){const roots=rootGroups(config),links=[];for(let i=0;i<roots.length;i++)for(let j=i+1;j<roots.length;j++)links.push([roots[i].id,roots[j].id]);return links;}
export function connectedRoots(config,links){const roots=rootGroups(config);if(!roots.length)return true;const seen=new Set([roots[0].id]);let changed=true;while(changed){changed=false;for(const [a,b] of links){if(seen.has(a)&&!seen.has(b)){seen.add(b);changed=true;}if(seen.has(b)&&!seen.has(a)){seen.add(a);changed=true;}}}return roots.every(g=>seen.has(g.id));}
export function rootLinks(config){
 const roots=rootGroups(config),ids=new Set(roots.map(g=>g.id));if(!Array.isArray(config.layout?.rootLinks))return allRootLinks(config);
 const seen=new Set(),links=[];for(const pair of config.layout.rootLinks){if(!Array.isArray(pair)||pair.length!==2)continue;const [a,b]=pair,key=[a,b].sort().join('\0');if(a===b||!ids.has(a)||!ids.has(b)||seen.has(key))continue;seen.add(key);links.push([a,b]);}
 // Newly created folders and deletion of an intermediate folder must not leave an isolated root.
 if(roots.length){const reached=new Set([roots[0].id]);const visit=()=>{let changed=true;while(changed){changed=false;for(const [a,b] of links){if(reached.has(a)&&!reached.has(b)){reached.add(b);changed=true;}if(reached.has(b)&&!reached.has(a)){reached.add(a);changed=true;}}}};visit();for(const root of roots)if(!reached.has(root.id)){links.push([roots[0].id,root.id]);visit();}}
 return links;
}
export function uniqueRootLinks(links){const seen=new Set();return links.filter(([a,b])=>{const key=[a,b].sort().join('\0');if(a===b||seen.has(key))return false;seen.add(key);return true;});}
