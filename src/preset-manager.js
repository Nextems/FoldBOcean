const esc=value=>String(value).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
export function mountPresetManager(container,{repository,getSnapshot,apply,status,readOnly=false,native=false}){
  let records=[],busy=false;
  container.innerHTML=`<div class="eyebrow">ВАШИ КОНФИГУРАЦИИ</div><div class="heading"><div><h1>Пресеты</h1><p>Группы, программы, расположение, оформление и параметры вызова.</p></div></div><div class="preset-toolbar"><button class="primary" id="save-preset" ${readOnly?'disabled':''}>＋ Сохранить текущий пресет</button><button id="refresh-presets">Обновить список</button><span id="preset-count"></span></div><label class="preset-search">Поиск<input id="preset-search" type="search" placeholder="Название пресета" autocomplete="off"></label><p class="preset-location">${native?'Файлы: %APPDATA%\\com.spiderweb.settings\\presets · каждый пресет хранится в отдельном JSON.':'Предпросмотр: пресеты сохраняются в этом браузере. В приложении они хранятся в отдельных JSON-файлах на компьютере.'} Ограничения по количеству нет.</p><div id="preset-list" aria-live="polite">Загрузка…</div><dialog id="preset-dialog"></dialog>`;
  const list=container.querySelector('#preset-list'),search=container.querySelector('#preset-search'),modal=container.querySelector('#preset-dialog');
  function controls(){
    container.querySelector('#save-preset').disabled=busy||readOnly;
    container.querySelector('#refresh-presets').disabled=busy;
    container.querySelectorAll('[data-preset-action]').forEach(button=>button.disabled=busy||(readOnly&&['apply','update'].includes(button.dataset.presetAction))||Boolean(records.find(r=>r.id===button.dataset.id)?.error&&button.dataset.presetAction!=='delete'));
  }
  function draw(){
    const query=search.value.trim().toLocaleLowerCase();const filtered=records.filter(record=>record.name.toLocaleLowerCase().includes(query));
    container.querySelector('#preset-count').textContent=records.length+' пресетов';
    list.innerHTML=filtered.length?filtered.map(record=>`<article class="preset-card" data-preset-id="${esc(record.id)}"><div class="preset-info"><h2 title="${esc(record.name)}">${esc(record.name)}</h2><p>${record.error?esc(record.error):`Групп: ${record.groups} · программ: ${record.apps} · ${esc(new Date(record.updatedAt).toLocaleString('ru-RU'))}`}</p></div><div class="preset-actions">${[['apply','Применить'],['update','Обновить'],['rename','Переименовать'],['delete','Удалить']].map(([action,label])=>`<button data-preset-action="${action}" data-id="${esc(record.id)}">${label}</button>`).join('')}</div></article>`).join(''):`<div class="empty"><h2>${records.length?'Ничего не найдено':'Пресетов пока нет'}</h2><p>${records.length?'Измените поисковый запрос.':'Настройте панель и сохраните её под своим именем.'}</p></div>`;
    controls();
  }
  async function refresh(){records=await repository.list();if(container.isConnected)draw();}
  async function run(action){
    if(busy)return false;busy=true;controls();
    try{await action();return true;}catch(error){status(String(error.message||error),true);return false;}
    finally{busy=false;if(container.isConnected)controls();}
  }
  function confirm(heading,description,button){
    modal.innerHTML=`<form method="dialog"><h2>${esc(heading)}</h2><p>${esc(description)}</p><div class="modal-footer"><button value="cancel">Отмена</button><button class="primary" value="yes">${esc(button)}</button></div></form>`;
    modal.returnValue='';modal.showModal();
    return new Promise(resolve=>modal.addEventListener('close',()=>resolve(modal.returnValue==='yes'),{once:true}));
  }
  function askName(record){
    modal.innerHTML=`<form><h2>${record?'Переименовать пресет':'Сохранить пресет'}</h2><label>Название<input name="presetName" maxlength="80" required value="${esc(record?.name||'')}" placeholder="Например, Работа" autofocus></label><p class="form-error" role="alert"></p><div class="modal-footer"><button type="button" class="cancel">Отмена</button><button type="submit" class="primary">Сохранить</button></div></form>`;
    modal.querySelector('.cancel').onclick=()=>modal.close();
    modal.querySelector('form').onsubmit=async event=>{
      event.preventDefault();if(busy)return;const name=event.target.elements.presetName.value.trim();
      if(!name){modal.querySelector('.form-error').textContent='Введите название.';return;}
      const submit=modal.querySelector('[type=submit]');submit.disabled=true;
      const done=await run(async()=>{
        if(record)await repository.rename(record.id,name);else await repository.save(name,await getSnapshot());
        await refresh();status(record?'Пресет переименован':'Пресет сохранён');
      });
      if(done)modal.close();else if(modal.isConnected){submit.disabled=false;modal.querySelector('.form-error').textContent='Не удалось сохранить. Подробности в строке состояния.';}
    };
    modal.showModal();
  }
  container.querySelector('#save-preset').onclick=()=>askName();
  container.querySelector('#refresh-presets').onclick=()=>run(refresh);
  search.oninput=draw;
  list.onclick=async event=>{
    const button=event.target.closest('[data-preset-action]');if(!button||button.disabled||busy)return;
    const record=records.find(r=>r.id===button.dataset.id);if(!record)return;const action=button.dataset.presetAction;
    if(action==='rename'){askName(record);return;}
    const texts={apply:['Применить пресет?',`«${record.name}» заменит текущие группы, программы, расположение и настройки.`, 'Применить'],update:['Обновить пресет?',`Сохранённый пресет «${record.name}» будет заменён текущей конфигурацией.`, 'Обновить'],delete:['Удалить пресет?',`Удалить «${record.name}» с этого компьютера? Текущая панель сохранится.`, 'Удалить']};
    if(!await confirm(...texts[action])||!container.isConnected)return;
    await run(async()=>{
      if(action==='apply'){const preset=await repository.load(record.id);if(await apply(preset.config))status('Применён пресет: '+record.name);return;}
      if(action==='update')await repository.save(record.name,await getSnapshot(),record.id);
      if(action==='delete')await repository.delete(record.id);
      await refresh();status(action==='delete'?'Пресет удалён':'Пресет обновлён');
    });
  };
  run(refresh);
}
