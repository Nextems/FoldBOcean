import {chromium} from 'playwright';
import {readFile} from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
import assert from 'node:assert/strict';
const fixture=JSON.parse(await readFile(new URL('../release/radial-geometry.json',import.meta.url),'utf8'));
const browser=await chromium.launch({channel:'msedge',headless:true});
try{
  const settle=()=>page.waitForFunction(()=>[...document.querySelectorAll('.radial-sector')].every(el=>el.getAnimations().length===0));
  const page=await browser.newPage({viewport:{width:1600,height:1000}}),errors=[];page.on('pageerror',e=>errors.push(e.message));await page.goto('http://127.0.0.1:1420');
  const geometry=await page.evaluate(async data=>{const {makeRadialScene,hitSector,ringWidth}=await import('/src/radial-layout.js');const scene=makeRadialScene(data.config,data.path);if(hitSector(scene,{x:0,y:0})!==-1)throw Error('Center is interactive');if(ringWidth(2)!==ringWidth(5))throw Error('Size shrinks after third ring');for(let count=1;count<=7;count++){const config={groups:Array.from({length:count},(_,i)=>({id:String(i),name:String(i),parentId:null,position:i+1})),apps:[]};const test=makeRadialScene(config);if(test.sectors.length!==count||test.sectors.some(s=>Math.abs(s.sweep-Math.PI*2/count)>1e-6))throw Error('Incorrect share');}return scene.sectors.map(s=>({id:s.item.id,parent:s.parent,depth:s.depth,x:s.x,y:s.y,inner:s.inner,radius:s.radius,start:s.start,sweep:s.sweep,color:s.color}));},fixture);
  assert.equal(geometry.length,fixture.sectors.length);for(let i=0;i<geometry.length;i++)for(const key of Object.keys(geometry[i]))if(typeof geometry[i][key]==='number')assert.ok(Math.abs(geometry[i][key]-fixture.sectors[i][key])<0.001,key);else assert.equal(geometry[i][key],fixture.sectors[i][key]);
  await page.evaluate(()=>localStorage.setItem('spiderweb-preview',JSON.stringify({version:1,groups:[{id:'work',name:'Работа',parentId:null,position:2},{id:'play',name:'Игры',parentId:null,position:3},{id:'sys-personal',name:'Свои',parentId:'sys-prog',position:7},{id:'deep',name:'Глубже',parentId:'sys-personal',position:1},{id:'deep2',name:'Ещё',parentId:'deep',position:1}],apps:[{id:'one',name:'Заметки',path:'one.exe',groupId:'work',position:1},{id:'two',name:'Почта',path:'two.exe',groupId:'work',position:2},{id:'last',name:'Редактор',path:'last.exe',groupId:'deep2',position:1}]})));
  await page.reload();await page.locator('#web-tab').click();await settle();assert.equal(await page.locator('[data-depth="0"]').count(),3);
  {const b=await page.locator('[data-sector-id="sys-prog"] .sector-label').boundingBox();await page.mouse.click(b.x+b.width/2,b.y+b.height/2);}assert.equal(await page.locator('[data-depth="0"]').count(),3);assert.equal(await page.locator('[data-depth="1"]').count(),7);
  await page.locator('#ring-group').selectOption('deep2');assert.ok(await page.locator('[data-depth="4"]').count());assert.equal(await page.locator('[data-depth="0"]').count(),3);
  await page.locator('#ring-back').click();assert.equal(await page.locator('[data-depth="4"]').count(),0);
  await page.locator('#ring-group').selectOption('work');await settle();{const b=await page.locator('[data-sector-id="one"] .sector-label').boundingBox();await page.mouse.click(b.x+b.width/2,b.y+b.height/2);}
  {const a=await page.locator('[data-sector-id="one"] .sector-label').boundingBox(),b=await page.locator('[data-sector-id="two"] .sector-label').boundingBox();await page.mouse.move(a.x+a.width/2,a.y+a.height/2);await page.mouse.down();await page.mouse.move(b.x+b.width/2,b.y+b.height/2,{steps:8});await page.mouse.up();}
  await page.waitForFunction(()=>JSON.parse(localStorage.getItem('spiderweb-preview')).apps.find(i=>i.id==='one').position===2);await page.locator('#sector-prev').click();
  await page.locator('#sector-color').evaluate(el=>{el.value='#9b6478';el.dispatchEvent(new Event('input',{bubbles:true}));el.dispatchEvent(new Event('change',{bubbles:true}));});
  await page.waitForFunction(()=>JSON.parse(localStorage.getItem('spiderweb-preview')).radial.colors.one==='#9b6478');
  await page.locator('#sector-next').click();let saved=await page.evaluate(()=>JSON.parse(localStorage.getItem('spiderweb-preview')));assert.equal(saved.apps.find(i=>i.id==='one').position,2);
  await page.locator('#radial-color').evaluate(el=>{el.value='#65899a';el.dispatchEvent(new Event('input',{bubbles:true}));el.dispatchEvent(new Event('change',{bubbles:true}));});
  await page.waitForFunction(()=>JSON.parse(localStorage.getItem('spiderweb-preview')).radial.color==='#65899a');saved=await page.evaluate(()=>JSON.parse(localStorage.getItem('spiderweb-preview')));assert.deepEqual(saved.radial.colors,{});
  await page.reload();await page.locator('#web-tab').click();await page.locator('#ring-group').selectOption('deep2');await page.screenshot({path:fileURLToPath(new URL('../FoldBOcean-preview.png',import.meta.url))});
  await page.locator('[data-side="interaction"]').click();assert.ok(await page.locator('[name="monitor"]').count());assert.equal(await page.locator('[name="branchLength"]').count(),0);assert.deepEqual(errors,[]);
  console.log('PASS: equal sectors 1–7, empty centers, retained concentric rings, size floor after layer 3, native/editor geometry parity, nested navigation, reorder, colors, persistence and interaction settings.');
}finally{await browser.close();}
