import {chromium} from 'playwright';
import {execFileSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
import assert from 'node:assert/strict';
const state=JSON.parse(execFileSync(fileURLToPath(new URL('../release/FoldBOcean.exe',import.meta.url)),['--autostart-status'],{encoding:'utf8'}));
assert.equal(typeof state.enabled,'boolean');assert.ok(state.command.endsWith('FoldBOcean.exe" --background'));
const browser=await chromium.launch({channel:'msedge',headless:true});
try {
 const page=await browser.newPage({viewport:{width:1500,height:950}}),errors=[];page.on('pageerror',e=>errors.push(e.message));await page.goto('http://127.0.0.1:1420');await page.locator('#launcher-options').click();assert.ok(await page.locator('#autostart-toggle').isDisabled());
 await page.evaluate(async()=>{const {mountAutostart}=await import('/src/autostart.js');const host=document.createElement('div');host.id='startup-test';document.body.replaceChildren(host);window.startupCalls=[];window.startupErrors=[];mountAutostart(host,{supported:true,read:async()=>({enabled:false}),set:async enabled=>{startupCalls.push(enabled);if(window.failStartup)throw Error('Registry access failed');return {enabled};},status:(message,error)=>startupErrors.push({message,error})});});
 const toggle=page.locator('#autostart-toggle');await page.waitForFunction(()=>!document.querySelector('#autostart-toggle').disabled);assert.equal(await toggle.isChecked(),false);await toggle.check();await page.waitForFunction(()=>!document.querySelector('#autostart-toggle').disabled);assert.equal(await toggle.isChecked(),true);await toggle.uncheck();await page.waitForFunction(()=>!document.querySelector('#autostart-toggle').disabled);assert.deepEqual(await page.evaluate(()=>startupCalls),[true,false]);
 await page.evaluate(()=>window.failStartup=true);await toggle.click();await page.waitForFunction(()=>!document.querySelector('#autostart-toggle').disabled);assert.equal(await toggle.isChecked(),false);assert.ok(await page.evaluate(()=>startupErrors.at(-1).error));assert.deepEqual(errors,[]);
 console.log('PASS: native status returns quoted background launcher command; browser fallback; enabled/disabled UI roundtrip; registry errors revert the switch.');
}finally{await browser.close();}
