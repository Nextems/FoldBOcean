import {chromium} from 'playwright';
import assert from 'node:assert/strict';
const browser=await chromium.launch({channel:'msedge',headless:true});
try {
 const page=await browser.newPage({viewport:{width:1600,height:1000}}),errors=[];page.on('pageerror',e=>errors.push(e.message));await page.goto('http://127.0.0.1:1420');await page.locator('#web-tab').click();
 const saved=()=>page.evaluate(()=>JSON.parse(localStorage.getItem('spiderweb-preview')));
 const change=async(id,value)=>page.locator(id).evaluate((el,v)=>{el.value=v;el.dispatchEvent(new Event('input',{bubbles:true}));el.dispatchEvent(new Event('change',{bubbles:true}));},String(value));
 const initialColor=(await saved()).radial.color;
 await page.locator('#rgb-radial').check();const first=await page.locator('.sector-outline').first().getAttribute('style');await page.waitForFunction(old=>document.querySelector('.sector-outline').getAttribute('style')!==old,first);
 await page.locator('#rgb-radial').uncheck();assert.ok((await page.locator('.sector-outline').first().getAttribute('style')).includes(initialColor));
 const width=await page.locator('.sector-outline').first().boundingBox();await change('#size-radial',140);assert.ok((await page.locator('.sector-outline').first().boundingBox()).width>width.width*1.39);
 await change('#size-ringStep',10);
 await page.evaluate(async()=>{const {ringScale,ringWidth,makeRadialScene,iconBox,hitSector}=await import('/src/radial-layout.js');if(ringScale(0,10)!==1||ringScale(1,10)!==.9||ringScale(2,10)!==.8||ringScale(4,10)!==.8)throw Error('Linear decrement must stop at third layer');if(Math.abs(ringWidth(1,10)-54)>.001||Math.abs(ringWidth(2,10)-48)>.001)throw Error('Main minus X width');const config=JSON.parse(localStorage.getItem('spiderweb-preview')),scene=makeRadialScene(config,['sys-prog','sys-personal']);for(const s of scene.sectors){const box=iconBox(s);if(hitSector(scene,{x:box.x,y:box.y})<0)throw Error('Scaled icon escapes ring');}});
 await page.locator('[data-layout="web"]').click();const node=page.locator('[data-node-id="sys-prog"]'),before=await node.boundingBox();await change('#size-web',130);assert.ok((await node.boundingBox()).width>before.width*1.29);
 await page.locator('#rgb-blocks').check();await page.locator('#rgb-branches').check();await change('#rgb-speed',2);
 const original=await page.locator('.web-connection').first().getAttribute('style');await page.waitForFunction(old=>document.querySelector('.web-connection').getAttribute('style')!==old,original);
 const colors=await page.locator('.web-border').evaluateAll(nodes=>nodes.map(n=>n.style.stroke));assert.ok(new Set(colors).size>1);assert.equal((await saved()).rgb.speed,2);
 assert.equal(await node.locator('.node-label').evaluate(el=>getComputedStyle(el).fontWeight),'400');
 await page.screenshot({path:'release/rgb-size-preview.png'});
 await page.reload();await page.locator('#web-tab').click();assert.ok(await page.locator('#rgb-blocks').isChecked());assert.equal(await page.locator('#size-web').inputValue(),'130');assert.equal(await page.locator('#size-ringStep').inputValue(),'10');
 await page.locator('#rgb-blocks').uncheck();await page.locator('#rgb-branches').uncheck();assert.equal(await page.locator('.web-border').first().evaluate(el=>el.style.stroke),'');assert.equal(await page.locator('.web-connection').first().evaluate(el=>el.style.stroke),'');assert.equal((await saved()).radial.color,initialColor);assert.deepEqual(errors,[]);
 console.log('PASS: RGB animation, independent colors/switches, restored static colors, speed and size persistence, linear X decrement through layer three, web font and both live scales.');
}finally{await browser.close();}
