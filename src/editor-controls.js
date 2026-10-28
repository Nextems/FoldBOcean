import {branchSettings,childrenOf,freezeDirection,branchPresetPositions,presetPositions,suggestedSpacing,setRadius,placeWorld} from './editor-geometry.js';

const esc=value=>String(value).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
export function appendLayoutTools(properties,{getConfig,getScene,getSelection,draw,persist}) {
  const section=document.createElement('section');section.id='layout-tools';properties.append(section);
  const chosen=getScene().nodes.filter(n=>getSelection().has(n.id));
  let target=chosen.length>1?'@selection':chosen.length===1?(chosen[0].group?chosen[0].id:chosen[0].parent??'@root'):'@root';
  function render() {
    const config=getConfig(),scene=getScene(),selection=getSelection();
    const selected=scene.nodes.filter(n=>selection.has(n.id));
    const items=target==='@selection'?selected:childrenOf(config,target);
    const count=items.length,settings=branchSettings(config,target);
    section.innerHTML=`<h3>Размещение ветки</h3><label>Что разместить<select id="arrange-target"><option value="@root">Корневые группы</option>${selected.length>1?`<option value="@selection">Выделенные (${selected.length})</option>`:''}${config.groups.map(g=>`<option value="${esc(g.id)}">${esc(g.name)}</option>`).join('')}</select></label>${target!=='@selection'?`<form id="branch-geometry">${target!=='@root'?`<label>Палка от группы, px<input name="stem" type="number" min="0" max="800" value="${Math.round(settings.length)}" required><input id="stem-range" type="range" min="0" max="800" value="${Math.round(settings.length)}"></label>`:''}<label>Расстояние до блоков, px<input name="radius" type="number" min="40" max="800" value="${Math.round(settings.radius)}" required></label><button type="submit">Применить расстояния</button></form><p class="editor-note">Точки соединений можно двигать свободно. Пресеты строятся от точки соединения: Y вдоль ветки, X поперёк. Автоповорот направляет ветви к дочерним блокам; Shift при перетаскивании точки двигает и блоки.</p>`:''}<h3>Пресет для ${count} элементов</h3><form id="preset-form"><label>Расположение<select name="pattern"><option value="auto">Авто · по направлению ветки</option><option value="star">Звезда</option><option value="row">Ряд</option><option value="column">Колонка</option><option value="arc">Дуга</option></select></label><label>Радиус / шаг, px<input name="spacing" type="number" min="40" max="800" value="${suggestedSpacing(Math.max(1,count),'star')}" required></label><button type="submit" ${count<1||count>13?'disabled':''}>Разложить ${count} элементов</button></form>${count<1||count>13?'<p>Пресет применяется к 1–13 элементам.</p>':''}`;
    const selector=section.querySelector('#arrange-target');selector.value=target;
    selector.onchange=()=>{target=selector.value;render();};
    const geometry=section.querySelector('#branch-geometry');
    if(geometry) {
      const stem=geometry.elements.namedItem('stem'),radius=geometry.elements.namedItem('radius'),range=section.querySelector('#stem-range');
      const apply=()=>{
        if(stem&&stem.checkValidity()){const branch=config.layout.branches[target]??={};branch.length=Number(stem.value);branch.manual=true;}
        if(radius.checkValidity()&&Number(radius.value)!==branchSettings(config,target).radius)setRadius(config,target,Number(radius.value));
        draw();
      };
      if(stem){stem.oninput=()=>{range.value=stem.value;apply();};range.oninput=()=>{stem.value=range.value;apply();};range.onchange=()=>persist();stem.onchange=()=>persist();}
      radius.oninput=apply;radius.onchange=()=>persist();
      geometry.onsubmit=async event=>{event.preventDefault();apply();await persist();};
    }
    const form=section.querySelector('#preset-form');
    form.elements.pattern.onchange=()=>form.elements.spacing.value=suggestedSpacing(Math.max(1,count),form.elements.pattern.value);
    form.onsubmit=async event=>{
      event.preventDefault();if(count<1||count>13)return;
      const spacing=Number(form.elements.spacing.value),pattern=form.elements.pattern.value;
      const hub=scene.hubs.find(h=>h.id===target);
      const points=hub?branchPresetPositions(count,pattern,spacing,hub.angle):presetPositions(count,pattern==='auto'?'star':pattern,spacing);
      if(target==='@selection') {
        const center={x:selected.reduce((sum,n)=>sum+n.x,0)/count,y:selected.reduce((sum,n)=>sum+n.y,0)/count};
        placeWorld(config,new Map(selected.map((node,index)=>[node.id,{x:center.x+points[index].x,y:center.y+points[index].y}])));
      }else {
        for(const item of items){const node=scene.nodes.find(n=>n.id===item.id);freezeDirection(config,node);}
        items.forEach((item,index)=>config.layout.nodes[item.id]=points[index]);
        const branch=config.layout.branches[target]??={};branch.radius=spacing;if(hub)branch.manual=true;
      }
      draw();await persist();
    };
  }
  render();
}
