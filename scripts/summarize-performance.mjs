import fs from 'node:fs';
import path from 'node:path';
const stages=process.argv.slice(2);
const mean=values=>values.reduce((a,b)=>a+b,0)/values.length;
const metrics=['workingSetMiB','privateMiB','gdi','user','handles','threads','interruptsPerSec','interruptPercent','dpcPercent','systemCpuPercent','systemContextSwitchesPerSec','launcherContextSwitchesPerSec'];
const result={};
for(const stage of stages){
 const folder=path.resolve('release/performance',stage),rows=JSON.parse(fs.readFileSync(path.join(folder,'raw.json'),'utf8').replace(/^\uFEFF/,''));
 const groups={};
 for(const row of rows){
  const key=row.mode+'/'+row.phase;(groups[key]??=[]).push(row);
 }
 const summary={};
 for(const [key,rows] of Object.entries(groups)){
  const entry={trials:rows.length};
  for(const metric of metrics){const values=rows.map(r=>mean(r.samples.map(s=>s[metric]).filter(v=>Number.isFinite(v)))).filter(Number.isFinite);entry[metric]={mean:mean(values),min:Math.min(...values),max:Math.max(...values)};}
  for(const metric of ['cpuOneCorePercent','cpuMachinePercent','processPeakWorkingSetMiB']){const values=rows.map(r=>r[metric]);entry[metric]={mean:mean(values),min:Math.min(...values),max:Math.max(...values)};}
  entry.visibleSamples=rows.flatMap(r=>r.samples).filter(s=>s.visible).length;
  entry.totalSamples=rows.reduce((n,r)=>n+r.samples.length,0);
  summary[key]=entry;
 }
 fs.writeFileSync(path.join(folder,'summary.json'),JSON.stringify(summary,null,2));result[stage]=summary;
}
for(const [stage,summary] of Object.entries(result)){
 console.log(stage);
 console.table(Object.entries(summary).map(([scenario,v])=>({scenario,n:v.trials,privateMiB:v.privateMiB.mean.toFixed(2),workingSetMiB:v.workingSetMiB.mean.toFixed(2),cpuCore:v.cpuOneCorePercent.mean.toFixed(2),cpuMachine:v.cpuMachinePercent.mean.toFixed(3),switches:v.launcherContextSwitchesPerSec.mean.toFixed(1),irq:v.interruptsPerSec.mean.toFixed(0),irqPercent:v.interruptPercent.mean.toFixed(3),dpcPercent:v.dpcPercent.mean.toFixed(3),visible:v.visibleSamples+'/'+v.totalSamples})));
}
