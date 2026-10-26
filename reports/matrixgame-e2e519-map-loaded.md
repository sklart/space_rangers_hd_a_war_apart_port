# E2E-5.19: настоящий PrepareMap и граница после загрузки карты

Исторический срез. В [E2E-5.20](matrixgame-e2e520-first-frame.md) остановка
ResizeOuter преодолена; оригинальный Init и кадр карты завершились на Windows.

25.10.2026. Локальный кандидат, без commit/push. **PARTIAL**: загрузка CPU-мира
на настоящем Windows HAL состоялась; успешный возврат CGame::Init, первый кадр,
цикл и бой не выполнены. Нового Switch runtime нет.

Этот отчёт продолжает [Path и полную линковку](matrixgame-e2e519-path-full-windows.md).
Прежний `unowned skin provider` снят. Windows — контроль исходников, изменённых
для переноса, а не отдельный разрабатываемый продукт. Дальнейшая цель и порядок
работы: [план подсистем Switch](../docs/matrixgame-readiness-plan.md).

## Исполненная цепочка

Настоящие `Direct3DCreate9 → CreateDevice(HAL)` вернули успех. SDK readback
проверил material, viewport, light, render register 146 и восстановление
состояния. Полный оригинальный `CGame::Init` использовал настоящий robots.pkg
и исходный CFG (`Matrix\Map\desert1_3e.cmap`). Исполнены RobotPreload, успешный
storage Load и полный PrepareMap, включая реальные материалы/текстуры.

В обоих входах наблюдатель, расположенный после исходного PrepareMap, сообщил:

```text
RobotPreload complete borrowed_skin_objects=0 map_size=0x0 groups=0x0
map storage loaded borrowed_skin_objects=0 map_size=0x0 groups=0x0
PrepareMap complete borrowed_skin_objects=9 map_size=276x285 groups=28x29
Apply window mode
original Init boundary: window resize requires reset-safe presentation ownership
```

После каждого отказа оригинальный Deinit освободил map/heap; ledger заимствованных
материалов пуст, gamma readback совпал с исходным, state/timer leases пусты.
Driver завершился кодом **10** — ожидаемая незавершённая Init-цепочка, не PASS Init.
Вывод этапов не заменяет исходные алгоритмы. Возврат storage Load наблюдается,
но исходное управление потоком не меняется. Аудит сравнивает Init с предыдущим
полным кандидатом после удаления только наблюдателей.

Первый точный блокер:

`CGame::Init → CGame::ApplyVideoParams → Window::ResizeOuter`.

`ApplyVideoParams` входит в исходную ветку оконного режима. Win32 provider пока
наследует явный отказ ResizeOuter: для изменения presentation нужны согласованные
leases/reset/default-pool resources и восстановление состояния хозяина.
Runtime-стек разрешён по MAP именно этого EXE с учётом ASLR; при оптимизации
ApplyVideoParams может быть встроен, источник ResizeOuter проверяется отдельно.
Пропуск ApplyVideoParams или фиктивный успех Reset не использовались.

## Что изменено и почему

1. **Оригинальный CSkinManager остаётся владельцем SSkin.** Его блоки не копируются
   и не получают фиктивный shared_ptr owner; UV Takt меняет исходные материалы.
   BorrowedSkinScope регистрирует точный callback GetSkin на время игровой сессии.
   Внешний ledger связывает VO с weak epoch; Clear инвалидирует epoch до HFree.
   BeforeDraw/Draw отвергают истёкшее заимствование до dereference/callback.
   Unload снимает запись. Owned SkinLoadScope сохранён. Новых членов/изменения
   layout CVectorObject нет; его заголовки сверены с использованными Windows libs.
   Контракт действует на текущем потоке, как существующий callback context;
   произвольные fallback skin pointers вне VO этим не объявляются защищёнными.
2. **Восемь неверных texture casts заменены типизированным lookup.** Один в
   MatrixMap, три в OriginalMapPrepare и четыре в MatrixTerSurface обращались
   к CacheTexture как к CTextureManaged. Это давало реальный AV при Preload.
   legacy_texture::GetManaged возвращает настоящий original managed object
   через registry и сохраняет проверку несовместимого типа. Использованы реальные
   SDK texture operations; подмена тестовыми картинками отсутствует.
3. **Cleanup до выбора курсора.** Частичный Init при Deinit вызывал
   CMatrixSideUnit::PLDropAllActions → CMatrixCursor::SetVisible с ещё не созданным
   массивом иконок. Проверка массива и границ frame сохраняет исходный выбранный
   курсор, а до его выбора восстанавливает сохранённый window cursor.
4. **Необходимые операции Win32 Device.** Viewport/material/light/enable light
   передаются в настоящий D3D9 с явным преобразованием полей и HRESULT.
   Capture/restore явно сохраняет viewport/material. Render/sampler registers
   используют исходные таблицы 103/13 ключей; неизвестные ключи отвергаются.

## Адресная проверка

| Проверка | Результат и границы |
| --- | --- |
| Borrowed scope/ledger | Host ASan/UBSan/leaks, ARM64/QEMU, Windows x86 PASS: peers, nested callbacks, invalidation, scope end, retry/reentry |
| Оригинальный VO с реальным model-0.vo | На тех же runtime PASS: LoadSpecial/BeforeDraw, настоящее освобождение fixture SSkin, отказ до callback/Draw, Unload/retry; два цикла, preloads=4 |
| Оригинальный CSkinManager в полном Init | Реально выполнен на Windows HAL в PrepareMap; девять заимствующих VO, два чистых завершения после последующего отказа |
| devkitA64 | Десять изменённых TU compile PASS, ledger ELF link PASS, strong undefined=0; assertions включены; runtime NOT RUN, полный game SDK link не заявляется |
| Полная Windows-линковка | PASS: исходные Path/visibility/loop и новая ownership/provider цепочка; адресная сборка с хэшированными прежними libs, без большой повторной сборки |
| CMake | Configure/generate PASS; новые borrowed-skin и borrowed-skin-vo test targets, model path задаётся явно; полной пересборки движка не было |

VO-тест использует fixture callback и настоящий исходный загрузчик/модель;
он не объявляется проверкой оригинального CSkinManager. Менеджер отдельно
исполнялся в полном Windows Init. GPU Draw в этих CPU тестах не выполняется.
В Windows ledger fixture ранее одинаковые функции callbacks были объединены
`/OPT:ICF`; независимый второй callback сделан различимым, итоговый runtime PASS.
Исторические AV выше исправлены и не являются текущей границей.

В заключительном аудите команд обнаружен неверный порядок `-UNDEBUG` перед
`-DNDEBUG` в прежних SDK Path и Linux borrowed-skin runners. Повторены только
SDK Path compile/link и адресные host/ARM64 ledger/VO проверки с последним
`-UNDEBUG`. Итоговые results используют эти исправленные команды; более ранние
borrowed runs не служат доказательством assertions. Windows runners и Path
host/ARM64 driver уже имели правильный порядок флагов.

Path runtime-наборы и baseline повторно не запускались: тело Path текущего
полного кандидата совпадает с ранее проверенным, включая offset-before-free,
nullptr, принудительный move и allocation failures. Отдельные большие
CPU/ARM64 наборы прежних этапов также не повторялись.

## Открытые границы и сохранность

Первая CPU-загрузка карты на Windows доказана. Для первой карты на Switch
нужны native CGame entry/Window/Device providers вместо legacy D3D identities,
настоящий lifecycle и интеграция deko3d с материалами/текстурами/буферами.
Для первого отображённого кадра добавляются terrain/water/объекты/visibility,
исходные material passes и нужные эффекты. Standalone ordinal smoke не заменяет
этот контракт. Прежний Switch GPU smoke остановился до draw; SDK-исправление
allocator не закрывает аппаратный FAIL.

P0 основной игры: [frozen GI audit](e2e5/hardware-20261023-cache-frozen-audit.md)
подтвердил дефект shared-cache на нужном типе ресурса и в точных frozen sources.
Реальный вызов на дефектном экране и причинная связь с повторяющимся текстом
остаются OPEN; eviction policy не менялась. `manual-publish ENOENT` и Switch
hangs остаются OPEN. Новых аппаратных запусков нет.

Аудит повторно сверяет защищённые файлы, 107 аппаратных входов, robots.pkg и
исходный CFG. Основная NRO, старые комплекты, ресурсы, сейвы и hardware logs
не изменяются. Сгенерированные тестовые каталоги находятся только в build.

[Машинные результаты, исходные команды, хэши и разрешённый стек](matrixgame-e2e519-map-loaded-evidence.json).
Аудит: `python tools/matrixgame_map_loaded_audit.py`.

## Аудит исходного поручения

| Требование | Доказательство и результат |
| --- | --- |
| Path: nullptr, пустой/непустой список, move/failure, offsets до free | Исходный UpdateData захватывает index+1 до HAllocEx; runtime assertions и sanitizer результаты, шесть moves/четыре failures за два цикла; SDK compile/link отдельно |
| Подключить original Path и снять loop/visibility link dependencies | Полный Windows EXE линкуется с исходными Path/L3GRun/обеими visibility функциями; отдельные CMake game/bootstrap цели; runtime loop/visibility не заявлен |
| Window/Device: реальное устройство и загрузка карты | HAL HRESULT=0; original storage Load/PrepareMap возвращаются; 276×285, два чистых входа; после загрузки точный ResizeOuter блокер сохранён |
| P0 GI: применимость к ресурсу и frozen sources | 2RCbg Format2/background.pkg и cache source SHA совпадают; класс known eviction отличается; причинность повторяющегося текста OPEN |
| ENOENT/hangs и аппаратные границы | Оба исторических дефекта OPEN, Switch runtime не выполнялся; контроль сохранности входов пройден |
| Итог и общий план подсистем | Этот отчёт, evidence и связанный план Switch; общий процент не выведен из compile/test counters |
