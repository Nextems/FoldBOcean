const same=(a,b)=>Math.hypot(a.x-b.x,a.y-b.y)<0.01;
const toward=(a,b,distance)=>{const length=Math.hypot(b.x-a.x,b.y-a.y);return {x:a.x+(b.x-a.x)*distance/length,y:a.y+(b.y-a.y)*distance/length};};
export function roundedConnections(edges,nodes) {
  const paths=edges.map(e=>({start:e.a,end:e.b,curved:false}));
  edges.forEach((edge,index)=>{
    if(nodes.some(n=>same(n,edge.b)))return;
    const length=Math.hypot(edge.b.x-edge.a.x,edge.b.y-edge.a.y);if(length<0.01)return;
    const children=edges.map((e,i)=>({edge:e,index:i,length:Math.hypot(e.b.x-e.a.x,e.b.y-e.a.y)})).filter(e=>e.index!==index&&same(e.edge.a,edge.b)&&e.length>0.01);
    if(!children.length)return;
    const radius=Math.min(32,length*0.4,...children.map(e=>e.length*0.4));
    const start=toward(edge.b,edge.a,radius);paths[index].end=start;
    for(const child of children){
      const join=toward(edge.b,child.edge.b,radius);
      Object.assign(paths[child.index],{curved:true,start,join,c1:{x:start.x+(edge.b.x-start.x)*2/3,y:start.y+(edge.b.y-start.y)*2/3},c2:{x:join.x+(edge.b.x-join.x)*2/3,y:join.y+(edge.b.y-join.y)*2/3}});
    }
  });
  return paths;
}
export function connectionPath(path) {
  const {start,end,c1,c2,join}=path;
  return `M ${start.x} ${start.y} ${path.curved?`C ${c1.x} ${c1.y} ${c2.x} ${c2.y} ${join.x} ${join.y} `:''}L ${end.x} ${end.y}`;
}
