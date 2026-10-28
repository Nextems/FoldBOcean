import {chromium} from 'playwright';
import assert from 'node:assert/strict';
import fs from 'node:fs';
const fixtures=JSON.parse(fs.readFileSync('tests/config-cases.json','utf8'));
const browser=await chromium.launch({channel:'msedge',headless:true});
try{
  const page=await browser.newPage();const errors=[];page.on('pageerror',e=>errors.push(e.message));
  await page.addInitScript(config=>localStorage.setItem('spiderweb-preview',JSON.stringify(config)),fixtures.find(x=>x.name==='13 positions in third ring').config);
  await page.goto('http://127.0.0.1:1420');
  await page.locator('nav [data-select="child"]').click();
  const before=await page.evaluate(()=>localStorage.getItem('spiderweb-preview'));
  await page.locator('#edit-current').click();
  await page.locator('[name=parent]').selectOption('');
  await page.locator('#editor button[type=submit]').click();
  await page.waitForFunction(()=>document.querySelector('.form-error')?.textContent.includes('потомков'));
  assert.equal(await page.evaluate(()=>localStorage.getItem('spiderweb-preview')),before);
  assert.equal(await page.locator('#editor').evaluate(el=>el.open),true);
  await page.locator('#editor .cancel').click();
  await page.locator('[data-home]').click();
  await page.evaluate(()=>{
    const original=Storage.prototype.setItem;
    window.failSaves=true;
    Storage.prototype.setItem=function(key,value){if(window.failSaves&&key==='spiderweb-preview')throw Error('disk full');return original.call(this,key,value);};
  });
  async function create(name){await page.locator('#create-group').click();await page.locator('[name=name]').fill(name);await page.locator('#editor button[type=submit]').click();}
  await create('Failed');
  await page.waitForFunction(()=>document.querySelector('#status')?.textContent.includes('disk full'));
  assert.equal(await page.evaluate(()=>localStorage.getItem('spiderweb-preview')),before);
  assert.equal(await page.locator('.item-name').filter({hasText:'Failed'}).count(),0);
  await page.evaluate(()=>window.failSaves=false);
  await create('Recovered');
  await page.waitForFunction(()=>JSON.parse(localStorage.getItem('spiderweb-preview')).groups.some(g=>g.name==='Recovered'));
  assert.deepEqual(errors,[]);
  console.log('PASS: descendant overflow, visible rollback and recovery after failed save');
}finally{await browser.close();}
