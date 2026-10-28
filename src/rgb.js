export const rgbSettings=config=>({radial:false,blocks:false,branches:false,speed:1,...config.rgb});
export function rgbColor(id,time,speed=1){
 let hash=2166136261;for(const c of new TextEncoder().encode(id))hash=Math.imul(hash^c,16777619)>>>0;
 const h=(time*.024*speed+hash%360)%360,x=1-Math.abs(h/60%2-1),rgb=h<60?[1,x,0]:h<120?[x,1,0]:h<180?[0,1,x]:h<240?[0,x,1]:h<300?[x,0,1]:[1,0,x];
 return '#'+rgb.map(c=>Math.round((.35+.65*c)*255).toString(16).padStart(2,'0')).join('');
}
export function appendRgbControls(container,{getConfig,draw,persist}){
 const config=getConfig();config.rgb=rgbSettings(config);const section=document.createElement('section');section.className='rgb-controls';
 section.innerHTML='<h3>RGB · переливание цветов</h3>'+[['radial','Круговое меню'],['blocks','Блоки паутины'],['branches','Веточки паутины']].map(([key,label])=>`<label class="rgb-toggle"><input id="rgb-${key}" type="checkbox" ${config.rgb[key]?'checked':''}> ${label}</label>`).join('')+`<label>Скорость: <output>${config.rgb.speed}×</output><input id="rgb-speed" type="range" min="0.2" max="3" step="0.1" value="${config.rgb.speed}"></label><p>У элементов разные оттенки. После отключения вернутся выбранные цвета.</p>`;
 container.append(section);
 for(const key of ['radial','blocks','branches'])section.querySelector('#rgb-'+key).onchange=async e=>{config.rgb[key]=e.target.checked;draw();await persist();};
 const speed=section.querySelector('#rgb-speed');speed.oninput=()=>{config.rgb.speed=Number(speed.value);section.querySelector('output').value=speed.value+'×';draw();};speed.onchange=()=>persist();
}
// Animate only paint properties, without rebuilding geometry or intercepting drag.
export function rgbPreview(svg,getConfig,mode){
 let timer=0;
 function refresh(){
  clearTimeout(timer);timer=0;if(!svg.isConnected)return;
  const config=getConfig(),rgb=rgbSettings(config),enabled=mode==='radial'?rgb.radial:rgb.blocks||rgb.branches;
  if(!enabled)return;
  if(!document.hidden){const time=performance.now();
   if(mode==='radial')svg.querySelectorAll('[data-sector-id]').forEach(el=>{const color=rgbColor(el.dataset.sectorId,time,rgb.speed);el.querySelector('.sector-outline').style.stroke=color;el.querySelector('.sector-glass').style.background=color+(el.classList.contains('chosen')?'47':'24');});
   else {
    if(rgb.blocks)svg.querySelectorAll('[data-node-id]').forEach(el=>{const color=rgbColor(el.dataset.nodeId,time,rgb.speed),n=parseInt(color.slice(1),16),values=[n>>16,n>>8&255,n&255].map(c=>Math.round(18+c*.22));el.querySelector('.web-material div').style.background=`rgba(${values.join(',')},${config.webStyle?.blur?0.75:1})`;el.querySelector('.web-border').style.stroke=color;});
    if(rgb.branches)svg.querySelectorAll('.web-connection').forEach((el,i)=>el.style.stroke=rgbColor('edge-'+i,time,rgb.speed));
   }
  }
  timer=setTimeout(refresh,document.hidden?500:33);
 }
 return refresh;
}
