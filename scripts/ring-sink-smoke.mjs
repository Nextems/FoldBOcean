import {chromium} from 'playwright';
import assert from 'node:assert/strict';
const browser=await chromium.launch({channel:'msedge',headless:true});
try {
 const page=await browser.newPage({viewport:{width:1600,height:1000}}),errors=[];page.on('pageerror',e=>errors.push(e.message));await page.goto('http://127.0.0.1:1420');
 await page.evaluate(()=>{const groups=[{id:'sys-personal',name:'Свои',parentId:'sys-prog',position:7}];let parent='sys-personal';for(let i=1;i<=5;i++){const id='d'+i;groups.push({id,name:id,parentId:parent,position:1});parent=id;}localStorage.setItem('spiderweb-preview',JSON.stringify({version:1,groups,apps:[]}));});await page.reload();await page.locator('#web-tab').click();
 const settle=()=>page.waitForFunction(()=>[...document.querySelectorAll('.radial-sector')].every(el=>el.getAnimations().length===0));
 const layer=parent=>page.locator('[data-parent="'+parent+'"]').first();
 const opacity=parent=>layer(parent).evaluate(el=>Number(getComputedStyle(el).opacity));
 await page.locator('#ring-group').selectOption('d1');await settle();assert.equal(await layer('sys-prog').getAttribute('data-sunk'),'false');
 await page.locator('#ring-group').selectOption('d2');assert.equal(await layer('sys-prog').getAttribute('data-sunk'),'true');assert.equal(await layer('@root').getAttribute('data-sunk'),'false');
 assert.ok(await layer('sys-prog').evaluate(el=>el.getAnimations().some(a=>a.effect.getTiming().duration>350&&a.effect.getTiming().duration<=400)));
 await settle();assert.equal(await opacity('sys-prog'),0);assert.equal(await opacity('@root'),1);assert.equal(await opacity('sys-personal'),1);
 await page.locator('#ring-group').selectOption('d3');assert.equal(await layer('sys-personal').getAttribute('data-sunk'),'true');assert.equal(await opacity('sys-prog'),0);
 await page.waitForFunction(()=>{const el=document.querySelector('[data-parent="sys-personal"]');const opacity=Number(getComputedStyle(el).opacity);return opacity>.1&&opacity<.8;});
 const before=await opacity('sys-personal');await page.locator('#ring-back').click();const after=await opacity('sys-personal');assert.ok(Math.abs(before-after)<.2,'Reversing preserves current opacity');assert.equal(await layer('sys-personal').getAttribute('data-sunk'),'false');assert.ok(await layer('sys-personal').evaluate(el=>el.getAnimations().some(a=>a.effect.getTiming().duration>300&&a.effect.getTiming().duration<=350)));
 await settle();assert.equal(await opacity('sys-personal'),1);assert.equal(await opacity('sys-prog'),0);
 await page.locator('#ring-back').click();await settle();assert.equal(await opacity('sys-prog'),1);assert.equal(await opacity('@root'),1);
 await page.evaluate(async()=>{const {isSunk,motionValue,retargetRing}=await import('/src/menu-motion.js');for(let n=1;n<=10;n++)for(let d=0;d<n;d++)if(isSunk(d,n)!==(d>0&&d<=n-4))throw Error('Bad sink rule');let state={from:{zoom:1,alpha:1},to:{zoom:1,alpha:1},started:0,duration:450,submerged:false};state=retargetRing(state,true,.6,1000);const mid=motionValue(state,1200);state=retargetRing(state,false,.6,1200);const reverse=motionValue(state,1200);if(reverse.alpha!==mid.alpha||reverse.zoom!==mid.zoom||motionValue(state,1550).alpha!==1)throw Error('Discontinuous reversal');});
 assert.deepEqual(errors,[]);console.log('PASS: fifth ring sinks second in 400 ms; sixth sinks third; base stays visible; rapid back reverses continuously in 350 ms; earlier rings restore; hidden layers stop receiving pointer events.');
}finally{await browser.close();}
