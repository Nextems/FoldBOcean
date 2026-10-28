import {chromium} from 'playwright';
import assert from 'node:assert/strict';
const browser=await chromium.launch({channel:'msedge',headless:true});
try{
  const page=await browser.newPage({viewport:{width:1400,height:950}}),errors=[];
  page.on('pageerror',error=>errors.push(error.message));await page.goto('http://127.0.0.1:1420');
  await page.evaluate(async()=>{
    const {initialConfig}=await import('/src/config.js');const config=initialConfig();
    config.groups.push({id:'work',name:'Работа',parentId:null,position:2});
    config.apps.push({id:'editor',name:'Редактор',path:'C:\\Apps\\Editor.exe',groupId:'work',position:1});
    config.appearance={web:125,radial:110,ringStep:9,openMs:650};config.radial.color='#6595aa';
    config.layout.nodes.work={x:200,y:90};localStorage.setItem('spiderweb-preview',JSON.stringify(config));
  });
  await page.reload();const before=await page.evaluate(()=>JSON.parse(localStorage.getItem('spiderweb-preview')));
  await page.locator('#presets-tab').click();await page.locator('#save-preset').click();
  await page.locator('[name=presetName]').fill('Работа <новая>');await page.locator('#preset-dialog [type=submit]').click();
  await page.locator('.preset-card').waitFor();assert.equal(await page.locator('.preset-info h2').textContent(),'Работа <новая>');
  const id=await page.locator('.preset-card').getAttribute('data-preset-id');
  await page.locator('[data-preset-action=rename]').click();await page.locator('[name=presetName]').fill('Игры');await page.locator('#preset-dialog [type=submit]').click();
  await page.waitForFunction(()=>document.querySelector('.preset-info h2')?.textContent==='Игры');
  await page.evaluate(()=>{const config=JSON.parse(localStorage.getItem('spiderweb-preview'));config.groups.find(g=>g.id==='work').name='Другая';config.appearance.web=80;config.launcher.view='web';localStorage.setItem('spiderweb-preview',JSON.stringify(config));});
  await page.reload();await page.locator('#presets-tab').click();await page.locator('[data-preset-action=apply]').click();
  await page.locator('#preset-dialog [value=cancel]').click();assert.equal(await page.evaluate(()=>JSON.parse(localStorage.getItem('spiderweb-preview')).appearance.web),80);
  await page.locator('[data-preset-action=apply]').click();await page.locator('#preset-dialog [value=yes]').click();
  await page.waitForFunction(()=>JSON.parse(localStorage.getItem('spiderweb-preview')).appearance.web===125);
  assert.deepEqual(await page.evaluate(()=>JSON.parse(localStorage.getItem('spiderweb-preview'))),before);
  await page.evaluate(()=>{const config=JSON.parse(localStorage.getItem('spiderweb-preview'));config.appearance.web=130;localStorage.setItem('spiderweb-preview',JSON.stringify(config));});
  await page.reload();await page.locator('#presets-tab').click();
  await page.locator('[data-preset-action=update]').click();await page.locator('#preset-dialog [value=yes]').click();
  await page.waitForFunction(()=>document.querySelector('#status')?.textContent==='Пресет обновлён');
  assert.equal(await page.evaluate(async id=>{const {createPresetRepository}=await import('/src/preset-repository.js');return (await createPresetRepository().load(id)).config.appearance.web;},id),130);
  await page.evaluate(async()=>{
    const {createPresetRepository}=await import('/src/preset-repository.js');const repo=createPresetRepository();
    const config=JSON.parse(localStorage.getItem('spiderweb-preview'));
    for(let index=0;index<205;index++)await repo.save('Набор '+index,config);
    localStorage.setItem('foldbocean-preset:broken','invalid JSON');
    const summary=await repo.list();if(summary.length!==207||summary.some(item=>'config' in item))throw Error('Bad preset summary');
    try{await repo.load('..\\config');throw Error('Unsafe id accepted');}catch(error){if(error.message==='Unsafe id accepted')throw error;}
  });
  await page.reload();await page.locator('#presets-tab').click();await page.waitForFunction(()=>document.querySelectorAll('.preset-card').length===207);
  await page.locator('#preset-search').fill('Игры');assert.equal(await page.locator('.preset-card').count(),1);
  await page.locator('[data-preset-action=delete]').click();await page.locator('#preset-dialog [value=cancel]').click();assert.equal(await page.locator('.preset-card').count(),1);
  await page.locator('[data-preset-action=delete]').click();await page.locator('#preset-dialog [value=yes]').click();await page.waitForFunction(id=>!localStorage.getItem('foldbocean-preset:'+id),id);
  assert.equal(await page.evaluate(id=>localStorage.getItem('foldbocean-preset:'+id),id),null);
  await page.locator('#preset-search').fill('Набор 20');await page.screenshot({path:'release/presets-preview.png'});
  assert.deepEqual(errors,[]);
  console.log('PASS: full presets, save/rename/update/apply/delete confirmations, 205+ saved presets, reload, search, corruption isolation and escaped names.');
}finally{await browser.close();}
