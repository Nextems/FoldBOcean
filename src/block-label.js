import {menuName} from './config.js';
const esc=value=>String(value).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const measure=document.createElement('canvas').getContext('2d');
export const missingMessage=name=>'Программа\n«'+name+'»\nне найдена :(';
export const blockLabel=(item,mode,missing=false)=>missing?(mode==='radial'?missingMessage(item.name):missingMessage(item.name).replaceAll('\n',' ')):menuName(item.name,mode==='radial'?20:15);
export function fitWrappedLabel(value,width,height,maximum=13){
  let lines=[],size=maximum;
  for(;size>=.5;size-=.25){
    measure.font='600 '+size+'px Segoe UI';lines=[];
    for(const paragraph of value.split('\n')){
      let line='';
      for(const word of paragraph.split(/\s+/u)){
        if(!word)continue;
        if(measure.measureText(line?line+' '+word:word).width<=width){line=line?line+' '+word:word;continue;}
        if(line){lines.push(line);line='';}
        for(const glyph of Array.from(word)){if(line&&measure.measureText(line+glyph).width>width){lines.push(line);line='';}line+=glyph;}
      }
      if(line||!paragraph)lines.push(line);
    }
    if(lines.length*size*1.15<=height)break;
  }
  return {size,lines};
}
export function wrappedLabelSvg(value,box,{maximum=13,className='sector-label wrapped-label',missing=false}={}){
  const fit=fitWrappedLabel(value,box.width,box.height,maximum),height=fit.size*1.15;
  return `<text class="${className}" data-full-text="${esc(value)}" text-anchor="middle" transform="translate(${box.x} ${box.y}) rotate(${box.angle||0})" style="font-family:Segoe UI,sans-serif;font-weight:600;font-size:${fit.size}px;fill:${missing?'#ffc9c9':'#f2f5f7'};pointer-events:none">${fit.lines.map((line,index)=>`<tspan x="0" y="${(index-(fit.lines.length-1)/2)*height+fit.size*.34}">${esc(line)}</tspan>`).join('')}</text>`;
}
export function radialBlockLabelSvg(item,sector,missing=false){
  const value=blockLabel(item,'radial',missing),sweep=Math.max(.02,Math.min(sector.sweep-.09,Math.PI*.95)),height=Math.max(1,sector.radius-sector.inner-8);
  let fit;
  for(let size=13*sector.layerFactor;size>=.5;size-=.25){fit=fitWrappedLabel(value,(sector.inner+size*.75+4)*sweep,100000,size);if(fit.lines.length*size*1.15<=height)break;}
  const lower=Math.sin(sector.angle)>.1,sign=lower?-1:1,mid=(sector.inner+sector.radius)/2;
  const rows=fit.lines.map((line,index)=>{
    const radius=mid+(lower?1:-1)*(index-(fit.lines.length-1)/2)*fit.size*1.15;
    const from=sector.angle-sign*sweep/2,to=sector.angle+sign*sweep/2;
    const id='wrapped-'+item.id+'-'+index,d=`M ${Math.cos(from)*radius} ${Math.sin(from)*radius} A ${radius} ${radius} 0 0 ${lower?0:1} ${Math.cos(to)*radius} ${Math.sin(to)*radius}`;
    return `<defs><path id="${id}" d="${d}"/></defs><text class="sector-label arc-label" style="font-size:${fit.size}px;font-weight:600;fill:${missing?'#ffc9c9':'#f2f5f7'}"><textPath href="#${id}" startOffset="50%" text-anchor="middle" dominant-baseline="central">${esc(line)}</textPath></text>`;
  }).join('');
  return `<g class="wrapped-label" data-full-text="${esc(value)}">${rows}</g>`;
}
export function appendBlockLabelControl(container,{items,persist,draw}){
  if(!items.length)return;
  const label=document.createElement('label');label.className='block-label-option';
  label.innerHTML='<input type="checkbox" data-no-icon> Без значка — больше текста';
  const checkbox=label.querySelector('input');checkbox.checked=items.every(item=>item.noIcon===true);checkbox.indeterminate=items.some(item=>item.noIcon===true)&&!checkbox.checked;
  checkbox.onchange=async()=>{for(const item of items){if(checkbox.checked)item.noIcon=true;else delete item.noIcon;}draw();await persist();};
  container.append(label);const note=document.createElement('p');note.className='field-hint';note.textContent='Без значка: до 15 символов в паутине и до 20 в круге, с переносами.';container.append(note);
}
