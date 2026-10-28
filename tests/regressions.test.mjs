import {test} from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import {validateHierarchy} from '../src/config-validation.js';
import {createConfigStore} from '../src/config-store.js';

for(const fixture of JSON.parse(fs.readFileSync(new URL('./config-cases.json',import.meta.url),'utf8'))){
  test(fixture.name,()=>{
    if(fixture.valid)assert.doesNotThrow(()=>validateHierarchy(fixture.config));
    else assert.throws(()=>validateHierarchy(fixture.config));
  });
}
test('failed write rolls back, cancels queued edits, and permits a new edit',async()=>{
  const written=[],restored=[];let fail=false;
  const store=createConfigStore({initial:{value:0},validate:()=>{},write:async value=>{if(fail)throw Error('disk full');written.push(value);},onRollback:(value,error)=>restored.push({value,error:error.message})});
  assert.equal(await store.save({value:1}),true);
  fail=true;
  const first=store.save({value:2}),stale=store.save({value:3});
  assert.deepEqual(await Promise.all([first,stale]),[false,false]);
  assert.deepEqual(restored,[{value:{value:1},error:'disk full'}]);
  assert.deepEqual(written,[{value:1}]);
  fail=false;assert.equal(await store.save({value:4}),true);
  assert.deepEqual(written,[{value:1},{value:4}]);
});
test('invalid edit never reaches storage; snapshots are independent',async()=>{
  const restored=[],writes=[];
  const store=createConfigStore({initial:{value:0},validate:value=>{if(value.value<0)throw Error('invalid');},write:async value=>writes.push(value),onRollback:value=>restored.push(value)});
  const candidate={value:1};const pending=store.save(candidate);candidate.value=-1;
  assert.equal(await pending,true);assert.equal(await store.save(candidate),false);
  assert.deepEqual(writes,[{value:1}]);assert.deepEqual(restored,[{value:1}]);
});
