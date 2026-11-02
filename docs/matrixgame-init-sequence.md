# MatrixGame — текущая последовательность полного native Init

Срез E2E-5.59, 26.10.2026. Это карта оригинального кода и имеющихся
доказательств. **Native Init на Switch начался и остановился в ApplyGammaRamp;
полный Init не завершился.** E558 содержит software candidate исправления,
ещё не проверенный на устройстве. Исторический
срез E2E-5.21 содержит уже снятые guards; актуальная граница приведена здесь.

## От процесса к первой карте

| Порядок / исходный потребитель | Операции и native provider | Владелец и cleanup | Доказательство |
| --- | --- | --- | --- |
| 1. Standalone entry | Активный HID; Npad initialize, IDs No1/Handheld, FullCtrl styles. Applet visibility baseline; настоящий NWindow. | Политика Npad принадлежит standalone-процессу, заканчивается с его applet/HID lifetime. Borrowed SwitchInput не перенастраивает хозяина. | E2E-5.35 SDK compile/link; IPC и контроллеры HARDWARE_PENDING. |
| 2. SwitchWindow, DekoDevice | Device/queue, material shaders, scene/presentation color/depth, swapchain, texture arena; E558 presentation gamma pass. | Внешний владелец живёт дольше NativeInitRuntime. Device завершает работу очереди перед уничтожением backing. NWindow borrowed. | E557 hardware: BC1 draw/readback/Present PASS. Раздельные scene images и gamma pass E558: software/SDK PASS, HARDWARE_PENDING. |
| 3. NativeInitRuntime::InitializeNativeGame → InitializeOriginalGame | Проверка drawable/identity, typed settings без fake D3D pointers; private output scope; buffer/cache/draw/skin contexts. | Runtime; исключение запускает Deinit. Private output живёт до окончания map/cache cleanup. Входные файлы разрешаются исходным CFile относительно resource CWD/package. | E527 output tests; E557 на Switch успешно прочитал package/config из resource root. Полный map/resource/output путь ещё не выполнен. |
| 4. CGame::Init | seed → static_init → heap → AddPackFile/OpenPackFiles DATA/robots.pkg → scoped CLoadProgress → CFG/DAT → MatrixData/replacements → CacheInit/settings. | Heap/cache/package принадлежат original game owner; локальные CStorage/CLoadProgress разматываются по C++. | Прежний полный WINDOWS HAL Init, CPU storage/cache тесты; E557 native package/config/cache prefix PASS, затем отказ gamma. |
| 5. original_game::EnterPlatform → AttachDeviceImpl | Native window attachment/HID/focus; ReadCaps shader checks; AvailableTextureBytes arena budget; CaptureState presentation/render/surface/gamma lease. | Restore lease и attachment runtime; provider обязан пережить restore. | CPU ownership/failure/restore и SDK; E557 native attachment PASS и отдельный smoke capture/restore PASS. Полный game/reset lifecycle остаётся открытым. |
| 6. CRenderPipeline, Window::Show/Update, g_Config/g_Sampler | Оригинальный renderer selection; applet pump; defaults/params/settings/samplers/max camera distance. | Game heap, platform attachment; pipeline уничтожается при Deinit. | E557 остановился в ReadParams → gamma до ApplySettings. E558: gamma software PASS/hardware pending. E559: полный original ReadParams с настоящим data.txt и gamma-only CPU provider HOST ASan/UBSan/ARM64/Windows PASS; это не GPU Init. |
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

Первый runtime blocker внутри настоящего Switch Init установлен в E557:
`CGame::Init → g_Config.ReadParams → ApplyGammaRamp` бросает
`backend calibrated gamma submission unsupported`. Пользовательский logs2
подтвердил BC1 draw/readback/Present, OpenPackFiles/config.text/CacheInit и
platform attachment. E558 реализует application presentation LUT, owning
snapshot/restore и diagnostic post-gamma readback; HOST ASan/UBSan, ARM64/QEMU,
Windows reference, Mesa GLSL и SDK/NRO PASS. На Switch кандидат ещё не исполнен.
[Baseline](../reports/matrixgame-e2e557-logs2.md) и
[реализация/проверки/артефакт](../reports/matrixgame-e2e558-gamma.md).
E559 исполнил полный original ReadParams после этой границы на CPU/QEMU/Windows,
включая настоящие 25 labels/25 descriptions и повторный Clear. Исправлено
прежнее неточное имя caller: ApplySettings не вызывает gamma.
[Полный config contract и границы](../reports/matrixgame-e2e559-config-tail.md).
Это не PASS полного Init.
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

Исторический Windows FAIL E541 закрыт в E542: исходный DestroyPneumaticData
после освобождения обнуляет m_Pneumaic и g_fcnt. Baseline double-free
воспроизведён, candidate проверен ASan/UBSan, ARM64/QEMU, полными Windows
Init/Draw/recovery и SDK. Это не закрывает ещё не исполненный Switch Init.
[Исправление и доказательства](../reports/matrixgame-e2e542-pneumatic.md).

E2E-5.42: этот failure path исправлен в CMatrixRobot::DestroyPneumaticData:
после исходного HFree static m_Pneumaic и g_fcnt обнуляются. Deinit карты
предшествует release. Поэтому следующий ранний Init failure больше не делает
double-free буфера прежней сессии. Адресные HOST/ARM64 tests, полная двухсеансовая
Windows Init/Draw/recovery/cleanup regression и SDK PASS.
[Подробности и граница diagnostic entry](../reports/matrixgame-e2e542-pneumatic.md).

E2E-5.43: добавлены before/after durable journal scopes настоящих Init методов,
включая config/font/cursor/RobotPreload/stor.Load/BuildTexUnions/PrepareMap/
interface/minimap/ApplyVideoParams. Отдельный diagnostic ELF открывает уникальный
SD run journal до GPU, направляет output в свой run directory и ограничивает
original loop completed Present counters. Native init/frame/HID/SD ещё hardware
pending; standalone GPU smoke/status/readback и NRO packaging остаются до передачи.
[Формат, реальные Windows journals и SDK evidence](../reports/matrixgame-e2e543-journal.md).

E2E-5.44 / передача E2E-5.27: одна отдельная diagnostic NRO упакована;
smoke использует настоящий BC1 upload/draw/readback/Present, status/error —
оригинальный debug font. Последующие original Init и form/loop сохранены;
readback берётся до первого Present, выход — после четырёх completed frames.
Ресурсы диагностики изолированы в `sdmc:/switch/matrixgame-diag/game`,
вывод — в уникальном run directory. Эти пути ещё не исполнялись на Switch.
Первый наблюдённый аппаратный отказ по-прежнему отсутствует; его должен
установить первый ручной запуск. [Инструкция](matrixgame-e2e527-first-run.md).

Первый ручной запуск E2E-5.27 подтвердил Device/Queue/shaders/swapchain/BC1
upload, затем остановился в Lifecycle::Update: ранний smoke не имел attachment.
E546 исправляет порядок только diagnostic entry: свой lease для ранних кадров,
Release перед original Init, original lease во время Init/Draw, новый error-lease
после Deinit при необходимости. HOST/ARM64 ownership и SDK PASS;
вторая NRO подготовлена, но ещё не запускалась на Switch.
[Второй запуск](matrixgame-e2e527-second-run.md).

Второй ручной журнал (E547) подтвердил ранний attachment, но остановился ещё
в smoke acquire: NxAppletBackend ошибочно считал штатный nonblocking
KernelError_TimedOut (59905) аварией. Полный original Init не достигнут.
Обработка пустого опроса исправлена офлайн, сохранены настоящие ошибки и
appletProcessMessage. ASan/UBSan и ARM64/QEMU исполняют точное тело libnx
appletGetMessage с тестовыми IPC endpoints; аппаратного подтверждения
исправления нет. Третья NRO не создаётся.
[Подробности](../reports/matrixgame-e2e547-applet-poll.md).

Ссылки: [readiness plan](matrixgame-readiness-plan.md),
[E2E-5.33 runtime evidence](../reports/matrixgame-e2e533-form.md),
[E2E-5.34 reset evidence](../reports/matrixgame-e2e534-reset-owners.md),
[E2E-5.35 standalone input](../reports/matrixgame-e2e535-standalone-input.md).

E2E-5.61: реализован native backend одноаргументного CTexture::LoadFromBitmap:
настоящий CBitmap DDS, triangle/dither POT resampling, BOX mips и DEFAULT upload
с rollback. HOST ASan/UBSan и ARM64/QEMU: 156 случаев/390 уровней, одинаковые
CPU outputs; против Windows HAL reference 148 exact cases, 12 channel bytes
отличаются на 1. SDK compile/link и отдельная штатная NRO PASS. Одноаргументный
loader пока удаляется linker GC как недостижимый из диагностической Init-цепочки;
это закрытие зависимости, не новый native Init PASS. Последняя аппаратная
граница остаётся ReadParams/ApplyGammaRamp; дальнейшая офлайн-разработка
возможна без устройства. [Состояние и воспроизведение](../reports/matrixgame-e2e561-unmanaged-bitmap.md).

E2E-5.62: исправлен отказ native setter на исходном UI restore
MULTISAMPLEANTIALIAS=TRUE. На single-sample targets enable/mask теперь
сохраняются без изменения rasterization; реальный MSAA type/quality явно
отвергается по фактическому draw attachment. Windows HAL pixels, host ASan/UBSan,
ARM64/QEMU, Win32, original CDeviceState и raw map state replay PASS; тестовая
подмена TRUE→FALSE удалена. Отдельная E562 SDK/NRO PASS, E561 сохранена.
Это не новый actual native Init run.
[Точная семантика и evidence](../reports/matrixgame-e2e562-single-sample.md).

E2E-5.63: реализован настоящий wireframe polygon mode optional selection
`m_IzvratMS`: native draw привязывает SDK Line для WIREFRAME и Fill для SOLID
на обе faces, сохраняя исходные geometry/shaders/stencil. HOST ASan/UBSan,
ARM64/QEMU, original CDeviceState restore, 680 addressed HAL/production GLSL GPU
cases и отдельная SDK/NRO PASS. Полный optional selection path и native Init
на Switch этим не проверены; defaults игры не менялись.
[Реализация и пределы](../reports/matrixgame-e2e563-wireframe.md).

E2E-5.64: исправлен controller typematic: удерживаемый X повторно вызывал
исходный robot-mode toggle. Старый Router воспроизвёл выход из настоящего
выбранного робота через 500 мс на Windows HAL. Candidate выдержал 800 мс в двух
actual original Init/map/selection/recovery сессиях. Typematic оставлен физической
keyboard source с собственным delay; pad held bits и aliases сохранены.
HOST ASan/UBSan, ARM64/QEMU, original Form/Input regression и отдельная SDK/NRO
PASS. Native HID/Init hardware не исполнялись. Также уточнён незакрытый callback
resource/audio provider для standalone entry.
[Проверки и оставшаяся работа](../reports/matrixgame-e2e564-pad-repeat.md).

E2E-5.65: native entry принимает копию настоящего SRobotsSettings; defaults
сохранены, foreign D3D handles/nonfinite/mismatched drawable отклоняются до Init.
Оригинальные config/sampler ApplySettings и AF clamp проверены на host ASan/UBSan,
ARM64/QEMU и Windows; actual Init prefix/cleanup — на host/ARM64. Исправлено
native повышение фильтрации при AF: anisotropy только MIN, исходные MAG/MIP
requests сохранены, Windows branch unchanged. Реальные SDK sampler fields и
отдельная SDK/NRO PASS; нового native Init/map hardware run нет.
[Точная цепочка, артефакт и незакрытые компоненты](../reports/matrixgame-e2e565-native-settings.md).

E2E-5.66: AbortNativeFrame отменяет и primary, и presentation readback до всех
queue-health exits. Старое удержание primary callback воспроизведено реальным
production prefix на host ASan/UBSan и ARM64/QEMU; candidate освобождает оба.
Отдельная SDK/NRO PASS. Это CPU callback ownership evidence, не GPU recovery
или новый Init run. F9 screenshot provider ещё не реализован.
[Регрессия, сборка и следующий шаг](../reports/matrixgame-e2e566-readback-abort.md).

E2E-5.67: native F9 программно интегрирован с настоящим Deko scene readback
последнего завершённого Present и оригинальными CBitmap PNG/CFile/private output.
Slot инвалидируется recording/reset/abort; нет fallback на произвольный image.
Host ASan/UBSan, ARM64/QEMU, Win32 component/reference, независимый PNG oracle,
readback cancellation regression и отдельная SDK/NRO PASS. GPU capture/F9 и
дальнейший native Init на Switch не запускались.
[Семантика кадра, воспроизведение и границы доказательств](../reports/matrixgame-e2e567-screenshot.md).

E2E-5.68: NativeGameOptions принимает полную SMGDRangersInterface с копированием
16 pointers и session-owned scope до конца original Deinit. Неполная таблица
отклоняется до Init/output binding; чужая привязка не заменяется. Host ASan/UBSan,
ARM64/QEMU: original progress dispatch, callback error/unbind, early Init rollback
PASS; отдельная SDK/NRO PASS. Реальные standalone Rangers host services и полный
successful native Init этим не реализованы и не доказаны.
[Подключение существующего bridge и оставшаяся работа](../reports/matrixgame-e2e568-native-services.md).

E2E-5.69: добавлены producer 16 настоящих Robot callbacks без Windows DLL loader
и RangersAudioSession для original sound/cache globals с borrowed real TDataEC.
На host исполнены 9 sound callbacks через настоящий CacheData.dat/GlobalCache/
TCSoundEC и SDL dummy; independent WAV/PCM exact, backend failure rollback PASS.
ASan/UBSan новых TUs PASS (329 original TUs reused без instrumentation).
SDK новые producer/owner compiled в archive; ELF link probe достигает только
старого bridge. Полного SDK audio-host link, native Init или новой NRO нет.
[Настоящая звуковая цепочка и следующие зависимости](../reports/matrixgame-e2e569-rangers-sound.md).

E2E-5.70: real original Rangers AFT text/free-text исполнены с Font.2Normal из
настоящих пакетов. Baseline null clip падает в RobotRenderText; native producer
теперь передаёт полный rectangle при отсутствии clip, сохраняя original renderer.
24 exact original pixel pairs, crop/auto size/error cleanup и ASan/UBSan новых
TUs PASS; 331 original TU reused без instrumentation. SDK archive compile и
existing-bridge ELF link PASS, новый host в ELF не достигается. Full SDK host
link, native Init/map и новая NRO не заявляются.
[Дефект, AFT proof и оставшаяся интеграция](../reports/matrixgame-e2e570-rangers-text.md).

E2E-5.71: original Music.Robot/Vorbis workers исполнены через настоящие config,
Rus CaseConv и SDL dummy в режимах effects+music и music-only. Music owner
освобождает workers до audio/cache; AudioSession допускает music-only без
включения effects. Host и ASan/UBSan новых пяти TUs PASS, 331 dependency object
reused без instrumentation. Original progress/release исполнены только в
штатных standalone no-main-device/empty-textures ветках. SDK archive compile
PASS; полный SDK host link, единый native Init/map и новая NRO ещё открыты.
Аппаратная граница ReadParams/ApplyGammaRamp сохранена.
[Владение, результаты и следующий link gate](../reports/matrixgame-e2e571-rangers-music.md).

E2E-5.72: full SDK host-services link закрыт отдельной целью
matrixgame-rangers-host-link-probe. В AArch64 PIE реально присутствуют producer,
AudioSession/MusicSession, EngineCallbacks и все 16 original Robot functions;
strong undefined = 0. Тот же full16 adapter probe исполнен на host и с ASan/UBSan
новых шести TUs через original sound/AFT/music resources + SDL dummy. Original
Init в этот ELF не входит. Общий config/package owner и native runtime
composition ещё необходимы; отдельная новая deko3d NRO не выпускалась.
[Полный link, execution scope и оставшиеся Init gates](../reports/matrixgame-e2e572-rangers-host-sdk.md).

E2E-5.73: общий RangersDataSession владеет packages/install/mod configs и Main/
Language/CacheData/CaseConv; AudioSession/MusicSession явно заимствуют контекст.
Пять exact original GR_Main comparisons, реальные mod merges, rollback/retry и
borrowed full16 host execution PASS. ASan обнаружил 76-byte original DAT-loader
exception leak; generated port copies получили file/buffer scope, без изменений
формата/проверок. Оба loaders open/read failure regression PASS. Частичный ASan/
UBSan и full SDK shared-data link PASS. Native Init/map и deko3d NRO composition
ещё открыты; аппаратная граница ReadParams/ApplyGammaRamp сохранена.
[Общее владение, дефект cleanup и доказательства E573](../reports/matrixgame-e2e573-rangers-data.md).

E2E-5.74: RangersHostSession владеет data → audio → optional music → full16
EngineCallbacks и восстанавливает flags. Все четыре audio modes и failure/retry
host/partial-ASan PASS. Настоящий ранний native CGame::Init с real text/sound
callbacks прошёл два rollback цикла на явном Unsupported ReadCaps; host переживает
rollback и очищается после runtime. Matrix text-CFG root и Rangers install root
разделены. Owning-host full SDK link PASS; joint native/deko+Rangers SDK entry и
обновлённая NRO — следующий шаг. Полный Init/GPU/hardware не доказан.
[Общий host owner, muted mode и native rollback E574](../reports/matrixgame-e2e574-rangers-host-session.md).

E2E-5.75: стандартный diagnostic entry подключает owning Rangers host к native
options и сохраняет его до конца original Deinit. Joint native/deko+Rangers
AArch64 link и отдельная NRO PASS, undefined=0. NRO/ASET/NACP/fonts/provenance
проверены, подготовлен read-only ресурсный план. Combined NRO NOT RUN: полный
Init/map не доказан; hardware boundary ReadParams/ApplyGammaRamp сохраняется.
[Артефакты и границы E575](../reports/matrixgame-e2e575-hosted-diagnostic.md),
[ручной запуск и roots](matrixgame-e2e575-hosted-diagnostic.md).

E2E-5.76: новая hosted цепочка впервые исполнена на Linux ARM64/QEMU: все четыре
audio modes/full16 callbacks, failure/retry/teardown PASS. AFT 24 pairs/crop/auto
pixels совпали с x86 E570 побайтно. Два real hosted native Init rollback cycles
на явном Unsupported ReadCaps PASS с ARM Matrix objects. Это CPU/ABI proof,
не GPU/полный Init/запуск NRO; E575 NRO сохранена. Windows reference новой цепочки
ещё открыт, hardware boundary не изменился.
[Cross-prefix, реальное исполнение и пределы E576](../reports/matrixgame-e2e576-rangers-arm64.md).

E2E-5.77: Windows MSVC x64 впервые исполнил real owning host chain: 339 objects,
full16 callbacks во всех четырёх sound/music modes, backend failure/retry и
teardown PASS. AFT 24 pairs/crop/auto дали exact Linux/ARM64 pixels. Private
Windows runtime исправляет доказанный 24→40-byte native critical-section
overwrite; SDK layout/recursive/contended execution PASS. Windows locale и
timing используют реальные host APIs. Двенадцать ограниченных host-ASan/ARM64/
SDK delta compile checks и host sanitized clock execution PASS. Windows native
CGame::Init prefix ещё NOT RUN; полный Init/map/hardware не продвинуты.
E575 NRO и protected inputs сохранены.
[Windows reference, дефекты ABI и точные gates E577](../reports/matrixgame-e2e577-rangers-windows.md).

E2E-5.78: Windows hosted native CGame::Init prefix теперь реально исполнен.
Пять current production MatrixGame archives: 159 actual MSVC x64 objects,
assertions включены. Joint link с 337 reused Rangers objects и original prefix
probe прошёл два caps-refusal rollback/reentry cycles: real AFT/sound calls,
copied table, host survives runtime cleanup, final host cleanup PASS. CPU stop
до успешной device negotiation; полный Init/map/GPU/Windows HAL не запускались.
Аппаратная ReadParams/ApplyGammaRamp boundary и E575 NRO сохранены.
[Фактическая композиция и доказательства E578](../reports/matrixgame-e2e578-hosted-windows-native.md).

E2E-5.79: full original Windows HAL Init/map впервые прошёл с owning Rangers
host: два Init/transition/four-visible-frames/Deinit cycles PASS, восемь 800×600
readbacks. Исправлены несовместимый CPU cache object вместо actual original
texture и два 4-byte pointer queue shifts (terrain/static sorting). Factory
Windows/host-ASan/ARM64 и early rollback PASS. Новая отдельная E579 SDK NRO
compile/link/package PASS, NOT RUN. Точный следующий native-компонент — real
transition framebuffer capture: non-Windows GDI boundary пока Unsupported.
Switch boundary не изменён PC-проверкой, полный native Init/map ещё открыт.
[Production fixes, реальные кадры и оставшийся capture provider E579](../reports/matrixgame-e2e579-hosted-windows-map.md).

E2E-5.80: native loading capture подключён к existing real DekoDevice screenshot
provider до original video Reset. Owned CPU pixels однократно передаются original
CTransition; frame proof/extent failures остаются явными. Windows HAL reset/readback,
два full hosted Init/map/frames/Deinit, CPU ASan/ARM64, native capture failure/retry
и 108 PNG oracle PASS. Отдельная E580 SDK NRO compile/link/package PASS, NOT RUN.
Полный native/deko Init/map и прежняя hardware boundary не закрыты PC evidence.
[Owned snapshot, проверки и оставшиеся gates E580](../reports/matrixgame-e2e580-loading-capture.md).

E2E-5.81: original form/loop впервые проверен совместно с настоящим owning
Rangers host после закрытия стартового диалога. Два Windows HAL цикла: по 208
loop Draw/5125 ms симуляции, Enter/Pause/menu/camera, focus release/recovery,
четыре changing frames и Deinit/host cleanup PASS. Production source не менялся;
E580 NRO сохранена, большие ASan/ARM64/SDK suites не повторялись. Switch runtime
и полноценный бой/возврат результата остаются открытыми.
[Совместный interactive loop и точные границы E581](../reports/matrixgame-e2e581-hosted-windows-loop.md).

E2E-5.82: original CInfo ignore FIFO overflow воспроизвёл MSVC ASan
memcpy-param-overlap. Generated port copy использует memmove для pointer/time
массивов и текущую метку новой цели. Два real-map oracle и Windows HAL cleanup
cycles PASS; host-ASan/ARM64 delta compile-only PASS. Новая отдельная SDK NRO,
без изменения E580/основной. Full native/deko runtime остаётся NOT RUN.
[ASan baseline, original AI fix и границы E582](../reports/matrixgame-e2e582-ai-ignore.md).

E2E-5.83: два original robot-order дефекта воспроизведены отдельно: memcpy
overlap при удалении и лишнее чтение при ротации из-за sizeof*count-1.
Generated MatrixRobot.cpp исправлен; два real-world MSVC partial-ASan oracle
(15 removal/60 rotation cases + genuine core refs) и HAL cleanup PASS.
Host-ASan/ARM64 здесь compile-only; отдельная SDK NRO, Switch NOT RUN.
[Два baseline и точная область исправления E583](../reports/matrixgame-e2e583-robot-orders.md).

E2E-5.84: original player command PlaceFindNear/RobotToLogicGroup/PGOrderMoveTo
реально исполнился внутри hosted form/L3GRun. Два Windows HAL цикла: за 5000 мс
робот переместился на 94.624159 world units, grid distance² к requested point
сократился 100→5, queue стала пустой. Teleport/state injection не применялись.
Production source/NRO E583 сохранены; большие suites не повторялись.
Native/deko runtime, HID и полноценный бой остаются открытыми.
[Наблюдения, кадры и ограничения E584](../reports/matrixgame-e2e584-hosted-movement.md).

E2E-5.85: original selection → arcade → EngineSink/KA_FIRE → weapon Fire/Hit
callbacks → key release → recovery/cleanup реально исполнены дважды на Windows
HAL с owning host. На сессию 4 Fire, 11 callbacks (7 object events), 208 loop
Draw/5125 ms. Test-only read observers не меняют original weapon body; неверные
fixture prerequisites диагностированы и сохранены. E583 production/NRO
сохранены. Host/ARM/SDK suites не повторялись; full native/deko/battle NOT RUN.
[Проверка ручной стрельбы и точные ограничения E585](../reports/matrixgame-e2e585-hosted-firing.md).

E2E-5.86: отдельная matrixgame-native-interactive-nro оставляет B/Plus игре и
даёт до 120 секунд после Init; выход Minus+Plus. Quick цель сохранена. Frame gate
и original-first readback теперь исключают loading transition: нужны готовые
video resources и видимые map presents. Policy реально выполнена Windows/
ASan/ARM64; обе SDK NRO zero-U/package/font PASS, NOT RUN. Original generated
game sources совпали с E583; прежние/основная NRO сохранены.
[Manual target, новые frame gates и границы E586](../reports/matrixgame-e2e586-interactive-diagnostic.md).

E2E-5.87: два original hosted Windows HAL цикла прослежены до настоящего
virtual Damage и измеримого поджога map objects. На сессию 7 Damage, два
перехода ablaze false→true/TTL 0→10000; HP decrease не доказан. Наблюдатели
только в test copies, исходные тела проверены на точное восстановление;
core guard/cleanup, восемь PNG и защищённые fingerprints PASS. Production
и обе E586 NRO сохранены. Full native/deko runtime и полный бой открыты.
[Состояние Damage и область доказательства E587](../reports/matrixgame-e2e587-hosted-damage.md).

E2E-5.88: штатная PGOrderAttack и original AI/weapon/virtual Damage доказали
hostile robot HP decrease в двух Windows HAL сессиях. По два события:
210→185 и 185→160 getter units. Ускоренная симуляция добавила 1324 настоящих
form Takt(25), время учитывается отдельно от 208 loop Draw/5125 ms. Full
Init/recovery/readback/Deinit/host cleanup PASS. Production/E586 NRO сохранены;
ASan/ARM/SDK не повторялись. Полный бой/возврат и native/deko hardware открыты.
[Урон HP, ускоренные ticks и границы E588](../reports/matrixgame-e2e588-hosted-battle.md).

E2E-5.89: native entry подключил настоящий CGame::SaveResult до Deinit; все
шесть signed statistics сохранены в journal. Interactive исправлен для legal
surrender state4 (exact E586 baseline ранее отклонял его). Два Windows HAL
пути Esc/S/Enter/statistics/Enter прошли без подстановки исхода; stats сверены
с side, snapshot пережил cleanup при живом owning host. CPU policy Windows/
ASan/ARM64 PASS, обе новые отдельные SDK NRO zero-U/package PASS, NOT RUN.
Полный бой win/loss и owning galaxy transition пока не доказаны.
[SaveResult, исправление surrender4 и точная граница E589](../reports/matrixgame-e2e589-original-result.md).

E2E-5.90: полный original бой дважды дошёл до естественного Loose/state2:
1707100 ms, 11 robot kills/5 turret kills, исходные dialog/statistics/exit и
SaveResult→Robot global six statistics, focus/readback/cleanup PASS. Выявлены
и исправлены FlameData 32-bit callback cookie и legacy total-length RIFF
в исходном Sound.Flame. WAV runtime Windows/ASan/ARM64 PASS, Flame TU host/
ARM compile-only, обе новые SDK NRO с обновлённым реальным audio member NOT RUN.
Реальный context probe подтвердил отсутствие Galaxy/Player/Gov screen/Run
dispatch у standalone host; galactic transition и победа не объявлены PASS.
[Полный бой, два дефекта и точная граница Galaxy E590](../reports/matrixgame-e2e590-completed-battle.md).

E2E-5.91: NativeRangersRunBinding реализовал настоящий TRobotRun slot: UTF-16/
settings boundary → original Init/form/L3GRun → SaveResult → проверенный cleanup
→ caller output/code. Три полных Windows HAL поражения под partial MSVC ASan,
включая nonnull readonly caller settings, failed Init/retry и owned context PASS.
Host/ARM TU compile-only, стандартная SDK library/API ELF zero-U link PASS.
E590 NRO сохранены; complete Robot interface и настоящий FRun/Gov/Galaxy caller
ещё не подключены и не объявлены PASS.
[Run slot, проверки и оставшаяся caller integration E591](../reports/matrixgame-e2e591-native-rangers-run.md).

E2E-5.92: complete NativeRangersInterface публикует реальные Initialize/Support/
Run/Finalize. Windows HAL partial ASan: полный бой до natural loss state2,
nonnull settings, six statistics, исходный FinalizeRobotRuntime и повторный
Install/Finalize PASS. CPU Support policy Windows/ASan/ARM64 runtime PASS;
полные interface/support/host TUs host/ARM compile-only PASS. Стандартная SDK
library и retained API AArch64 ELF zero-U link PASS, новая NRO не создавалась,
E590 сохранены. Galaxy/Player/Gov session, исходные FRun/OnOpen transition,
естественная победа и native/deko hardware остаются открытыми.
[Полный интерфейс, владение и точные gates E592](../reports/matrixgame-e2e592-native-rangers-interface.md).

E2E-5.93: original Robot::FRun исполнил полный Windows HAL бой до natural loss
state2 и вернул все six statistics. Настоящие base GI panels/cursor/rectangles,
SDL foreign wrapper того же SDK окна и scoped token→HWND binding обеспечили
реальный caller restore. Исправлена Win32 record→Win64 SDK Reset ABI boundary;
actual Reset/default-resource refusal/retry/unaligned canaries под ASan PASS.
Full FRun partial ASan runtime exit0: gamma owning caller, SDK target/viewport,
volumes/GI invalidation/cwd cleanup/leases/Finalize PASS. SDK configure-only
проверил exclusion Windows TU; больших passed builds не повторяли, NRO прежние.
TfGov/Galaxy/Player session и rewards/history transition ещё не выполнялись;
естественная победа и native/deko FRun/hardware открыты.
[Исходный FRun и оставшаяся граница Galaxy E593](../reports/matrixgame-e2e593-rangers-caller-reset.md).

E2E-5.94: RangersGovernmentSession подключил original TfGov constructor,
Main.dat ML/Data/ZPos roots, InitializeFromConfig и InitializeLayout. По 395
настоящих controls в двух циклах Windows partial ASan/host ASan+UBSan/ARM64
QEMU; missing prerequisites, partial-tree rollback, foreign/duplicate ownership
и cold/borrowed-root cleanup PASS. Новая стандартная SDK library и retained
API AArch64 ELF zero-U link PASS, runtime NOT RUN; прежние engine objects и
NRO сохранены. OnOpen/Draw/Galaxy/Player/reward/history ещё не исполнялись.
Внешний logs2.7z сейчас отсутствует: отдельный archive gate NOT VERIFIED.
[Правительственный GI owner, проверки и оставшиеся зависимости E594](../reports/matrixgame-e2e594-government-session.md).

E2E-5.95: RangersScriptSession подключил original Script host/VM/builtins,
path-node pool и оба настоящих worker. Найдена и исправлена в port owner
незавершённая очистка request worker и 12 builtin-контейнеров; оригинальный
FinalizeScriptEngine их не освобождал. По два цикла Windows partial ASan,
host ASan/UBSan и ARM64/QEMU теперь завершаются exit0: 912 builtins, реальный
script result42, path allocate/recycle, два worker и 14 событий cleanup PASS.
Standard SDK library/retained API AArch64 ELF zero-U PASS; новая NRO не
создавалась. Galaxy/Player и gameplay worker jobs ещё не выполнялись.
Следующие обязательные зависимости: настоящие Hangar/TCCInterface owners,
исходная генерация кампании, Gov OnOpen/result/reward/history/return.
[Script host, устранённое зависание и точные границы E595](../reports/matrixgame-e2e595-script-session.md).

E2E-5.96: RangersGalaxyPrerequisites владеет настоящими Hangar(Main.dat config/
layout, 341 control) и TCCInterface(lock/Buffer/snapshots). В двух циклах
Windows partial ASan/host ASan+UBSan/ARM64 QEMU выполнены неизменённые
TGalaxy_Create/Destroy, включая integrity operations и CC reset; foreign/
duplicate/live-world guards, partial-layout rollback и borrowed roots PASS.
Standard SDK library/retained API AArch64 ELF zero-U PASS. Кампания/Player не
генерировались; следующие зависимости: original gameplay definitions,
SE ship/Script templates из InitializeGlobalUiRuntime, затем полный original
TThreadCreateNewGame_Execute. Gov OnOpen/победа/возврат и native FRun открыты.
[Galaxy prerequisites, проверки и оставшаяся цепочка E596](../reports/matrixgame-e2e596-galaxy-prerequisites.md).

E2E-5.97: RangersGlobalUiSession исполняет полный original
InitializeGlobalUiRuntime: 36 экранов/10826 controls, 40 ScriptTemplates,
32 global variables, 207 planet templates и исходные gameplay/weapon/map/
greeting definitions. Два цикла и original parser-exception rollback PASS
на Windows partial ASan, host ASan/UBSan и ARM64/QEMU. Оригинальный Finalize
дополнен освобождением пропущенных AchievementsScreen/SpaceObjectUiLoop,
потом вызывается original ResetScriptHostRuntimeState. SDK standard library/
retained API AArch64 ELF zero-U PASS. ARM64 attempt 1 timeout180 сохранён;
attempt 2 — неизменённый binary с лимитом480, оба цикла exit0. Перед полной
генерацией обнаружена LP64 allocation граница: fGameSettings выделяет 12 bytes
для TStorageEntry, devkitA64 sizeof=20. Нужен port overlay sizeof fix, затем
полный original campaign generator. Player/Gov result/победа ещё NOT RUN.
[Полная UI/gameplay инициализация и точные gates E597](../reports/matrixgame-e2e597-global-ui.md).

E2E-5.98: original campaign generator выполнен до stage8/turn301 с настоящими
Player/Star/HomePlanet и полным warmup300. Windows partial ASan и ARM64/QEMU
runtime exit0; host также дошёл до stage8, но общий ASan/UBSan gate FAIL:
LeakSanitizer 226478 bytes/4272 allocations. Подключены original TQuestMessages,
localized PlanetQuest cache AddMissingFromBlock и native sizeof StorageEntry
в port overlay. SDK original fGameSettings object + standard campaign library/
retained API AArch64 ELF zero-U PASS; остальные377 original members сохранены.
Следующая открытая граница — ownership leaks оригинальной кампании, затем
Gov OnOpen/result/reward/history/return, победа и native/deko FRun. Новая NRO
не создавалась; успешная генерация не закрывает leak или battle-return gate.
[Настоящая кампания и явный FAIL LeakSanitizer E598](../reports/matrixgame-e2e598-campaign.md).

E2E-5.99: подтверждённые cleanup leaks исправлены в port overlay, включая
VM array lifetime owner с сохранением borrowed aliases. Полная кампания
ASan/UBSan/LeakSanitizer PASS без suppressions; отдельный actual ctor/dtor/
alias/thread probe PASS. Windows partial ASan и ARM64/QEMU stage8/turn301
exit0; SDK семь affected TUs в отдельном архиве, standard campaign target и
retained API AArch64 ELF zero-U PASS. Проверены protected inputs/upstream pins/
старые NRO. Новая NRO не создавалась; battle-return остаётся открытым.
[Cleanup, отрицательные попытки и аудит E599](../reports/matrixgame-e2e599-campaign-cleanup.md).

E2E-6.00: Windows partial ASan — настоящие OrderTakeoff/OrderLanding и исходные
расчёты ходов доставили Player со станции на HomePlanet без присваивания
местоположения. После отдельного настоящего InitializeLayout правительства
OnOpen/OnClose PASS с реальным диалогом. Первая попытка без layout сохранила
ASan nil Screen failure. Memory snapshot выявил отсутствующие original
AchievementDefinitions; интеграция local achievement owner и корректного
campaign lifetime при замене мира начата, gate ещё не закрыт.
[Навигация и вход в Gov E600](../reports/matrixgame-e2e600-campaign-navigation.md).

Продолжение E600: Windows partial ASan подтвердил настоящий memory-only snapshot
round-trip из Gov и cleanup с восстановленными Player/Planet/money/storage/turn
и неизменными persistent save/load counters. Production campaign owner учитывает
замену Galaxy исходным restore; scoped original local achievement service даёт
82 настоящих определения. SDK campaign/achievement + два loader cleanup TUs в
новом архиве compile/link zero-U PASS. Host functional snapshot PASS, но общий
gate ещё FAIL: LSan30200/1214; адресные fixes перезаписи constructor resources
прошли Windows, повторный host остановлен другим warmup UBSan seed overflow.
Revolution-seed candidate и ARM64/QEMU runtime выполняются отдельно. Полный
Gov/FRun/result/reward/history/return и победа остаются NOT RUN; NRO не менялись.

E2E-6.01: host ASan/UBSan/LSan original snapshot/cleanup exit0; revolution news
seed сохраняет native low32 без signed overflow, oracle77 PASS host/devkitA64.
Production generator routing для пяти ранее пропущенных CPP и invalidation
source cache исправлены; все8 полных generated CPP и13 affected methods
byte-identical compiled SDK inputs. SDK updated original archive (aPlanet only)
и standard campaign/achievement retained API ELF zero-U PASS. ARM64 original
snapshot exit0 на предшествующем коде; последняя seed delta — compile-only.
[Точная компонентная цепочка и версия ARM64 E601](../reports/matrixgame-e2e601-revolution-seed.md).

E2E-6.02: Windows reference partial ASan впервые выполнил настоящую цепочку
Gov OnOpen → memory snapshot → Robot::FRun → full CGame Init/Takt/Draw/combat →
natural defeat → restored Galaxy → actual reward/history/result dialog/
requested Gov return → cleanup. Difficulty1 reward+170; difficulty3 reward+30;
actual stats совпали с original SaveResult и PlanetBattleHistory. Framebuffer
context берётся у настоящего renderer_platform owner; UI auto-save handler
не запускался, физических Gov UI frames этим host provider не доказано.
Естественная победа ещё открыта; выполняется обычный Winter map id12 сценарий.
Native/deko FRun, новая отдельная NRO и user hardware acceptance остаются
открытыми. [Бой, возврат и точные границы E602](../reports/matrixgame-e2e602-campaign-frun.md).

E602 strategy follow-up: исправлена economy fixture — существующий
GetGroupLogic сохраняется, RobotToLogicGroup больше не сбрасывает маршрут
при каждом опросе. Штатный heavy design (SuperDjeans + оплаченный RemoteBuild)
проверен, но победы не дал. Stock с исправленным управлением дошёл до
120000 тактов без исхода; исключение лимита завершилось 0xc0000409.
Восстановление snapshot при C++ исключении FRun/Gov требует отдельного
исправления/проверки; этот запуск не является PASS возврата или боя.

E2E-6.03: новый Windows fault-injection после полного original Init и восьми
Draw завершился exit0. FRun выполняет original caller restore перед rethrow;
Gov восстанавливает настоящий memory snapshot и очищает pending request.
Исходный тип исключения сохранён; money/history/battle count не изменились;
gamma readback и полная cleanup PASS. Два production-generated SDK units
скомпилированы и linked retained campaign API ELF zero-U PASS. Инвариант
campaign owner сохранён; старый failed Winter attempt не переобозначался.
[Проверка ошибки и точные границы E603](../reports/matrixgame-e2e603-campaign-exception.md).

E604: normal-return regression после exception overlays E603 закрыт на
Windows natural defeat2 (Winter heavy squad): actual stats/reward+90/history/
snapshot/Gov return/gamma/cleanup exit0 PASS. Fresh standard diagnostic NRO
цели interactive и quick собраны отдельно, ELF/NRO/embedded assets проверены;
HARDWARE_PENDING. Standalone entry не вызывает Galaxy/Gov/native FRun.
[Сборка и оставшиеся границы E604](../reports/matrixgame-e2e604-current-diagnostic.md).

E605: реализован scoped RGB565 presenter реального DekoDevice для галактического
canvas; native NativeRangersInterface владеет им на Switch. Managed texture,
row upload, native scene/draw/present и state lease restore реализованы; SDK
compile/retained link zero-U PASS. CPU full RGB565/padding/bounds/aspect tests
PASS host ASan/UBSan/LSan, Windows ASan и ARM64/QEMU UBSan. GPU runtime NOT RUN.
Full native FRun ещё требует caller reset/gamma/window adaptation и общего
Galactic caller context; E604 NRO/ZIP сохранены, новый NRO здесь не создавался.
[Подробности и оставшаяся native цепочка E605](../reports/matrixgame-e2e605-rangers-canvas.md).

E606: FRun получил scoped caller presentation lease: real capture → keep UI
window for progress → release immediately before Run → original Init/Deinit →
reacquire caller window + device/presentation/gamma/cursor restore. Native
Robot interface владеет phase и отвергает Run без handoff. Windows reference
настоящего HWND/D3D9 проверил exception path и natural defeat/result/Gov return,
exit0; actual target/viewport/gamma/cleanup PASS. Production-generated Robot
и native owner SDK retained link zero-U PASS. Полный native Gov context и
GPU runtime ещё открыты; E604 NRO/ZIP сохранены.
[Механизм передачи и финальные gates E606](../reports/matrixgame-e2e606-caller-presentation.md).
<!-- E607 evidence extension; preceding milestones retain their version scope. -->

E607: добавлен `RangersUiContext` для оригинальных read-only CFG/TCC,
ML/Data/ZPos, RGB565 canvas, palette/cursors/depth. Время жизни:
Host → UiContext → Script → GlobalUi → Campaign, освобождение обратно.
Host ASan/UBSan/LSan, ARM64/QEMU и Windows partial ASan подтвердили два цикла
UI/script и cleanup; host/ARM также реальный поздний отказ malloc/realloc.
devkitA64 retained UI/caller/campaign ELF linked, zero strong undefined.
Это ещё не runtime полного native Gov/FRun и не полный Windows startup.
[Проверки и границы E607](../reports/matrixgame-e2e607-ui-context.md).

E608: production UiContext E607 включён в настоящий Windows campaign/Gov/FRun.
Полные Init/Takt/Draw → natural defeat (808000 ms, 32155 ticks) → window/gamma
restore → memory snapshot restore → original history/statistics/+90/dialog/
requested Gov return → полная cleanup PASS, partial MSVC ASan. Приватный
rangers-user без файлов; защищённые входы/NRO/upstream сохранены. Native UI
input/main Galaxy loop/deko execution и новая integrated NRO остаются открыты.
[Совместная runtime проверка и границы E608](../reports/matrixgame-e2e608-ui-campaign.md).

E609: добавлены scoped native UI target и RangersInputQueue; EngineSink может
передавать события оригинальному GI вне MatrixGame. Host ASan/UBSan/LSan и
ARM64/QEMU UBSan проверили aspect-fit координаты/кнопки/очередь/владение.
Windows original MainMenu ProcessWindowMessage и fallback Input PASS; SDK
текущий EngineSink/queue retained link zero-U PASS. Постоянный native poll/
dispatch owner и единый async key/cursor state ещё не подключены; это
компонентная проверка, не готовый native UI loop.
[Область доказательств и следующие зависимости E609](../reports/matrixgame-e2e609-rangers-input.md).

E610: реализован RangersNativeInput и real SwitchWindow backend factory.
Production GR_WinMessage и async-key/cursor/warp imports используют общий
scoped source; FRun lease делает suspend/resume и input cancellation.
Held-until-release/warp/restore-position и UI owner reset добавлены. UiContext
привязывает оригинальный user-directory cache к отдельному user_root/config,
устраняя найденную production зависимость от Desktop Shell. Host sanitizers,
Windows partial ASan, ARM64/QEMU owner и SDK retained link PASS. Граница
runtime-ввода в этих probes сценарная; physical HID/GPU не проверены.
Следующая цепочка: общая native entry с Script/GlobalUi/Campaign/Gov и новая NRO.
[Точные gates и границы E610](../reports/matrixgame-e2e610-native-ui-pump.md).
