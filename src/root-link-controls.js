import {rootGroups,rootLinks,allRootLinks,connectedRoots,uniqueRootLinks} from './root-links.js';
const esc=value=>String(value).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
export function appendRootLinkControls(properties,{getConfig,draw,persist,status}){
 const config=getConfig(),roots=rootGroups(config);if(roots.length<3)return;
 const section=document.createElement('section');section.id='root-link-controls';properties.append(section);
 function render(){const links=rootLinks(config),options=value=>roots.map(g=>'<option value="'+esc(g.id)+'" '+(g.id===value?'selected':'')+'>'+esc(g.name)+'</option>').join('');
  section.innerHTML='<h3>Связи основных папок</h3><p>Прямые линии между папками, без общего центра. Выберите, какие папки связать.</p><div class="root-links-presets"><button id="roots-all">Все со всеми</button><button id="roots-chain">Цепочка</button></div><div class="root-links-list">'+links.map(([a,b],i)=>'<div class="root-link-row" data-root-link="'+i+'"><select aria-label="Первая папка связи '+(i+1)+'" data-link-end="0">'+options(a)+'</select><span>↔</span><select aria-label="Вторая папка связи '+(i+1)+'" data-link-end="1">'+options(b)+'</select><button data-link-remove="'+i+'" title="Удалить связь">×</button></div>').join('')+'</div><div class="root-link-row"><select id="root-link-from" aria-label="От какой папки">'+options(roots[0].id)+'</select><span>↔</span><select id="root-link-to" aria-label="К какой папке">'+options(roots[1].id)+'</select></div><button id="root-link-add">Добавить связь</button>';
  const save=async pairs=>{if(pairs.some(([a,b])=>a===b)){status('Выберите две разные папки.',true);render();return;}const unique=uniqueRootLinks(pairs);if(!connectedRoots(config,unique)){status('Все основные папки должны оставаться связанными. Сначала добавьте другую связь.',true);render();return;}config.layout.rootLinks=unique;draw();render();await persist();};
  section.querySelector('#roots-all').onclick=()=>save(allRootLinks(config));section.querySelector('#roots-chain').onclick=()=>save(roots.slice(1).map((g,i)=>[roots[i].id,g.id]));
  section.querySelectorAll('[data-root-link] select').forEach(select=>select.onchange=()=>{const row=select.closest('[data-root-link]'),index=Number(row.dataset.rootLink),end=Number(select.dataset.linkEnd),next=links.map(p=>[...p]);next[index][end]=select.value;save(next);});
  section.querySelectorAll('[data-link-remove]').forEach(button=>button.onclick=()=>save(links.filter((_,i)=>i!==Number(button.dataset.linkRemove))));
  section.querySelector('#root-link-add').onclick=()=>save([...links,[section.querySelector('#root-link-from').value,section.querySelector('#root-link-to').value]]);
 }render();
}
