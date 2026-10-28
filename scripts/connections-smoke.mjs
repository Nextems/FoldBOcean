import {chromium} from 'playwright';
import {readFile} from 'node:fs/promises';
import assert from 'node:assert/strict';
const native=JSON.parse(await readFile(new URL('../release/rounded-connections.json',import.meta.url),'utf8'));
const browser=await chromium.launch({channel:'msedge',headless:true});
try {
  const page=await browser.newPage({viewport:{width:1200,height:900}});await page.goto('http://127.0.0.1:1420');
  const rendered=await page.evaluate(async()=>{
    const {roundedConnections,connectionPath}=await import('/src/connections.js');
    const edges=[{a:{x:0,y:0},b:{x:120,y:0}},{a:{x:120,y:0},b:{x:220,y:180}}],nodes=[{x:0,y:0},{x:220,y:180}];
    const paths=roundedConnections(edges,nodes);
    document.body.innerHTML=`<svg xmlns="http://www.w3.org/2000/svg" viewBox="-20 -20 280 240" width="560" height="480" style="background:#1f2123">${paths.map(p=>`<path d="${connectionPath(p)}" fill="none" stroke="#7b7f85" stroke-width="1.4"/>`).join('')}</svg>`;
    return {paths,d:paths.map(connectionPath)};
  });
  assert.ok(rendered.d[1].includes('C '));
  for(let i=0;i<native.length;i++)for(const field of ['start','end',...(native[i].curved?['join','c1','c2']:[])])for(const axis of ['x','y'])assert.ok(Math.abs(native[i][field][axis]-rendered.paths[i][field][axis])<0.001,'Editor and launcher curves match');
  await page.locator('svg').screenshot({path:new URL('../rounded-preview.png',import.meta.url).pathname.replace(/^\/([A-Za-z]:)/,'$1')});
  await page.goto('http://127.0.0.1:1420');await page.locator('#web-tab').click();await page.locator('[data-layout="web"]').click();
  assert.ok(await page.locator('#web-drawing path.web-connection').count());assert.equal(await page.locator('#web-drawing > line').count(),0);
  assert.ok((await page.locator('.web-connection').first().evaluate(el=>getComputedStyle(el).fill))==='none');
  console.log('PASS: native and editor use identical tangent curves; the editor renders curved connections without fills or sharp polyline joints.');
}finally{await browser.close();}
