import {chromium} from 'playwright';
import assert from 'node:assert/strict';
const browser=await chromium.launch({channel:'msedge',headless:true});
try{
  const page=await browser.newPage({viewport:{width:1500,height:1000}}),errors=[];page.on('pageerror',error=>errors.push(error.message));
  await page.goto('http://127.0.0.1:1420');
  await page.locator('nav [data-select="sys-prog"]').click();await page.locator('[data-edit="sys-cmd"]').click();
  await page.locator('[name=noIcon]').check();await page.locator('#editor [type=submit]').click();
  await page.reload();await page.waitForFunction(()=>JSON.parse(localStorage.getItem('spiderweb-preview')).apps.find(a=>a.id==='sys-cmd').noIcon===true);
  const state=await page.evaluate(async()=>{
    const {initialConfig,normalizeConfig}=await import('/src/config.js');const {missingMessage,fitWrappedLabel,blockLabel}=await import('/src/block-label.js');
    const config=initialConfig();config.groups.find(g=>g.id==='sys-personal').noIcon=true;
    config.apps.push({id:'missing',name:'Очень длинное полное название потерянной программы',path:'C:\\Missing\\Program.exe',groupId:'sys-personal',position:2});
    config.apps.push({id:'text-only',name:'ДлинноеНазваниеПрограммыБезЗначка',path:'cmd.exe',groupId:'sys-personal',position:3,noIcon:true});
    window.labelConfig=normalizeConfig(config);window.labelIcons={'@missing':{missing:true}};
    const longest=missingMessage('Я'.repeat(256));const fit=fitWrappedLabel(longest,108,32,12.5);
    if(fit.lines.join('').replace(/\s/g,'')!==longest.replace(/\s/g,''))throw Error('Full name was truncated');
    if(fit.lines.length*fit.size*1.15>32.01)throw Error('Message overflows');
    return {message:missingMessage(config.apps.find(a=>a.id==='missing').name),web:blockLabel(config.apps.at(-1),'web'),radial:blockLabel(config.apps.at(-1),'radial')};
  });
  async function mount(view){await page.evaluate(async view=>{
    const {mountWebEditor}=await import('/src/web-editor.js');window.labelConfig.launcher.view=view;
    const container=document.querySelector('.workspace,.web-workspace');container.className='web-workspace';document.querySelector('main').classList.add('web-view');
    mountWebEditor(container,{getConfig:()=>window.labelConfig,persist:async()=>true,launch:async()=>{},edit:()=>{},status:()=>{},getIcons:async()=>window.labelIcons});
  },view);}
  await mount('web');await page.waitForFunction(()=>document.querySelector('[data-node-id="missing"] .wrapped-label'));
  const web=page.locator('[data-node-id="missing"]');assert.equal(await web.locator('.wrapped-label').getAttribute('data-full-text'),state.message.replaceAll('\n',' '));
  assert.equal(await web.locator('image,.node-symbol').count(),0);
  assert.equal(await page.locator('[data-node-id="text-only"] .wrapped-label').getAttribute('data-full-text'),state.web);
  assert.equal(await page.locator('[data-node-id="text-only"] .wrapped-label').evaluate(element=>getComputedStyle(element).fontWeight),'600');
  assert.equal(Array.from(state.web).length,15);await page.evaluate(()=>Promise.all(document.getAnimations().filter(a=>a.effect.getTiming().iterations!==Infinity).map(a=>a.finished.catch(()=>{}))));await page.screenshot({path:'release/missing-web-preview.png'});
  await mount('radial');await page.locator('#ring-group').selectOption('sys-personal');await page.waitForFunction(()=>document.querySelector('[data-sector-id="missing"] .wrapped-label'));
  const ring=page.locator('[data-sector-id="missing"]');assert.equal(await ring.locator('.wrapped-label').getAttribute('data-full-text'),state.message);
  assert.equal(await ring.locator('image,.sector-icon').count(),0);
  assert.equal(await page.locator('[data-sector-id="text-only"] .wrapped-label').getAttribute('data-full-text'),state.radial);
  assert.equal(await page.locator('[data-sector-id="text-only"] .wrapped-label text').first().evaluate(element=>getComputedStyle(element).fontWeight),'600');
  assert.equal(Array.from(state.radial).length,20);
  await page.evaluate(()=>Promise.all(document.getAnimations().filter(a=>a.effect.getTiming().iterations!==Infinity).map(a=>a.finished.catch(()=>{}))));
  await page.screenshot({path:'release/missing-radial-preview.png'});assert.deepEqual(errors,[]);
  console.log('PASS: no-icon option survives system normalization/reload; 15/20-character wrapped labels; full missing messages replace icons in both modes; 256-character name is not truncated.');
}finally{await browser.close();}

