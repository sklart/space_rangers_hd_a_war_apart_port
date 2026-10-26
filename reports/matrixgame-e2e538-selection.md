# MatrixGame E2E-5.38 — original input и logical viewport clipping

**INPUT / VIEWPORT CLIP SOFTWARE PASS; SWITCH RUNTIME PARTIAL.**
Original Init и игровой сценарий исполнялись на Windows HAL. Native Switch
Init, загрузка карты и первый Present на устройстве не исполнялись.
Новых NRO и аппаратных проверок нет; commit/push не выполнялись.

## Оригинальный текст

Text::Render / DeviceIdentity уже реализованы через исходный Font.2Ranger;
этот проход заново проверил production backend на HOST ASan/UBSan и
ARM64/QEMU: по 17940 glyph/layout cases и original Text::Render до bitmap,
upload failure/retry, ownership, cache rebind и managed reupload.
Большой Windows font oracle переиспользован при неизменных rasterizer/assets.
[Отчёт и evidence текста](matrixgame-text-current.md).

## Два воспроизведённых дефекта

1. Switch ZR задаёт key KA_FIRE, по умолчанию VK_LBUTTON. Original Keyboard
   и MouseKey меняют один Input bit. Отпускание ZR прекращало ROT_FIRE при
   удерживаемой мыши: ReadEngineKeys()[1]=0, ROT_FIRE=0. Baseline сохранён в
   build/matrixgame/e2e538-selection/fire-alias-reproduced.
   Router теперь согласует владельцев VK_L/R/MBUTTON после исходных callbacks,
   не генерируя дополнительные Keyboard/MouseKey события. B_DOUBLE сохраняет
   исходное отсутствие onKeyDown; Focus/Reset очищают alias latch.

2. После настоящего движения робота original CMinimap::Draw создаёт закрытый
   LINESTRIP рамки камеры с вершинами за viewport. В 6 из 114 draws GLSL/native
   physical viewport допускал пиксели x=158 за исходной границей [13,158).
   Диагностика: local cases 25/26/30 в обоих сеансах, всего 10 лишних пикселей,
   max channel delta 216. Причина установлена в контракте recorder/reference:
   half-pixel physical viewport сдвигался, implicit logical clip не сохранялся.
   Record теперь задаёт hardware scissor исходного logical viewport на каждом
   draw; GL reference выполняет тот же контракт. Геометрия не меняется,
   D3DRS_SCISSORTESTENABLE остаётся отдельным неподдержанным режимом.
   Clear сохраняет собственные rectangles и не наследует draw clipping.
   Baseline FAIL и точная диагностика сохранены в line-failure-diagnostic.

Первый неуспешный arcade test ошибочно требовал входа именно в anchor object
рамки. Оригинал выбирает current object всей выделенной группы. Тест исправлен
по исходным Keyboard → CreateGroupFromCurrent → EnterRobot(false); записи
в selection, orders или координаты робота не добавлялись. Отрицательный
прогон сохранён отдельно в fire-alias-baseline. Это не production bug.

## Что исполнялось

| Категория | Результат |
| --- | --- |
| WINDOWS HAL | Два полных original Init → RobotPreload → stor.Load → PrepareMap → form loop → Deinit. Выделение настоящего robot AI рамкой; вход в текущего робота группы; original forward order и движение 18.483131 единицы; original fire, перекрывающиеся mouse/ZR holds, выход в RTS, merged drag, focus release/recreate. По 38 дополнительных Takt/Draw / 950 ms; прежний loop 8 Draw / 125 ms также сохранён. |
| HOST | ASan/UBSan: три aliases, оба порядка отпускания, typematic, focus/Reset, исходные Form/Input. Replay 227586 actual draw calls / 159694 unique-per-session state snapshots, 0 failures. Отдельно logical clip и прежние LASTPIXEL/AA/scissor guards. |
| ARM64/QEMU | Те же input assertions, map state replay и logical clip guards PASS. Не libnx HID и не deko GPU. |
| Desktop HAL/GLSL | После исправления те же 114 original captured line draws / 54720000 RGBA pixels exact, max delta 0. Дополнительно 398 line cases и 24 triangle/strip/fan viewport cases exact. Baseline capture bytes сохранены. |
| COMPILE/LINK | Обновлённая полная devkitA64 native-entry ELF: 183 own reachable link inputs, 0 strong undefined; 255 generated originals побайтно совпадают с предыдущим этапом. Повторная сборка после final Switch clip изменения выполнена. |
| SWITCH HARDWARE | NOT RUN / HARDWARE_PENDING. Нет доказательства native map upload, изображения, Present, FPS, VRAM или аппаратного управления. |

Windows runtime использовал controlled events, не OS HID. Upload в CPU text
проверках — явная fault model. State replay проверяет production validators,
а не исполняет native GPU draw. Изменение clipping после полного Windows
сценария относится к Switch recorder и desktop GL reference; original D3D9
provider не менялся. Поэтому карта повторно не загружалась ради этого изменения;
захваченная геометрия проверена заново на final clip contract.

Параметр IzvratMS в настоящих сеансах равен 0. Необязательный selection
mask/wireframe pass не исполнялся и не объявляется закрытым.

[Сводное машиночитаемое evidence](matrixgame-e2e538-selection-evidence.json).
[Единый Init/cleanup план](../docs/matrixgame-init-sequence.md).

## Где остановилась настоящая native цепочка

На Switch подтверждён COMPILE/LINK, не вызов CGame::Init. Первый точный
аппаратный блокер не наблюдался. Следующий обязательный gate — исполнить
platform/window/device creation → original Init → RobotPreload → stor.Load →
PrepareMap → GPU upload → Draw → Present с реальными allocation/lifetime.
По Windows/CPU результатам называть карту Switch загруженной нельзя.

Сохранены protected fingerprints основной/root NRO, frozen-комплектов,
аппаратных входов, robots.pkg, CFG и frozen GI source. Ресурсы/сейвы не
редактировались, upstream pin чистый. P0, GI repeating-text causality,
manual-publish ENOENT, исторические Switch hangs и E2E-5.34 raw HAL retry FAIL
остаются OPEN. Прошедшие адресные проверки их не закрывают.
