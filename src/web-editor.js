import {mountRadialEditor} from './radial-editor.js';
import {mountSpiderEditor} from './spider-editor.js';
export function mountWebEditor(container,options){
 const config=options.getConfig();config.launcher??={};
 function mount(){const mode=config.launcher.view||'radial';container.innerHTML='<div class="layout-switch" aria-label="Вид лаунчера"><button data-layout="radial" class="'+(mode==='radial'?'active':'')+'">Круг</button><button data-layout="web" class="'+(mode==='web'?'active':'')+'">Паутина</button><span>Одни группы — два вида меню</span></div><div class="mode-content"></div>';
 container.querySelectorAll('[data-layout]').forEach(button=>button.onclick=async()=>{config.launcher.view=button.dataset.layout;if(await options.persist()&&container.isConnected)mount();});
 (mode==='web'?mountSpiderEditor:mountRadialEditor)(container.querySelector('.mode-content'),options);}
 mount();
}
