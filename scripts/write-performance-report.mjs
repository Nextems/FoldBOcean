import fs from 'node:fs';
import assert from 'node:assert/strict';
const read=file=>JSON.parse(fs.readFileSync(file,'utf8').replace(/^\uFEFF/,''));
const base='release/performance/';
const before=read(base+'before-quiet/summary.json'),after=read(base+'after-final/summary.json');
const average=a=>a.reduce((x,y)=>x+y,0)/a.length;
const f=n=>n.toFixed(2),m=(data,key,metric)=>data[key][metric].mean;
const rows=[['radial/tray-cold','Круг: свежий процесс в трее'],['radial/root','Круг: только корневые группы'],['radial/expanded','Круг: раскрыта системная группа'],['radial/expanded-rgb','Круг: раскрытая группа + RGB'],['radial/tray-warm','Круг: в трее после использования'],['web/tray-cold','Паутина: свежий процесс в трее'],['web/root','Паутина: только корневые группы'],['web/expanded','Паутина: раскрыта системная группа'],['web/expanded-rgb','Паутина: раскрытая группа + RGB'],['web/tray-warm','Паутина: в трее после использования']];
for(const data of [before,after])for(const [key] of rows){assert.equal(data[key].trials,3);if(!key.includes('tray'))assert.equal(data[key].visibleSamples,data[key].totalSamples);}
const memory=rows.map(([key,label])=>`| ${label} | ${f(m(before,key,'workingSetMiB'))} | ${f(m(after,key,'workingSetMiB'))} | ${f(m(before,key,'privateMiB'))} | ${f(m(after,key,'privateMiB'))} |`).join('\n');
const activities=[['radial/open-close','Круг: открытие/закрытие'],['radial/expanded-rgb','Круг: RGB'],['web/open-close','Паутина: открытие/закрытие'],['web/expanded-rgb','Паутина: RGB']];
const cpu=activities.map(([key,label])=>`| ${label} | ${f(m(before,key,'cpuMachinePercent'))} | ${f(m(after,key,'cpuMachinePercent'))} | ${f(m(before,key,'cpuOneCorePercent'))} | ${f(m(after,key,'cpuOneCorePercent'))} |`).join('\n');
const system=rows.filter(([k])=>!k.endsWith('tray-cold')||k.startsWith('radial')).map(([key,label])=>`| ${label} | ${Math.round(m(before,key,'interruptsPerSec'))} → ${Math.round(m(after,key,'interruptsPerSec'))} | ${f(m(before,key,'interruptPercent'))} → ${f(m(after,key,'interruptPercent'))} | ${f(m(before,key,'dpcPercent'))} → ${f(m(after,key,'dpcPercent'))} |`).join('\n');
const mouse=stage=>read(base+stage+'/mouse-idle.json').map(r=>average(r.samples.map(s=>s.launcherContextSwitchesPerSec)));
const mouseBefore=mouse('before-quiet'),mouseAfter=mouse('after-final');
const control=read(base+'system-control.json');
const metadata=read(base+'after-final/metadata.json');
const content=`# FoldBOcean: измерения и оптимизация 28.10.2026

Измерена нативная панель C++ в режимах круга и паутины, без открытого окна настроек Tauri. Итоговое сравнение выполнено после выключения пользователем VirtualBox. Предварительные серии с работающей виртуальной машиной и промежуточная сборка исключены из таблиц.

## Методика

Три отдельных запуска процесса для каждого режима до и после изменений. Одинаковый JSON: 2 корневые группы и 8 приложений; раскрывалась системная группа с 6 приложениями. Размер круга 120%, паутины 124%, анимация 200 мс, размытие 11/24. RGB выключен кроме отдельного сценария, где он включён со скоростью 3. Основные интервалы — не менее 4 секунд, цикл открытия и закрытия — не менее 12 секунд. Открытие корня и группы отделено от статического замера паузой в 1 секунду. Проверялось, что окно действительно видно во всех отсчётах открытых статических режимов.

ПК: ${metadata.logicalProcessors} логических процессоров, ${f(metadata.physicalMemoryGiB)} ГиБ RAM. Остальные приложения, в том числе живые обои, оставались запущены. Числа — среднее трёх средних по проходам, а не гарантированный расход для любой конфигурации. Полные интервалы, отсчёты, диапазоны и SHA-256 сборок сохранены в JSON.

Память измерена через Process.WorkingSet64 и PrivateMemorySize64. CPU — разность TotalProcessorTime, нормированная на фактическое время и число логических процессоров. Системные счётчики прочитаны через PDH, отдельно от процесса: Processor(_Total)\\Interrupts/sec, % Interrupt Time, % DPC Time. Переключения потоков панели — сумма Thread(FoldBOcean/*)\\Context Switches/sec с фильтрацией имени экземпляра.

## Память, МиБ

Working Set — резидентные страницы процесса, включая общие DLL. Private Bytes — выделенная приватная память (commit), которая не обязана целиком находиться в RAM. Это разные показатели; их нельзя складывать. Принудительное обрезание рабочего набора не применялось.

| Состояние | WS до | WS после | Private до | Private после |
|---|---:|---:|---:|---:|
${memory}

У круга снизился рабочий набор, но не все показатели одновременно: часть приватной памяти выросла из-за небольших кэшей и поведения графического heap. У паутины обычный режим практически не изменился; при RGB рабочий набор увеличился примерно на 1 МиБ, поскольку буфер переиспользуется между непрерывными кадрами. После остановки анимации без RGB и при скрытии панели он освобождается. Сохранённые Windows страницы DLL и внутренних кэшей объясняют отличие свежего процесса от процесса после использования.

## CPU

100% в колонках «ПК» означает все ${metadata.logicalProcessors} логических процессоров; 100% в колонках «одно ядро» — один полностью занятый логический процессор.

| Сценарий | ПК до, % | ПК после, % | Одно ядро до, % | Одно ядро после, % |
|---|---:|---:|---:|---:|
${cpu}

В неподвижных сценах без RGB и в трее во всех итоговых контрольных интервалах прирост CPU-времени оказался 0 мс на точности счётчика. Это не утверждение о математически нулевых затратах. У круга с RGB средняя нагрузка CPU уменьшилась примерно на ${Math.round(100*(1-m(after,'radial/expanded-rgb','cpuMachinePercent')/m(before,'radial/expanded-rgb','cpuMachinePercent')))}%; у паутины в цикле открытия/закрытия — на ${Math.round(100*(1-m(after,'web/open-close','cpuMachinePercent')/m(before,'web/open-close','cpuMachinePercent')))}%. Малые различия в других сценариях не считаются доказанным ускорением.

## Прерывания и переключения потоков

| Состояние | Interrupts/sec до → после | Interrupt Time, % до → после | DPC Time, % до → после |
|---|---:|---:|---:|
${system}

В дополнительном контрольном интервале при полностью завершённой панели: ${Math.round(average(control.map(s=>s.interruptsPerSec)))} Interrupts/sec, ${f(average(control.map(s=>s.interruptPercent)))}% Interrupt Time, ${f(average(control.map(s=>s.dpcPercent)))}% DPC Time. Это фон ПК, а не расход FoldBOcean.

Устойчивого снижения аппаратных прерываний вследствие правок не доказано: показатели меняются в обе стороны и относятся ко всей системе. Счётчик IRQ не равен числу пробуждений приложения. ISR/DPC выполняются отдельно от обычных потоков; [описание Microsoft](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/cpu-analysis). Выбранные системные показатели соответствуют [рекомендациям Performance Monitor](https://learn.microsoft.com/en-us/troubleshoot/windows-server/support-tools/troubleshoot-issues-performance-monitor).

Для панели отдельно проверен поток искусственных движений мыши через SendInput с паузой 10 мс, в скрытом состоянии с клавиатурным биндом. Фактическая частота зависит от планировщика. Три прохода до: ${mouseBefore.map(f).join(', ')} переключений/с; после: ${mouseAfter.map(f).join(', ')}. Среднее: **${f(average(mouseBefore))} → ${f(average(mouseAfter))} переключений/с**. Это измеренный эффект отключения ненужного mouse hook, а не уменьшение аппаратных IRQ. Для бинда на мышь перехват сохраняется.

## Что изменено

- Два полноразмерных временных изображения и копирование кадра заменены одним общим premultiplied DIB для GDI+ и UpdateLayeredWindow. Буфер переиспользуется только во время движения/RGB и освобождается в статике и при скрытии.
- Убраны пустые поля графического буфера круга. Размер рассчитан по достижимой глубине с запасом под анимацию; центр, пользовательский масштаб и уплотнение глубоких колец сохранены.
- Измерения глифов кэшируются на время текущей сцены. Во время RGB неподвижный текст кэшируется в небольших растровых областях с учётом масштаба. Кэши очищаются при изменении сцены и скрытии.
- Hit testing кольца больше не создаёт временную сцену с копией JSON. В статической паутине проверка блока не создаёт анимационный кадр. Векторы кадров сразу резервируют нужный размер, настройки материала читаются без копирования JSON.
- RGB-таймер не переустанавливается на каждом кадре и не проверяет неподвижный курсор каждый тик. После обычной анимации таймер выключается; длительности анимаций и целевые интервалы 16/33 мс сохранены.
- Для скрытой панели с клавиатурным биндом mouse hook снимается; при открытии или переключении на мышиный бинд возвращается. Наблюдение за JSON остаётся блокирующим, без опроса в цикле.
- При завершении окна графические ресурсы освобождаются до GdiplusShutdown. Функции приложения, пользовательские группы и программы не удалялись. Неиспользуемые preview-файлы на диске не влияют на RAM и не удалялись ради цифр.

Окно настроек было просмотрено на предмет фоновых циклов: анимации конечные, RGB-предпросмотр прекращается после удаления SVG; Tauri завершается при закрытии окна. Его код и сборка не менялись, отдельный профиль памяти WebView2 в это сравнение не включён.

## Проверки и артефакты

32 нативных теста прошли. Новые проверки охватывают повторное создание/освобождение DIB без роста GDI handles, premultiplied alpha, покрытие глифов в кэше при масштабах 100/120/150%, попадание в анимированные секторы и запас холста для глубоких колец. На живой сборке проверены клавиатурные hold/toggle, удержание СКМ и смена типа бинда без перезапуска.

- [Исходные отсчёты до](release/performance/before-quiet/raw.json), [после](release/performance/after-final/raw.json).
- [Сводка до с диапазонами](release/performance/before-quiet/summary.json), [после](release/performance/after-final/summary.json).
- [Мышиный ввод до](release/performance/before-quiet/mouse-idle.json), [после](release/performance/after-final/mouse-idle.json), [система без панели](release/performance/system-control.json).
- Исходная сборка сохранена в release/performance/original/FoldBOcean.exe.
- Итоговая сборка: release/FoldBOcean.exe, SHA-256 ${metadata.exeHash}; совместимая копия release/SpiderWeb.exe обновлена.

Для повторения: закройте настройки, сохраните текущую конфигурацию и запускайте из корня проекта \`pwsh -NoProfile -File scripts/profile-launcher.ps1 -Stage имя-серии -Trials 3 -Seconds 4\`. Скрипт временно меняет режим/RGB, открывает панель, двигает курсор и сохраняет оригинальный JSON в каталоге серии; после штатного завершения восстанавливает конфигурацию и запускает панель в трее. При принудительном завершении процесса скрипта используйте сохранённый config.original.json. Затем \`pwsh -NoProfile -File scripts/profile-mouse-idle.ps1 -Stage имя-серии\` и \`node scripts/summarize-performance.mjs имя-серии\`.
`;
const archive='docs/performance/2026-10-28/';
for(const file of ['before-quiet/raw.json','after-final/raw.json','before-quiet/summary.json','after-final/summary.json','before-quiet/mouse-idle.json','after-final/mouse-idle.json','system-control.json']){
  const target=archive+file;fs.mkdirSync(target.slice(0,target.lastIndexOf('/')),{recursive:true});fs.copyFileSync(base+file,target);
}
fs.writeFileSync('PERFORMANCE.md',content.replaceAll('](release/performance/',']('+archive));
console.log('PERFORMANCE.md written');
