import {chromium} from 'playwright';
import assert from 'node:assert/strict';

const browser=await chromium.launch({channel:'msedge',headless:true});
try {
  const page=await browser.newPage({viewport:{width:1600,height:1000}}),errors=[];
  page.on('pageerror',e=>errors.push(e.message));await page.goto('http://127.0.0.1:1420');
  await page.evaluate(()=>localStorage.setItem('spiderweb-preview',JSON.stringify({version:1,groups:[
    {id:'work',name:'Работа',parentId:null,position:2},{id:'tools',name:'Утилиты',parentId:null,position:3},{id:'nested',name:'Вложенная',parentId:'work',position:4}
  ],apps:[{id:'one',name:'Один',groupId:'work',path:'one.exe',position:1},{id:'two',name:'Два',groupId:'work',path:'two.exe',position:2},{id:'three',name:'Три',groupId:'work',path:'three.exe',position:3},{id:'deep',name:'Внутри',groupId:'nested',path:'deep.exe',position:1}],layout:{nodes:{one:{x:-100,y:0},two:{x:100,y:80},three:{x:0,y:-110}},branches:{}}})));
  await page.reload();await page.locator('#web-tab').click();await page.locator('[data-layout="web"]').click();
  const node=id=>page.locator(`[data-node-id="${id}"]`);
  const data=()=>page.evaluate(()=>JSON.parse(localStorage.getItem('spiderweb-preview')));
  const position=id=>node(id).evaluate(el=>({x:el.transform.baseVal.getItem(0).matrix.e,y:el.transform.baseVal.getItem(0).matrix.f}));
  const screen=point=>page.locator('svg').evaluate((svg,p)=>{const t=new DOMPoint(p.x,p.y).matrixTransform(svg.getScreenCTM());return{x:t.x,y:t.y};},point);
  async function drag(id,dx,dy){const p=await position(id),a=await screen(p),b=await screen({x:p.x+dx,y:p.y+dy});await page.mouse.move(a.x,a.y);await page.mouse.down();await page.mouse.move(b.x,b.y,{steps:7});await page.mouse.up();}
  await node('one').click();await node('two').click({modifiers:['Control']});assert.equal(await page.locator('.web-node.chosen').count(),2);
  const oneBefore=await position('one'),twoBefore=await position('two');await drag('one',30,15);
  const oneAfter=await position('one'),twoAfter=await position('two');
  assert.ok(Math.abs(oneAfter.x-oneBefore.x-(twoAfter.x-twoBefore.x))<0.001);assert.ok(Math.abs(oneAfter.y-oneBefore.y-(twoAfter.y-twoBefore.y))<0.001);
  await node('work').click();await node('deep').click({modifiers:['Shift']});
  const parentBefore=await position('work'),deepBefore=await position('deep');await drag('work',20,25);
  const parentAfter=await position('work'),deepAfter=await position('deep');
  assert.ok(Math.abs(parentAfter.x-parentBefore.x-(deepAfter.x-deepBefore.x))<0.001,'Selected descendants are not moved twice');
  assert.ok(Math.abs(parentAfter.y-parentBefore.y-(deepAfter.y-deepBefore.y))<0.001);
  // A rectangle intersects exactly the two lower sibling cards.
  const p1=await position('one'),p2=await position('two');
  let a=await screen({x:Math.min(p1.x,p2.x)-72,y:Math.min(p1.y,p2.y)-30});const b=await screen({x:Math.max(p1.x,p2.x)+72,y:Math.max(p1.y,p2.y)+30});
  for(let offset=80;offset<300;offset+=20){if(await page.evaluate(p=>{const el=document.elementFromPoint(p.x,p.y);return !!el?.closest('svg')&&!el.closest('[data-node-id],[data-hub-id]');},a))break;a=await screen({x:Math.min(p1.x,p2.x)-offset,y:Math.min(p1.y,p2.y)-40});}
  await page.mouse.move(a.x,a.y);await page.mouse.down();await page.mouse.move(b.x,b.y,{steps:8});await page.mouse.up();
  assert.equal(await node('one').getAttribute('class').then(c=>c.includes('chosen')),true);assert.equal(await node('two').getAttribute('class').then(c=>c.includes('chosen')),true);
  await node('one').click(); // An ordinary click reduces an existing multi-selection.
  assert.equal(await page.locator('.web-node.chosen').count(),1);
  const source=await position('one'),target=await position('two'),from=await screen(source),to=await screen({x:source.x,y:target.y-2});
  await page.mouse.move(from.x,from.y);await page.mouse.down();await page.mouse.move(to.x,to.y,{steps:6});
  assert.ok(await page.locator('.alignment-guide').count()>0,'Alignment assistant is visible during drag');
  assert.ok(Math.abs((await position('one')).y-(await position('two')).y)<0.01,'Y coordinate snaps to the other block');
  await page.screenshot({path:'editor-tools-preview.png'});await page.mouse.up();
  await node('work').click();await page.locator('#branch-geometry [name=stem]').fill('75');await page.locator('#branch-geometry button[type=submit]').click();
  assert.equal((await data()).layout.branches.work.length,75);
  const group=await position('work'),handle=page.locator('[data-hub-id="work"]');
  const box=await handle.boundingBox(),end=await screen({x:group.x+110,y:group.y});
  await page.mouse.move(box.x+box.width/2,box.y+box.height/2);await page.mouse.down();await page.mouse.move(end.x,end.y,{steps:6});await page.mouse.up();
  const branch=(await data()).layout.branches.work;assert.ok(Math.abs(branch.length-110)<2);assert.ok(Math.abs(branch.angle)<0.02);
  await page.locator('#preset-form [name=pattern]').selectOption('row');await page.locator('#preset-form [name=spacing]').fill('140');await page.locator('#preset-form button[type=submit]').click();
  let saved=await data();const peers=[...saved.apps.filter(a=>a.groupId==='work'),...saved.groups.filter(g=>g.parentId==='work')];
  assert.equal(peers.length,4);for(const item of peers)assert.ok(Math.abs(saved.layout.nodes[item.id].x)<0.001);
  const arranged=structuredClone(saved.layout.nodes);
  await page.reload();await page.locator('#web-tab').click();await page.locator('[data-layout="web"]').click();saved=await data();assert.ok(Math.abs(saved.layout.branches.work.length-110)<2);
  for(const item of peers)assert.deepEqual(saved.layout.nodes[item.id],arranged[item.id]);
  const checked=await page.evaluate(async()=>{
    const {presetPositions,snapDelta,movementRoots}=await import('/src/editor-geometry.js');
    for(let n=1;n<=7;n++)for(const pattern of ['star','arc','row','column']){
      const points=presetPositions(n,pattern,150);if(points.length!==n||new Set(points.map(p=>p.x+','+p.y)).size!==n)throw new Error('Invalid preset');
      if(pattern==='row'&&points.some(p=>p.y!==0))throw new Error('Row not aligned');if(pattern==='column'&&points.some(p=>p.x!==0))throw new Error('Column not aligned');
    }
    return true;
  });assert.equal(checked,true);
  await node('one').click();await page.keyboard.press('Control+a');assert.equal(await page.locator('.chosen').count(),await page.locator('.web-node').count());
  await page.keyboard.press('Escape');assert.equal(await page.locator('.chosen').count(),0);
  assert.deepEqual(errors,[]);console.log('PASS: Ctrl/Shift and rectangle selection, shared movement without double shifting, X/Y guides, snapping, branch handle and distance, presets 1–7, persistence and select-all.');
} finally {await browser.close();}
