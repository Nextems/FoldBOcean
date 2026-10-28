import {chromium} from 'playwright';
import {execFileSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
import assert from 'node:assert/strict';
const monitors=JSON.parse(execFileSync(fileURLToPath(new URL('../release/FoldBOcean.exe',import.meta.url)),['--export-monitors'],{encoding:'utf8'}));
assert.ok(monitors.some(m=>m.primary));
for(const monitor of monitors)assert.ok(monitor.id&&monitor.width>0&&monitor.height>0);
const browser=await chromium.launch({channel:'msedge',headless:true});
try {
  const page=await browser.newPage({viewport:{width:1440,height:1000}}),errors=[];
  page.on('pageerror',e=>errors.push(e.message));await page.goto('http://127.0.0.1:1420');
  await page.evaluate(async()=>{
    const {mountWebEditor}=await import('/src/web-editor.js'),{initialConfig}=await import('/src/config.js');
    window.monitorConfig=initialConfig();
    window.mountMonitorEditor=(list)=>{document.body.innerHTML='<div id="test-editor"></div>';mountWebEditor(document.querySelector('#test-editor'),{getConfig:()=>window.monitorConfig,persist:async()=>true,launch:async()=>{},edit:()=>{},status:()=>{},initialInteraction:true,getMonitors:async()=>list});};
    window.mountMonitorEditor([{id:'test-display',number:2,width:2560,height:1440,primary:false}]);
  });
  await page.waitForFunction(()=>document.querySelector('[name="monitor"] option[value="test-display"]'));
  assert.equal(await page.locator('[name="monitor"]').inputValue(),'primary');
  await page.locator('[name="monitor"]').selectOption('test-display');await page.locator('#interaction-form button[type="submit"]').click();
  assert.equal(await page.evaluate(()=>window.monitorConfig.launcher.monitor),'test-display');
  await page.evaluate(()=>window.mountMonitorEditor([]));
  await page.waitForFunction(()=>document.querySelector('[name="monitor"]').textContent.includes('отключён'));
  assert.equal(await page.locator('[name="monitor"]').inputValue(),'test-display');
  await page.locator('[name="monitor"]').selectOption('primary');await page.locator('#interaction-form button[type="submit"]').click();
  assert.equal(await page.evaluate(()=>window.monitorConfig.launcher.monitor),'primary');assert.deepEqual(errors,[]);
  console.log('PASS: native display enumeration, primary default, screen selection, disconnected screen preference and return to primary.');
}finally{await browser.close();}
