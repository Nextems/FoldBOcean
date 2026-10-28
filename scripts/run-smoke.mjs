import {createServer} from 'vite';
import {spawn} from 'node:child_process';
import {readdir} from 'node:fs/promises';
const server=await createServer({server:{host:'127.0.0.1',port:1420,strictPort:true}});
try{
  await server.listen();
  const requested=process.argv.slice(2);
  const files=requested.length?requested:(await readdir(new URL('./',import.meta.url))).filter(name=>name.endsWith('-smoke.mjs')&&name!=='run-smoke.mjs').concat('smoke.mjs');
  const failures=[];
  for(const file of files){
    console.log('\nRunning '+file);
    const code=await new Promise((resolve,reject)=>{
      const child=spawn(process.execPath,['scripts/'+file],{stdio:'inherit',windowsHide:true});
      child.on('error',reject);child.on('close',resolve);
    });
    if(code!==0)failures.push(file);
  }
  if(failures.length)throw Error('Failed smoke tests: '+failures.join(', '));
}finally{await server.close();}
