export function webStyle(config){return {color:'#70818b',opacity:100,blur:0,...config.webStyle};}
export function webFill(style){
 const rgb=style.color.match(/[a-f\d]{2}/gi).map(c=>parseInt(c,16));
 return `rgba(${rgb.map(c=>Math.round(18+c*.22)).join(',')},${style.opacity/100})`;
}
function hue(hex){const [r,g,b]=hex.match(/[a-f\d]{2}/gi).map(c=>parseInt(c,16)/255),max=Math.max(r,g,b),min=Math.min(r,g,b),d=max-min;return d?Math.round(((max===r?(g-b)/d:max===g?(b-r)/d+2:(r-g)/d+4)*60+360)%360):0;}
function hueColor(h){const x=1-Math.abs(h/60%2-1),rgb=h<60?[1,x,0]:h<120?[x,1,0]:h<180?[0,1,x]:h<240?[0,x,1]:h<300?[x,0,1]:[1,0,x];return '#'+rgb.map(c=>Math.round((.25+c*.55)*255).toString(16).padStart(2,'0')).join('');}
export function appendWebStyleControls(container,{getConfig,draw,persist}){
 const config=getConfig();config.webStyle=webStyle(config);const style=config.webStyle;
 const section=document.createElement('section');section.className='web-material-controls';
 section.innerHTML=`<h3>Вид паутины</h3><label>Цвет блоков <input id="web-color" type="color" value="${style.color}"></label><label>Оттенок<input id="web-hue" aria-label="Оттенок паутины" type="range" min="0" max="359" value="${hue(style.color)}"></label><label>Прозрачность: <output id="web-transparency-value">${100-style.opacity}%</output><input id="web-transparency" type="range" min="0" max="100" value="${100-style.opacity}"></label><label>Размытие: <output id="web-blur-value">${style.blur} px</output><input id="web-blur" type="range" min="0" max="24" value="${style.blur}"></label><p>Цвет и прозрачность меняют фон блоков. Текст и значки остаются чёткими.</p>`;
 container.append(section);
 const color=section.querySelector('#web-color'),slider=section.querySelector('#web-hue');
 color.oninput=()=>{style.color=color.value;slider.value=hue(style.color);draw();};
 slider.oninput=()=>{style.color=hueColor(Number(slider.value));color.value=style.color;draw();};
 for(const [id,key,convert,unit] of [['transparency','opacity',v=>100-v,'%'],['blur','blur',v=>v,' px']]){const input=section.querySelector('#web-'+id);input.oninput=()=>{style[key]=convert(Number(input.value));section.querySelector('#web-'+id+'-value').value=input.value+unit;draw();};}
 section.onchange=()=>persist();
}
