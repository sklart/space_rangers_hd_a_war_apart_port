# MatrixGame — текущая последовательность полного native Init

Срез E2E-5.35, 26.10.2026. Это карта оригинального кода и имеющихся
доказательств. **Полный native Init на Switch не запускался.** Исторический
срез E2E-5.21 содержит уже снятые guards; актуальная граница приведена здесь.

## От процесса к первой карте

| Порядок / исходный потребитель | Операции и native provider | Владелец и cleanup | Доказательство |
| --- | --- | --- | --- |
| 1. Standalone entry | Активный HID; Npad initialize, IDs No1/Handheld, FullCtrl styles. Applet visibility baseline; настоящий NWindow. | Политика Npad принадлежит standalone-процессу, заканчивается с его applet/HID lifetime. Borrowed SwitchInput не перенастраивает хозяина. | E2E-5.35 SDK compile/link; IPC и контроллеры HARDWARE_PENDING. |
| 2. SwitchWindow, DekoDevice | Device/queue, material shaders, presentation color/depth, swapchain, texture arena. | Внешний владелец живёт дольше NativeInitRuntime. Device завершает работу очереди перед уничтожением backing. NWindow borrowed. | SDK link; адресные CPU allocator/transaction тесты. Создание GPU/VI на Switch не исполнялось. |
| 3. NativeInitRuntime::InitializeNativeGame → InitializeOriginalGame | Проверка drawable/identity, typed settings без fake D3D pointers; private output scope; buffer/cache/draw/skin contexts. | Runtime; исключение запускает Deinit. Private output живёт до окончания map/cache cleanup. Входные файлы разрешаются исходным CFile относительно resource CWD/package. | E2E-5.27 output tests, прежние native-entry contracts. Реальный resource root на устройстве ещё не проверен; root не объявляется обнаруженным блокером. |
| 4. CGame::Init | seed → static_init → heap → AddPackFile/OpenPackFiles DATA/robots.pkg → scoped CLoadProgress → CFG/DAT → MatrixData/replacements → CacheInit/settings. | Heap/cache/package принадлежат original game owner; локальные CStorage/CLoadProgress разматываются по C++. | Прежний полный WINDOWS HAL Init, CPU storage/cache тесты. Native цепочка только compile/link. |
| 5. original_game::EnterPlatform → AttachDeviceImpl | Native window attachment/HID/focus; ReadCaps shader checks; AvailableTextureBytes arena budget; CaptureState presentation/render/surface lease. | Restore lease и attachment runtime; provider обязан пережить restore. | CPU ownership/failure/restore tests, SDK. Физический capture/restore Switch не исполнен. |
| 6. CRenderPipeline, Window::Show/Update, g_Config/g_Sampler | Оригинальный renderer selection; applet pump; defaults/params/settings/samplers/max camera distance. | Game heap, platform attachment; pipeline уничтожается при Deinit. | WINDOWS HAL оригинальная ветка; native routing SDK. |
| 7. HNew CMatrixMapLogic → CMatrixMap | Члены в порядке объявления: m_RN, m_Console, m_DI; позднее m_Transition/m_Minimap, m_Cursor, m_Camera. Затем тело CMatrixMap (включая CSound::Init), затем тело CMatrixMapLogic. g_MatrixMap публикуется после конструктора. | Map; при constructor throw уничтожаются уже построенные члены. | Порядок проверен по MatrixMap.hpp/.cpp и MatrixLogic.cpp. Полный native конструктор не исполнялся. |
| 8. CMatrixDebugInfo → df::Create → debug_font::CreateFor | Исходные atlas/glyphs/metrics, upload, draw, lost/reset/release. CMatrixCursor отдельно сохраняет исходный cursor. | m_DI/m_Cursor; деструкторы map до освобождения heap/device. | Адресные HOST/ARM64/Windows tests и SDK. Не путать debug font с UI Font.2Ranger. |
| 9. CMatrixMap::LoadSide; CIFaceList; RobotPreload | Оригинальные side textures, interface list; VO heads/chassis/hulls/weapons, skin callbacks, pneumatic data. | Cache/skin leases и map/interface владельцы. | В сохранённом E2E-5.33 WINDOWS HAL журнале RobotPreload complete, два сеанса. GPU бюджет Switch не доказан. |
| 10. CGame::Init: CStorage stor; stor.Load(mapname) | Имя из override либо Config/Map, исходный parser/package. | Локальный storage; данные копируются/принимаются исходным PrepareMap. | WINDOWS HAL map storage loaded ×2; не новый native stor.Load. |
| 11. CMatrixMap::PrepareMap | Material/properties, UnitInit/points/moves/normals/IDs, texture unions BC1/mips; GroupBuild/terrain/water/bridges/surfaces/dynamics/road network/visibility/StaticPrepare/shadows. | Map/groups/cache/textures/buffers; original Clear/деструкторы. GPU uploads принадлежат native texture store/queue. | E2E-5.22–5.31 адресные encode, surfaces, material/fog/stencil tests; WINDOWS HAL PrepareMap complete ×2, карта 276×285, groups 28×29. На Switch не исполнено. |
| 12. CGame::Init после PrepareMap | CalcCannonPlace, AIRobot types, UI config/history, hint bitmaps, popup arrays. | Original game globals/heap; Deinit освобождает в исходном порядке. | Прежний полный WINDOWS HAL Init. |
| 13. CGame::ApplyVideoParams | ReadCreationParameters/display/VBlank, window geometry/style, multisample validation, Reset presentation, viewport/default material/light. Вызывается после PrepareMap. | Device transaction, old backing до успешного Attach/Commit; runtime baseline lease. | E2E-5.23 production CPU transaction + SDK; E2E-5.34 ownership candidate HOST/QEMU PASS. Raw HAL state-block retry FAIL остаётся отдельным открытым вопросом. Switch reset/queue failure recovery не доказан. |
| 14. CInterface::Load / Top/mm → SStateImages::SetStateText → Text::Render | Original Font.2Ranger/Rangers.ttf и запрошенные Verdana sizes; native font assets/metrics/kerning/layout/raster/texture upload/readback. | Original font cache, registered font/surface/texture leases; ClearFontCache до teardown device. | E2E-5.24: 17940 HAL exact comparisons; original Text::Render до bitmap HOST/ARM64, upload failure/retry/rebind. Switch GPU не исполнен. |
| 15. CGame::Init финал | Menu/constructor buttons; optional Rangers transition BuildTexture; RenderToPrimaryScreen; InitEffects; CreatePoolDefaultResources(true); InitObjectsLights; selection/cursor/dialog. | Map/effects/textures и transition. Offscreen scopes удерживают bindings до restore. | Полный WINDOWS HAL Init; native surfaces/shadow/stored-state routes SDK и адресные CPU tests. Native transition/offscreen/GPU runtime остаётся открытым. |
| 16. Native form Session → RunOriginalNativeLoop | FormChange/Enter; applet/HID pump; original L3GRun scheduler → original form Takt/Draw. Acquire перед Draw, original Clear/BeginScene/map Draw/EndScene/Present; rollback незавершённого кадра. | Form Leave/destructor до runtime Deinit; device/window живы весь цикл. | E2E-5.33 HOST/QEMU scheduler и WINDOWS HAL form/focus/Takt/Draw tests. Непрерывный Switch loop и управление боем не подтверждены. |

## Recovery и cleanup

CMatrixMap::ReleasePoolDefaultResources создаёт **собственный CDeviceState**,
вызывает StoreState и освобождает effects/water/minimap/default textures/buffers.
CheckLostDevice обращается к CooperativeLevel, при необходимости к Reset,
после чего CreatePoolDefaultResources(false) восстанавливает ресурсы и scalar
state. Этот CDeviceState не удерживает текстуры и не создаёт D3D9 state block.
Native outer CaptureState — отдельный lease с bindings/surface ownership.

E2E-5.34 raw HAL oracle с CreateStateBlock исследует более широкий сценарий.
Два его retry FAIL не закрыты и не объявлены воспроизведением отказа original
map recovery. Применимость ограничена различием этих двух контрактов.

При завершении: form Leave/destructor → отмена незавершённого GPU кадра →
original CGame::Deinit (AI/config/pipeline/hints/map/data/pneumatic/interface/
history/menu/popups/labels/instancing/package/heap) → оставшиеся runtime owners →
font cache и restore device lease → platform attachment/cursor/focus cleanup →
buffer/cache/statics/output scope → внешний Device/Window. CGame::SafeFree —
отдельный original entry, не имя этого runtime cleanup.

## Открытые операции и следующий точный блокер

E2E-5.36 воспроизвёл CMinimap::Draw → LINESTRIP / LASTPIXEL=1 guard на
настоящем map stream. E2E-5.37 реализовал aliased native line path: width 1,
stipple/smoothing off, реальные deko primitives. 398 addressed и 20 original
minimap draw HAL/GLSL comparisons exact; 33690 state replay HOST/QEMU PASS,
два полных Windows сеанса PASS, SDK link PASS. LASTPIXEL=FALSE и AA остаются
Unsupported. Native Switch runtime по-прежнему PARTIAL.
[Baseline](../reports/matrixgame-e2e536-actual-draw.md) и
[кандидат с проверками](../reports/matrixgame-e2e537-lines.md).

Первый runtime blocker настоящего Switch Init **не наблюдался**: запуска не было.
Все перечисленные provider routes программно связаны, но это не PASS Init.
Нужны реальные allocation/upload/resource lifetime, PrepareMap и первый
Draw/Present на устройстве. После первой карты — камера/RTS/direct control,
длительный бой, результат и восстановление основной игры.

Текущая проверка Text::Render / DeviceIdentity: исходный Font.2Ranger,
новые HOST ASan/UBSan и ARM64/QEMU прогоны по 17940 goldens, новая полная
SDK ELF. Windows font oracle переиспользован при неизменных rasterizer/assets.
[Данные и границы доказательств](../reports/matrixgame-text-current.md).

E2E-5.38 продвинул проверку настоящего original form после PrepareMap:
выделение реального робота рамкой, вход в текущего робота группы, движение,
стрельба, выход в RTS, объединённый mouse/pad drag и потеря/возврат фокуса.
Воспроизведён и исправлен native Router alias: keyboard ZR/KA_FIRE и MouseKey
используют один Input::VK_LBUTTON; отпускание одного владельца сбрасывало
удержание другого. Теперь владельцы трёх mouse/key bits согласуются после
исходных callbacks без дополнительных игровых событий; B_DOUBLE сохранён.
Это Windows/CPU проверка интеграции, не аппаратное управление Switch.
После движения робота original CMinimap::Draw также воспроизвёл выход линии
в колонку x=158 за viewport [13,158). Native recorder теперь задаёт исходное
logical viewport clipping независимо от physical half-pixel placement и
D3DRS_SCISSORTESTENABLE. Те же 114 captured draws HAL/GLSL exact; 24 triangle
viewport cases и 398 line cases PASS. Clear rects и original geometry сохранены.
[Отчёт E2E-5.38](../reports/matrixgame-e2e538-selection.md).

E2E-5.39 измерил demand 92 настоящих original form frames на Windows HAL:
40 кадров требуют более 2048 primary draws, максимум 4038. Старый native
Record hard cap применим к этому потоку. Теперь 2048 packets / 256 MiB —
границы batch, не кадра: queue submit/flush/wait сохраняет сцену и acquired
image; packet/target owners освобождаются только после completion; следующий
draw заново связывает targets/state. Между batches нет Present или Clear.
HOST ASan/UBSan и ARM64/QEMU policy replay всех 208808 measured draws PASS,
новая SDK ELF PASS. Реальный GPU batching и native первый кадр HARDWARE_PENDING.
[Baseline, реализация и пределы доказательств](../reports/matrixgame-e2e539-batches.md).

E2E-5.40: private native recorder больше не зависит от единственного 2 MiB
command block. Настоящий SDK add-memory callback добавляет owned pages;
command pressure вызывает batch drain без изменения сцены/image. При собственном
allocation failure запись отклоняется до queue submit; из C callback исключение
не выходит. После completion/abort SDK переводится на постоянную base page до
освобождения spills. Base 2 MiB + normal quota до 16 MiB + отдельный real discard
reserve 16 MiB; максимум backing 32 MiB. HOST ASan/UBSan, ARM64/QEMU и SDK PASS;
реальный GPU growth/VRAM и first native frame HARDWARE_PENDING.
[Реализация, failure model и SDK contract](../reports/matrixgame-e2e540-command-memory.md).

F9 в CFormMatrixGame::Keyboard явно Unsupported, как и GDI screen capture
borrowed transition при интеграции с хозяином. Это не обязательные операции
первого standalone Init. Queue-loss recovery остаётся открытым: атомарный CPU
rollback не доказывает восстановление потерянного аппаратного устройства.

E2E-5.41 PARTIAL: до BeginSession создаётся explicit resource read scope;
он сохраняется через Init/frames/recovery до окончания original package/cache
cleanup. HOST/ARM64 actual CFG/CStorage.Load и SDK PASS. Первый полный Windows
Init/PrepareMap/Draw/Deinit из foreign cwd PASS, но второй missing-package Init
падает на failure/rollback path с heap corruption: Windows gate FAIL.
[Следующий точный программный блокер](../reports/matrixgame-e2e541-resource-root.md).

Ссылки: [readiness plan](matrixgame-readiness-plan.md),
[E2E-5.33 runtime evidence](../reports/matrixgame-e2e533-form.md),
[E2E-5.34 reset evidence](../reports/matrixgame-e2e534-reset-owners.md),
[E2E-5.35 standalone input](../reports/matrixgame-e2e535-standalone-input.md).
