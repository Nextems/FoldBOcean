import {chromium} from 'playwright';
import assert from 'node:assert/strict';
const browser=await chromium.launch({channel:'msedge',headless:true});
try {
  const page=await browser.newPage({viewport:{width:1600,height:1000}}),errors=[];
  page.on('pageerror',e=>errors.push(e.message));await page.goto('http://127.0.0.1:1420');
  await page.evaluate(async()=>{
    const {initialConfig}=await import('/src/config.js'),{makeScene}=await import('/src/layout.js'),{autoRoute,moveHub,branchPresetPositions}=await import('/src/editor-geometry.js');
    const config=initialConfig();config.groups.push({id:'work',name:'Работа',parentId:null,position:2});config.apps.push({id:'a',name:'А',groupId:'work',path:'a.exe',position:1},{id:'b',name:'Б',groupId:'work',path:'b.exe',position:2});
    config.layout.nodes.work={x:100,y:0};config.layout.branches.work={angle:0,length:165};config.layout.nodes.a={x:0,y:-80};config.layout.nodes.b={x:0,y:80};
    const before=makeScene(config);autoRoute(config);const after=makeScene(config);
    for(const original of before.nodes){const node=after.nodes.find(n=>n.id===original.id);if(Math.hypot(node.x-original.x,node.y-original.y)>0.001)throw Error('Routing moved a block');}
    const stable=JSON.stringify(config);autoRoute(config);if(JSON.stringify(config)!==stable)throw Error('Routing is not stable');
    const hub=after.hubs.find(h=>h.id==='work');moveHub(config,after,'work',{x:hub.x+35,y:hub.y+45});const manual=makeScene(config);
    for(const original of after.nodes){const node=manual.nodes.find(n=>n.id===original.id);if(Math.hypot(node.x-original.x,node.y-original.y)>0.001)throw Error('Manual hub movement moved a block');}
    for(let n=1;n<=7;n++)for(const pattern of ['auto','star','row','column','arc'])for(const angle of [0,Math.PI/2,Math.PI,-Math.PI/2]){const points=branchPresetPositions(n,pattern,100,angle);if(points.length!==n||points.some(p=>!Number.isFinite(p.x)||!Number.isFinite(p.y)))throw Error('Invalid local preset');}
    const side=branchPresetPositions(2,'auto',100,0),down=branchPresetPositions(2,'auto',100,Math.PI/2);
    if(side.some(p=>Math.abs(p.x)>0.001)||down.some(p=>Math.abs(p.y)>0.001))throw Error('Pair is not perpendicular to its stem');
    localStorage.setItem('spiderweb-preview',JSON.stringify(config));
  });
  await page.reload();await page.locator('#web-tab').click();await page.locator('[data-layout="web"]').click();
  assert.equal(await page.locator('[data-hub-id]').count(),await page.evaluate(()=>JSON.parse(localStorage.getItem('spiderweb-preview')).groups.length));
  await page.locator('[data-node-id="work"]').click();assert.ok(await page.locator('.local-axes').count());
  await page.locator('#preset-form [name=pattern]').selectOption('auto');await page.locator('#preset-form button[type=submit]').click();
  const labels=await page.locator('.local-axes text').allTextContents();assert.ok(labels.includes('X')&&labels.includes('Y'));
  await page.reload();await page.locator('#web-tab').click();await page.locator('[data-layout="web"]').click();assert.ok(await page.locator('#auto-route').isChecked());assert.deepEqual(errors,[]);
  console.log('PASS: automatic and manual junction movement preserves every block; presets 1–7 use rotated local axes; local axes and auto mode render and persist.');
}finally{await browser.close();}
