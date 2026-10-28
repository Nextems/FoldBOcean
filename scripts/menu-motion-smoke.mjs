import {chromium} from 'playwright';
import assert from 'node:assert/strict';
const browser=await chromium.launch({channel:'msedge',headless:true});
try {
 const page=await browser.newPage({viewport:{width:1500,height:950}}),errors=[];page.on('pageerror',e=>errors.push(e.message));await page.goto('http://127.0.0.1:1420');
 await page.evaluate(()=>{window.motionLog=[];const original=Element.prototype.animate;Element.prototype.animate=function(frames,options){if(this.matches('.radial-sector,#web-drawing'))window.motionLog.push({parent:this.dataset.parent||'web',duration:options.duration,frames});return original.call(this,frames,options);};});
 await page.locator('#web-tab').click();assert.ok(await page.evaluate(()=>motionLog.some(a=>a.parent==='@root'&&a.duration===450)));
 const settle=()=>page.waitForFunction(()=>document.querySelectorAll('#radial-preview [data-parent]').length>0&&[...document.querySelectorAll('#radial-preview [data-parent]')].every(el=>el.getAnimations().length===0));await settle();
 await page.locator('#ring-group').selectOption('sys-prog');assert.ok(await page.evaluate(()=>motionLog.some(a=>a.parent==='sys-prog')));assert.equal(await page.locator('[data-parent="@root"]').first().evaluate(el=>el.getAnimations().length),0);await settle();
 await page.locator('#ring-group').selectOption('sys-personal');assert.ok(await page.evaluate(()=>motionLog.some(a=>a.parent==='sys-personal')));assert.equal(await page.locator('[data-parent="sys-prog"]').first().evaluate(el=>el.getAnimations().length),0);await settle();
 const count=await page.evaluate(()=>motionLog.length);await page.locator('#radial-color').evaluate(el=>{el.value='#7766aa';el.dispatchEvent(new Event('input'));});assert.equal(await page.evaluate(()=>motionLog.length),count,'Changing paint does not replay old rings');
 await page.locator('#ring-back').click();await page.locator('#ring-group').selectOption('sys-personal');assert.ok(await page.evaluate(()=>motionLog.length)>count,'Reopening a removed layer animates it again');await settle();
 await page.locator('[data-layout="web"]').click();assert.ok(await page.evaluate(()=>motionLog.some(a=>a.parent==='web'&&a.duration===450)));await page.waitForFunction(()=>document.querySelector('#web-drawing').getAnimations().length===0);assert.deepEqual(errors,[]);
 console.log('PASS: initial circle/web opening, second and third ring reveal, settled parents remain still, no replay on paint edits, reopening layers animates again.');
}finally{await browser.close();}
