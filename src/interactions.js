import {mountAutostart} from './autostart.js';
const interactionDefaults={hoverDelayMs:180,closeDelayMs:350};
const escape=value=>String(value).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
export function mountInteractions(properties,{getConfig,persist,status,getMonitors,draw}) {
    const preferences = { ...interactionDefaults, ...getConfig().launcher };
    let captured = preferences.trigger.type === 'keyboard' ? preferences.trigger : null;
    const label = t => [t.modifiers & 1 ? 'Ctrl' : '', t.modifiers & 2 ? 'Alt' : '', t.modifiers & 4 ? 'Shift' : '', t.label || String.fromCharCode(t.key)].filter(Boolean).join(' + ');
    properties.innerHTML = `<h2>Взаимодействие</h2><form id="interaction-form"><label>Монитор<select name="monitor"><option value="primary">Основной монитор Windows</option>${preferences.monitor&&preferences.monitor!=='primary'?`<option value="${escape(preferences.monitor)}">Сохранённый монитор</option>`:''}</select></label><p class="editor-note">Панель открывается по центру выбранного экрана. Если он отключён — на основном.</p><label>Режим<select name="mode"><option value="hold">Удержание</option><option value="toggle">Нажатие</option></select></label><label>Бинд<select name="trigger"><option value="middle">Средняя кнопка мыши</option><option value="x1">Кнопка мыши 4</option><option value="x2">Кнопка мыши 5</option><option value="keyboard">Клавиатура</option></select></label><label id="web-key-label">Сочетание<input name="key" readonly placeholder="Нажмите сочетание"></label><label>Задержка раскрытия, мс<input name="hoverDelayMs" type="number" min="0" max="1500" step="10" value="${preferences.hoverDelayMs}" required></label><label>Задержка сворачивания, мс<input name="closeDelayMs" type="number" min="0" max="2000" step="10" value="${preferences.closeDelayMs}" required></label><p>Кольца раскрываются наведением. Esc закрывает панель, ПКМ возвращает предыдущий слой.</p><p class="form-error" role="alert"></p><button class="primary" type="submit">Сохранить взаимодействие</button></form>`;
    const form = properties.querySelector('form');
    form.elements.monitor.value = preferences.monitor || 'primary';
    getMonitors().then(monitors=>{
      if(!form.isConnected)return;
      const selected=form.elements.monitor.value;
      form.elements.monitor.innerHTML='<option value="primary">Основной монитор Windows</option>'+monitors.map(m=>`<option value="${escape(m.id)}">Экран ${m.number} · ${m.width}×${m.height}${m.primary?' · основной':''}</option>`).join('')+(!monitors.some(m=>m.id===selected)&&selected!=='primary'?`<option value="${escape(selected)}">Сохранённый монитор · отключён</option>`:'');
      form.elements.monitor.value=selected;
    }).catch(error=>{if(form.isConnected)status('Не удалось получить мониторы: '+String(error),true);});
    form.elements.mode.value = preferences.mode;
    form.elements.trigger.value = captured ? 'keyboard' : preferences.trigger.button;
    form.elements.key.value = captured ? label(captured) : '';
    const update = () => properties.querySelector('#web-key-label').hidden = form.elements.trigger.value !== 'keyboard';
    update(); form.elements.trigger.onchange = update;
    mountAutostart(properties,{status});
    form.elements.key.onkeydown = event => {
      event.preventDefault();
      if (['Control', 'Alt', 'Shift', 'Meta'].includes(event.key) || event.metaKey || event.key === 'Escape' || event.keyCode < 8 || event.keyCode > 254) return;
      captured = { type: 'keyboard', key: event.keyCode, modifiers: (event.ctrlKey ? 1 : 0) | (event.altKey ? 2 : 0) | (event.shiftKey ? 4 : 0), label: event.key === ' ' ? 'Space' : event.key.toUpperCase() };
      form.elements.key.value = label(captured);
    };
    form.onsubmit = async event => {
      event.preventDefault();
      if (form.elements.trigger.value === 'keyboard' && !captured) { properties.querySelector('.form-error').textContent = 'Сначала задайте сочетание.'; return; }
      getConfig().launcher = { ...preferences, monitor: form.elements.monitor.value, mode: form.elements.mode.value, trigger: form.elements.trigger.value === 'keyboard' ? captured : { type: 'mouse', button: form.elements.trigger.value } };
      for (const key of Object.keys(interactionDefaults)) getConfig().launcher[key] = Number(form.elements[key].value);
      draw(); if (await persist()) status('Взаимодействие сохранено');
    };
  }

