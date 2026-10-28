import { chromium } from 'playwright';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';

const browser = await chromium.launch({channel:'msedge',headless:true});
try {
  const page = await browser.newPage({viewport:{width:1440,height:900}});
  const errors=[];page.on('pageerror',error=>errors.push(error.message));
  await page.goto('http://127.0.0.1:1420');
  const nativeGeometry=JSON.parse(await readFile(new URL('../release/root-links-geometry.json',import.meta.url),'utf8'));
  const browserGeometry=await page.evaluate(async config=>{const {makeScene}=await import('/src/layout.js');const scene=makeScene(config,new Set());return {nodes:scene.nodes.map(n=>({id:n.id,x:n.x,y:n.y})),edgeCount:scene.edges.length};},nativeGeometry.config);
  assert.equal(browserGeometry.edgeCount,nativeGeometry.edges.length);
  for(const expected of nativeGeometry.nodes){const actual=browserGeometry.nodes.find(n=>n.id===expected.id);assert.ok(actual);assert.ok(Math.abs(actual.x-expected.x)<0.001&&Math.abs(actual.y-expected.y)<0.001,'Editor matches native layout');}
  await page.evaluate(()=>localStorage.setItem('spiderweb-preview',JSON.stringify({version:1,groups:[
    {id:'work',name:'Работа',parentId:null,position:2},
    {id:'tools',name:'Инструменты',parentId:null,position:3},
    {id:'nested',name:'Редакторы',parentId:'work',position:1},
    {id:'personal',name:'Свои программы',parentId:'sys-prog',position:7}
  ],apps:[
    {id:'editor',name:'Редактор',groupId:'work',path:'C:\\Apps\\Editor.exe',position:2},
    {id:'paint',name:'Графика',groupId:'nested',path:'C:\\Apps\\Paint.exe',position:1},
    {id:'personal-app',name:'Моя программа',groupId:'personal',path:'C:\\Apps\\Custom.exe',position:1}
  ]})));
  await page.reload();await page.locator('#web-tab').click();await page.locator('[data-layout="web"]').click();
  const node=id=>page.locator(`[data-node-id="${id}"]`);
  const coordinates=async id=>node(id).getAttribute('transform');
  const data=()=>page.evaluate(()=>JSON.parse(localStorage.getItem('spiderweb-preview')));
  async function drag(id,dx,dy){const box=await node(id).boundingBox();await page.mouse.move(box.x+box.width/2,box.y+box.height/2);await page.mouse.down();await page.mouse.move(box.x+box.width/2+dx,box.y+box.height/2+dy,{steps:8});await page.mouse.up();}
  assert.equal(await page.locator('[data-node-id]').count(),14,'All branches, groups and applications are visible');
  const systemBefore=await coordinates('sys-prog'),appBefore=await coordinates('sys-cmd');
  const cmdLocal=await node('sys-cmd').getAttribute('data-local-x');
  await drag('sys-prog',35,20);
  assert.notEqual(await coordinates('sys-prog'),systemBefore);
  assert.notEqual(await coordinates('sys-cmd'),appBefore,'System commands follow their folder');
  assert.equal(await node('sys-cmd').getAttribute('data-local-x'),cmdLocal);
  assert.ok((await data()).layout.nodes['sys-prog']);
  await page.reload();await page.locator('#web-tab').click();await page.locator('[data-layout="web"]').click();
  assert.ok((await data()).layout.nodes['sys-prog'],'System folder position survives reload');
  await node('sys-prog').click();assert.equal(await page.locator('#edit-web-node').textContent(),'Изменить');
  const builtinIds=['sys-settings','sys-cmd','sys-regedit','sys-disks','sys-devices','sys-control'];
  for(const id of builtinIds){const before=await coordinates(id);await drag(id,14,10);assert.notEqual(await coordinates(id),before);assert.ok((await data()).layout.nodes[id]);}
  const cmdAfter=await coordinates('sys-cmd');
  await page.reload();await page.locator('#web-tab').click();await page.locator('[data-layout="web"]').click();
  for(const id of builtinIds)assert.ok((await data()).layout.nodes[id],'All system positions survive reload');
  assert.equal(await coordinates('sys-cmd'),cmdAfter);
  const descendantBefore=await coordinates('paint');
  const childLocal=await node('paint').evaluate(el=>[Number(el.dataset.localX),Number(el.dataset.localY)]);
  await drag('work',55,30);
  const movedWork=(await data()).layout.nodes.work;
  assert.ok(movedWork);assert.notEqual(await coordinates('paint'),descendantBefore,'Moving a group carries its subtree');
  const childLocalAfter=await node('paint').evaluate(el=>[Number(el.dataset.localX),Number(el.dataset.localY)]);assert.ok(childLocal.every((n,i)=>Math.abs(n-childLocalAfter[i])<0.001),'Child local coordinates remain unchanged');
  await drag('personal',-30,20);assert.ok((await data()).layout.nodes.personal,'Personal subgroup in the system branch can move');
  assert.equal(await coordinates('sys-cmd'),cmdAfter,'Personal subgroup movement leaves siblings alone');
  const personalAfter=await coordinates('personal');
  await page.reload();await page.locator('#web-tab').click();await page.locator('[data-layout="web"]').click();
  assert.equal(await coordinates('personal'),personalAfter,'Coordinates persist across reload');
  await node('work').click();
  await node('sys-cmd').click();assert.equal(await page.locator('#point-form').count(),1);
  await page.locator('[data-side="interaction"]').click();
  await page.locator('[name=closeDelayMs]').fill('800');
  await page.locator('[name=hoverDelayMs]').fill('100');
  await page.locator('#interaction-form button[type=submit]').click();
  const saved=await data();assert.equal(saved.launcher.closeDelayMs,800);assert.equal(saved.launcher.hoverDelayMs,100);
  const initialView=await page.locator('svg').getAttribute('viewBox');
  await page.locator('[data-zoom=in]').click();assert.notEqual(await page.locator('svg').getAttribute('viewBox'),initialView);
  await page.locator('#fit-web').click();
  await page.locator('[data-side="node"]').click();
  await node('personal').click();
  await page.screenshot({path:'web-editor-preview.png'});
  assert.deepEqual(errors,[]);
  console.log('PASS: full tree, movable system nodes, draggable personal subgroup, subtree movement, persistence, interaction settings and zoom.');
} finally {await browser.close();}

