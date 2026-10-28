import {chromium} from 'playwright';
import assert from 'node:assert/strict';
const browser=await chromium.launch({channel:'msedge',headless:true});
try {
 const page=await browser.newPage({viewport:{width:1500,height:950}}),errors=[];page.on('pageerror',e=>errors.push(e.message));
 await page.goto('http://127.0.0.1:1420');await page.locator('#web-tab').click();await page.locator('[data-layout="web"]').click();
 const change=async(id,value)=>page.locator(id).evaluate((el,value)=>{el.value=value;el.dispatchEvent(new Event('input',{bubbles:true}));el.dispatchEvent(new Event('change',{bubbles:true}));},String(value));
 const saved=()=>page.evaluate(()=>JSON.parse(localStorage.getItem('spiderweb-preview')));
 const radial=(await saved()).radial;
 await change('#web-color','#d24581');await change('#web-transparency',42);await change('#web-blur',18);
 assert.deepEqual((await saved()).webStyle,{color:'#d24581',opacity:58,blur:18});
 assert.equal(await page.locator('#web-transparency-value').textContent(),'42%');
 assert.equal(await page.locator('.web-material').first().evaluate(el=>getComputedStyle(el).opacity),'0.58');
 assert.equal(await page.locator('.web-material div').first().evaluate(el=>getComputedStyle(el).backdropFilter),'blur(18px)');
 assert.equal(await page.locator('.node-label').first().evaluate(el=>getComputedStyle(el).filter),'none');
 await change('#web-hue',120);assert.notEqual((await saved()).webStyle.color,'#d24581');
 await page.screenshot({path:'release/web-style-preview.png'});
 await page.locator('[data-layout="radial"]').click();assert.deepEqual((await saved()).radial,radial);
 await page.locator('[data-layout="web"]').click();await page.reload();await page.locator('#web-tab').click();assert.equal(await page.locator('#web-blur').inputValue(),'18');assert.equal(await page.locator('#web-transparency').inputValue(),'42');
 await change('#web-transparency',100);assert.equal(await page.locator('.web-material').first().evaluate(el=>getComputedStyle(el).opacity),'0');
 assert.deepEqual(errors,[]);console.log('PASS: web color/hue, transparency, blur preview and persistence; labels remain clear; radial style preserved.');
}finally{await browser.close();}
