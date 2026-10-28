import { spawn } from 'node:child_process';
import { chromium } from 'playwright';
import assert from 'node:assert/strict';

const helper=spawn(new URL('../release/SpiderWeb.exe',import.meta.url).pathname.replace(/^\/([A-Za-z]:)/,'$1'),['--export-icons'],{windowsHide:true});
let output='';helper.stdout.on('data',chunk=>output+=chunk);
assert.equal(await new Promise((resolve,reject)=>{helper.on('close',resolve);helper.on('error',reject);}),0);
const icons=JSON.parse(output);
const browser=await chromium.launch({channel:'msedge',headless:true});
try {
  const page=await browser.newPage({viewport:{width:1280,height:860}});
  await page.goto('http://127.0.0.1:1420');
  const count=await page.evaluate(async icons=>{
    const {initialConfig}=await import('/src/config.js');const {mountWebEditor}=await import('/src/web-editor.js');
    const config=initialConfig();config.launcher.view='web';
    const container=document.querySelector('.workspace');container.className='web-workspace';document.querySelector('main').classList.add('web-view');
    mountWebEditor(container,{getConfig:()=>config,persist:async()=>true,launch:async()=>{},edit:()=>{},status:()=>{},getIcons:async()=>icons});
    for(const id of ['@folder','sys-settings','sys-cmd','sys-regedit','sys-disks','sys-devices','sys-control']){
      if(!icons[id])throw new Error('Missing icon '+id);
      const image=await createImageBitmap(await(await fetch(icons[id])).blob());
      const canvas=document.createElement('canvas');canvas.width=24;canvas.height=24;const context=canvas.getContext('2d');context.drawImage(image,0,0);
      if(!Array.from(context.getImageData(0,0,24,24).data).some((value,i)=>i%4===3&&value>0))throw new Error('Empty icon '+id);
    }
    return Object.keys(icons).length;
  },icons);
  await page.waitForFunction(()=>document.querySelectorAll('svg image').length>=7);
  assert.ok(count>=7);assert.equal(await page.locator('[data-node-id="sys-regedit"] .node-label').textContent(),'Редактор ...');
  for(const label of await page.locator('.node-label').allTextContents())assert.ok(Array.from(label).length<=12);
  assert.equal(await page.locator('[data-node-id="sys-cmd"] > rect.web-border').getAttribute('width'),'120');
  assert.equal(await page.locator('[data-node-id="sys-cmd"] > rect.web-border').getAttribute('height'),'36');
  await page.screenshot({path:'compact-preview.png'});
  console.log('PASS: Windows icons contain visible pixels, compact cards show truncated full names up to twelve characters.');
} finally {await browser.close();}
