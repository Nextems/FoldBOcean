import {chromium} from 'playwright';
import {execFileSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
import assert from 'node:assert/strict';
const browser=await chromium.launch({channel:'msedge',headless:true});
try{
 const page=await browser.newPage({viewport:{width:1600,height:1000}}),errors=[];page.on('pageerror',e=>errors.push(e.message));await page.goto('http://127.0.0.1:1420');
 const metrics=await page.evaluate(async()=>{const m=await import('/src/radial-layout.js');return {capacity:[0,1,2,3].map(m.ringCapacity),limits:[0,1,2,3].map(m.labelLimit)};});assert.deepEqual(metrics.capacity,[5,7,13,13]);assert.deepEqual(metrics.limits,[8,12,10,10]);
 await page.locator('#web-tab').click();
 const add=async kind=>{await page.locator('#ring-add').click();await page.locator(`[data-add-kind="${kind}"]`).click();};
 await add('group');await page.locator('#editor [name="name"]').fill('ABCDEFGHIJKL');await page.locator('#editor button[type="submit"]').click();
 assert.equal(await page.locator('.radial-sector').filter({has:page.locator('title',{hasText:'ABCDEFGHIJKL'})}).locator('textPath').textContent(),'ABCDE...' );
 await add('app');assert.ok(await page.locator('#editor [name="path"]').count());await page.locator('#editor [name="name"]').fill('123456789012');await page.locator('#editor [name="path"]').fill('C:\\Apps\\Example.exe');await page.locator('#editor button[type="submit"]').click();
 await page.locator('#ring-group').selectOption('sys-personal');
 const config=()=>page.evaluate(()=>JSON.parse(localStorage.getItem('spiderweb-preview')));
 let data=await config();const app=data.apps.find(a=>a.name==='123456789012');assert.ok(app);assert.equal(app.groupId,'sys-personal');assert.equal(await page.locator(`[data-sector-id="${app.id}"] textPath`).textContent(),'1234567...');
 assert.ok((await page.locator('[data-depth="1"] .sector-outline').first().getAttribute('d')).includes('Q'));
 await page.locator('#rounded-sectors').uncheck();await page.waitForFunction(()=>JSON.parse(localStorage.getItem('spiderweb-preview')).radial.rounded===false);assert.ok(!(await page.locator('[data-depth="1"] .sector-outline').first().getAttribute('d')).includes('Q'));
 await page.locator('#rounded-sectors').check();
 for(const [id,value] of [['sector-roundness','12'],['radial-blur','20']])await page.locator('#'+id).evaluate((el,v)=>{el.value=v;el.dispatchEvent(new Event('input',{bubbles:true}));el.dispatchEvent(new Event('change',{bubbles:true}));},value);
 await page.waitForFunction(()=>JSON.parse(localStorage.getItem('spiderweb-preview')).radial.blur===20);data=await config();assert.equal(data.radial.roundness,12);
 await page.reload();await page.locator('#web-tab').click();assert.equal(await page.locator('#radial-blur').inputValue(),'20');assert.equal(await page.locator('#sector-roundness').inputValue(),'12');
 const icons=JSON.parse(execFileSync(fileURLToPath(new URL('../release/FoldBOcean.exe',import.meta.url)),['--export-icons'],{encoding:'utf8'}));assert.ok(icons['@folder']);
 await page.evaluate(async ({icons,data})=>{for(const [key,value] of Object.entries(icons)){if(key==='@missing'){if(!value||typeof value!=='object')throw Error('Bad availability metadata');continue;}const image=await createImageBitmap(await (await fetch(value)).blob());if(image.width!==48||image.height!==48)throw Error('Icon is not high resolution');const canvas=document.createElement('canvas');canvas.width=canvas.height=48;const g=canvas.getContext('2d');g.drawImage(image,0,0);const pixels=g.getImageData(0,0,48,48).data;let clear=0,visible=0;for(let i=3;i<pixels.length;i+=4){if(pixels[i]===0)clear++;else visible++;}if(!visible||key==='@folder'&&!clear)throw Error('Icon alpha is broken');}
 const {mountWebEditor}=await import('/src/web-editor.js');document.body.innerHTML='<div id="icon-preview" style="height:100vh;display:flex;flex-direction:column"></div>';mountWebEditor(document.querySelector('#icon-preview'),{getConfig:()=>data,persist:async()=>true,launch:async()=>{},edit:()=>{},status:()=>{},getIcons:async()=>icons});document.querySelector('.web-editor').style.cssText='flex:1;min-height:0';}, {icons,data});
 await page.locator('#ring-group').selectOption('sys-prog');await page.waitForFunction(()=>document.querySelectorAll('.radial-sector image').length>2);await page.screenshot({path:fileURLToPath(new URL('../FoldBOcean-readable-preview.png',import.meta.url))});assert.deepEqual(errors,[]);
 console.log('PASS: arc labels, 8/12/10 limits, 5/7/13 capacities, center Add group/app, rounding on/off/radius, blur persistence, and 48px icons with valid alpha.');
}finally{await browser.close();}
