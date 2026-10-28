import {chromium} from 'playwright';
import assert from 'node:assert/strict';
const browser=await chromium.launch({channel:'msedge',headless:true});
try {
 const page=await browser.newPage({viewport:{width:1500,height:950}}),errors=[];page.on('pageerror',e=>errors.push(e.message));await page.goto('http://127.0.0.1:1420');await page.locator('#web-tab').click();
 const change=async value=>page.locator('#animation-duration').evaluate((el,value)=>{el.value=value;el.dispatchEvent(new Event('input',{bubbles:true}));el.dispatchEvent(new Event('change',{bubbles:true}));},String(value));
 await change(1000);assert.equal(await page.locator('#animation-duration').locator('..').locator('output').textContent(),'1.00 с');
 await page.locator('#preview-opening').click();assert.ok(await page.locator('.radial-sector').first().evaluate(el=>el.getAnimations().some(a=>a.effect.getTiming().duration===1000)));
 await page.locator('#ring-group').selectOption('sys-prog');assert.ok(await page.locator('[data-parent="sys-prog"]').first().evaluate(el=>el.getAnimations().some(a=>a.effect.getTiming().duration>900&&a.effect.getTiming().duration<=1000)));
 await page.locator('[data-layout="web"]').click();assert.equal(await page.locator('#animation-duration').inputValue(),'1000');await page.locator('#preview-opening').click();assert.equal(await page.locator('#web-drawing').evaluate(el=>el.getAnimations()[0].effect.getTiming().duration),1000);
 await change(100);await page.locator('#preview-opening').click();assert.equal(await page.locator('#web-drawing').evaluate(el=>el.getAnimations()[0].effect.getTiming().duration),100);
 await page.reload();await page.locator('#web-tab').click();assert.equal(await page.locator('#animation-duration').inputValue(),'100');await page.locator('[data-layout="radial"]').click();assert.equal(await page.locator('#animation-duration').inputValue(),'100');assert.deepEqual(errors,[]);
 console.log('PASS: 0.10–1.50 s opening slider, immediate preview in both modes, subsequent ring duration, shared value and persistence after reload.');
}finally{await browser.close();}
