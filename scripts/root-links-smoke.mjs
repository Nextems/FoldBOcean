import {chromium} from 'playwright';
import {readFile} from 'node:fs/promises';
import assert from 'node:assert/strict';
const fixture=JSON.parse(await readFile(new URL('../release/root-links-geometry.json',import.meta.url),'utf8'));
const browser=await chromium.launch({channel:'msedge',headless:true});
try{
 const page=await browser.newPage({viewport:{width:1600,height:1000}}),errors=[];page.on('pageerror',e=>errors.push(e.message));await page.goto('http://127.0.0.1:1420');
 const geometry=await page.evaluate(async config=>{const {makeScene}=await import('/src/layout.js'),{rootLinks,connectedRoots}=await import('/src/root-links.js');const scene=makeScene(config,new Set());if(!connectedRoots(config,rootLinks(config)))throw Error('Disconnected roots');if(scene.edges.some(e=>!scene.nodes.some(n=>Math.hypot(n.x-e.a.x,n.y-e.a.y)<0.001)||!scene.nodes.some(n=>Math.hypot(n.x-e.b.x,n.y-e.b.y)<0.001)))throw Error('A virtual center remains');return {nodes:scene.nodes.map(n=>({id:n.id,x:n.x,y:n.y})),edges:scene.edges};},fixture.config);
 assert.equal(geometry.edges.length,fixture.edges.length);for(let i=0;i<fixture.edges.length;i++)for(const end of ['a','b'])for(const axis of ['x','y'])assert.ok(Math.abs(geometry.edges[i][end][axis]-fixture.edges[i][end][axis])<0.001,'Native root links match editor');
 await page.evaluate(()=>localStorage.setItem('spiderweb-preview',JSON.stringify({version:1,launcher:{view:'web',autoRoute:false},groups:[{id:'work',name:'Работа',parentId:null,position:2},{id:'play',name:'Игры',parentId:null,position:3}],apps:[],layout:{nodes:{'sys-prog':{x:-260,y:-80},work:{x:260,y:-80},play:{x:0,y:240}},branches:{}}})));
 await page.reload();await page.locator('#web-tab').click();assert.equal(await page.locator('[data-root-link]').count(),3);
 await page.locator('#roots-chain').click();await page.waitForFunction(()=>JSON.parse(localStorage.getItem('spiderweb-preview')).layout.rootLinks?.length===2);
 assert.deepEqual(await page.evaluate(()=>JSON.parse(localStorage.getItem('spiderweb-preview')).layout.rootLinks),[['sys-prog','work'],['work','play']]);
 await page.locator('[data-root-link="1"] [data-link-end="0"]').selectOption('sys-prog');
 await page.waitForFunction(()=>JSON.parse(localStorage.getItem('spiderweb-preview')).layout.rootLinks?.[1]?.[0]==='sys-prog');
 const chosen=await page.evaluate(()=>JSON.parse(localStorage.getItem('spiderweb-preview')).layout.rootLinks);assert.deepEqual(chosen,[['sys-prog','work'],['sys-prog','play']]);
 await page.reload();await page.locator('#web-tab').click();assert.equal(await page.locator('[data-root-link]').count(),2);assert.equal(await page.locator('[data-root-link="1"] [data-link-end="0"]').inputValue(),'sys-prog');
 await page.locator('[data-link-remove="0"]').click();assert.equal(await page.locator('[data-root-link]').count(),2);assert.match(await page.locator('#status').textContent(),/оставаться связанными/);
 await page.locator('#roots-all').click();await page.waitForFunction(()=>JSON.parse(localStorage.getItem('spiderweb-preview')).layout.rootLinks.length===3);assert.equal(await page.locator('[data-root-link]').count(),3);
 await page.locator('[data-layout="radial"]').click();assert.ok(await page.locator('#radial-preview').count());await page.locator('[data-layout="web"]').click();assert.equal(await page.locator('[data-root-link]').count(),3);assert.deepEqual(errors,[]);
 console.log('PASS: direct root links without a center, native/editor parity, all-to-all/chain presets, editable endpoints, persistence, connectivity guard and view switching.');
}finally{await browser.close();}
