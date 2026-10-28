// Failed saves roll back to the last confirmed snapshot and invalidate queued
// edits based on the failed state. A subsequent user edit starts a new batch.
export function createConfigStore({initial,validate,write,onRollback}){
  let confirmed=structuredClone(initial),queue=Promise.resolve(),generation=0;
  return {
    confirm(value){confirmed=structuredClone(value);},
    save(value){
      const snapshot=structuredClone(value),batch=generation;
      const result=queue.then(async()=>{
        if(batch!==generation)return false;
        try{validate(snapshot);await write(snapshot);confirmed=structuredClone(snapshot);return true;}
        catch(error){generation++;onRollback(structuredClone(confirmed),error);return false;}
      });
      queue=result.catch(()=>{});return result;
    }
  };
}
