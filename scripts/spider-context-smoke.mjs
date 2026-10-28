import {chromium} from 'playwright';
import assert from 'node:assert/strict';
const browser=await chromium.launch({channel:'msedge',headless:true});
try{
 const page=await browser.newPage({viewport:{width:1600,height:1000}}),errors=[];page.on('pageerror',e=>errors.push(e.message));await page.goto('http://127.0.0.1:1420');
 await page.evaluate(()=>localStorage.setItem('spiderweb-preview',JSON.stringify({version:1,launcher:{view:'web',autoRoute:false},groups:[{id:'work',name:'Работа',parentId:null,position:2},{id:'sys-personal',name:'Свои',parentId:'sys-prog',position:7}],apps:[{id:'note',name:'Заметки',path:'note.exe',groupId:'work',position:1}],layout:{nodes:{work:{x:220,y:0}},branches:{}}})));
 await page.reload();await page.locator('#web-tab').click();
 const menu=page.locator('.spider-context-menu');const rightClick=async id=>{const b=await page.locator(`[data-node-id="${id}"]`).boundingBox();await page.mouse.click(b.x+b.width/2,b.y+b.height/2,{button:'right'});await menu.waitFor({state:'visible'});};
 const save=async(name,path)=>{await page.locator('#editor [name="name"]').fill(name);if(path)await page.locator('#editor [name="path"]').fill(path);await page.locator('#editor button[type="submit"]').click();};
 await rightClick('work');await menu.getByRole('menuitem',{name:'Добавить группу',exact:false}).click();assert.equal(await page.locator('#editor [name="parent"]').inputValue(),'work');await save('Подгруппа');
 let data=await page.evaluate(()=>JSON.parse(localStorage.getItem('spiderweb-preview')));assert.equal(data.groups.find(g=>g.name==='Подгруппа').parentId,'work');
 await rightClick('work');await menu.getByRole('menuitem',{name:'Добавить программу',exact:false}).click();assert.equal(await page.locator('#editor [name="parent"]').inputValue(),'work');await save('Из ПКМ','C:\\Apps\\Context.exe');
 data=await page.evaluate(()=>JSON.parse(localStorage.getItem('spiderweb-preview')));const app=data.apps.find(a=>a.name==='Из ПКМ');assert.equal(app.groupId,'work');
 await rightClick(app.id);await menu.getByRole('menuitem',{name:'Добавить программу',exact:false}).click();assert.equal(await page.locator('#editor [name="parent"]').inputValue(),'work');await page.locator('#editor .cancel').click();
 await rightClick('sys-prog');assert.ok(await menu.getByRole('menuitem',{name:'Добавить группу',exact:false}).isDisabled());await menu.getByRole('menuitem',{name:'Добавить программу',exact:false}).click();assert.equal(await page.locator('#editor [name="parent"]').inputValue(),'sys-personal');await page.locator('#editor .cancel').click();
 const bounds=await page.locator('#web-preview').boundingBox();await page.mouse.click(bounds.x+8,bounds.y+8,{button:'right'});await menu.getByRole('menuitem',{name:'Создать основную группу'}).click();assert.equal(await page.locator('#editor [name="parent"]').inputValue(),'');await save('ПКМ корень');
 data=await page.evaluate(()=>JSON.parse(localStorage.getItem('spiderweb-preview')));assert.equal(data.groups.find(g=>g.name==='ПКМ корень').parentId,null);
 await rightClick('work');await page.keyboard.press('Escape');assert.ok(await menu.isHidden());await rightClick('work');await page.locator('[data-layout="radial"]').click();assert.equal(await page.locator('.spider-context-menu').count(),0);
 await page.reload();await page.locator('#web-tab').click();await page.locator('[data-layout="web"]').click();data=await page.evaluate(()=>JSON.parse(localStorage.getItem('spiderweb-preview')));assert.ok(data.apps.some(a=>a.name==='Из ПКМ'));assert.deepEqual(errors,[]);
 console.log('PASS: right-click group/app/canvas creation, correct parent forms, system subgroup routing, persistence, Escape and mode-change cleanup.');
}finally{await browser.close();}
