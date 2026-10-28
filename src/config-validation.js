import limits from './menu-limits.json' with {type:'json'};

export const capacityAtDepth=depth=>limits.capacities[Math.min(depth,limits.capacities.length-1)];
export function validateEntries(config){
  if(!config||config.version!==1||!Array.isArray(config.groups)||!Array.isArray(config.apps))throw new Error('Неподдерживаемый формат конфигурации');
  for(const key of ['launcher','radial','webStyle','rgb','appearance','layout'])if(Object.hasOwn(config,key)&&(!config[key]||typeof config[key]!=='object'||Array.isArray(config[key])))throw new Error('Некорректный раздел: '+key);
  const ids=new Set();
  for(const [collection,parent] of [['groups','parentId'],['apps','groupId']])for(const item of config[collection]){
    if(item&&Object.hasOwn(item,'noIcon')&&typeof item.noIcon!=='boolean')throw new Error('Некорректный режим подписи');
    if(!item||typeof item!=='object'||!Object.hasOwn(item,'id')||typeof item.id!=='string'||!/^[a-zA-Z0-9][a-zA-Z0-9_-]*$/.test(item.id)||item.id.length>limits.maxId)throw new Error('Некорректный id элемента');
    if(ids.has(item.id))throw new Error('Повторяющийся id элемента: '+item.id);ids.add(item.id);
    if(typeof item.name!=='string'||!item.name.trim()||item.name.includes('\0')||Array.from(item.name).length>limits.maxName)throw new Error('Имя должно содержать от 1 до 256 символов');
    if(!(collection==='groups'&&item[parent]===null)&&typeof item[parent]!=='string')throw new Error('Не указана группа элемента');
    if(!Number.isInteger(item.position))throw new Error('Позиция должна быть целым числом');
    if(collection==='apps'&&(typeof item.path!=='string'||!item.path.trim()||item.path.includes('\0')||item.path.length>limits.maxPath))throw new Error('Некорректный путь приложения: '+item.name);
  }
}
export function validateHierarchy(config){
  validateEntries(config);
  const parents=new Map(config.groups.map(g=>[g.id,g.parentId])),depths=new Map([[null,0]]);
  function depth(id){
    if(depths.has(id))return depths.get(id);
    const chain=[],seen=new Set();let next=id;
    while(!depths.has(next)){
      if(!parents.has(next))throw new Error('Родительская группа не найдена');
      if(seen.has(next))throw new Error('Циклическая вложенность групп');
      seen.add(next);chain.push(next);next=parents.get(next);
    }
    let value=depths.get(next);for(const group of chain.reverse())depths.set(group,++value);
    return depths.get(id);
  }
  for(const id of parents.keys())depth(id);
  const slots=new Map();
  for(const [collection,parentKey] of [['groups','parentId'],['apps','groupId']])for(const item of config[collection]){
    const parent=item[parentKey],max=capacityAtDepth(depth(parent));
    if(item.position<1||item.position>max)throw new Error(`«${item.name}»: в этом кольце допускается ${max} позиций. Перенос группы также меняет вместимость её потомков.`);
    const used=slots.get(parent)||new Set();if(used.has(item.position))throw new Error('В одном меню позиции не должны повторяться');used.add(item.position);slots.set(parent,used);
  }
  if(config.groups.filter(g=>g.parentId==='sys-prog').length>1)throw new Error('В sys prog разрешена только одна своя подгруппа');
}
