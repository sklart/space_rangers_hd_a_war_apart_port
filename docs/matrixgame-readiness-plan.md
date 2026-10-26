# MatrixGame: план готовности порта на Switch

Срез 25.10.2026. Цель — оригинальные планетарные бои внутри основной игры на
Switch: загрузка карты, настоящий кадр, управление, симуляция, результат боя и
возврат хозяину. Подстановка заглушек вместо исходного поведения не закрывает этап.
Исходная Windows-версия уже существует; выпуск новой Windows-версии не входит в
план. Windows используется для ограниченных контрольных проверок изменённых
исходников на настоящем D3D9.

**E2E-5.21 debug font:** исходный MS Sans Serif 25 (222 glyphs) подключён к native create/upload/draw/lost/reset/rebind. HOST ASan/UBSan, WINDOWS HAL (699 cases + alpha/reset), ARM64/QEMU и SDK link PASS; SWITCH HARDWARE PENDING. Этот guard BuildTexUnions → LoadFromBitmap(DXT1,6) закрыт в E2E-5.22; см. обновление ниже. [Отчёт](../reports/matrixgame-e2e521-font.md), [полная карта Init](../reports/matrixgame-e2e521-init-sequence.md).

**E2E-5.22:** исходный BuildTexUnions выделен в общий ресурсный модуль и выполнен на host/ARM64 из настоящего robots.pkg. Шесть bitmap и все BC1 mips совпадают с Windows; DXT1/шесть уровней подключены к native texture backend. Ошибка PNG/неполный bitmap теперь останавливают сборку с полным cleanup вместо публикации неверного union. Добавлены native viewport/status queries и исходный Clear/Scene/Present route с одним Present и rollback. Полный native Init/карта/кадр на Switch ещё не исполнялись. На срезе E2E-5.22 следующим был CGame::ApplyVideoParams → Device::ReadCreationParameters; этот presentation/window/reset рубеж программно закрыт продолжением E2E-5.23 ниже. [Единый отчёт E2E-5.22](../reports/matrixgame-e2e522-integration.md).

**E2E-5.23 presentation:** ReadCreationParameters, реальные VI display/VBlank queries, borderless window geometry, swapchain/color/depth recreation и presentation restore lease реализованы. Reset отказывает при живых default resources/открытом кадре; rollback/retry проверены production coordinator на CPU, SDK ELF слинкован. Original Init transition получает acquire из BeginScene; аварийный кадр отменяется до Deinit карты. Полный native Init/карта/кадр на Switch всё ещё NOT RUN. На завершении E2E-5.23 guard был Top/mm → SStateImages::SetStateText → Text::Render → text_backend::DeviceIdentity (text SDK device unavailable), ресурс Font.2Ranger; продолжение E2E-5.24 ниже снимает эту границу. [Отчёт](../reports/matrixgame-e2e523-presentation.md).

## Что уже доказано

- Исходники закреплены на upstream `218115a72fa9c5d43269f4ca84a8596a0bcb82f1`.
  Адаптации создаются генераторами, исходный подмодуль не переписывается.
- CPU-пути оригинальных ресурсов, навигации, VO, анимации и ряда подсистем
  проверены адресными host/ARM64 тестами; границы каждого теста указаны в отчётах.
- Исправленный Path выдержал ASan/UBSan, ARM64/QEMU и Windows runtime;
  devkitA64 compile/link успешен, на Switch тест не исполнялся.
- Полная Windows-цепочка линкуется с оригинальными Path, `L3GRun`, `CalcVis` и
  `CalcMapGroupVisibility`. Сам цикл и эти расчёты видимости ещё не исполнялись.
- Настоящий D3D9 HAL создан. Полный исходный `CGame::Init` загрузил storage и
  завершил `PrepareMap`: карта 276×285, группы 28×29, девять VO с материалами
  оригинального менеджера. Два отказа после этого корректно освободили ресурсы.
  Это загрузка CPU-мира на Windows, без успешного возврата Init и первого кадра.

**Обновление E2E-5.20:** последний пункт преодолён. В двух циклах на Windows
успешно вернулись оригинальные Init и полный form Draw; выполнены настоящий
resize/Reset, readback кадра 800×600 и Deinit с восстановлением исходного режима.
Подтверждены объекты/terrain/water/shadows/effects, 1008 tracked SDK draws на
кадр. [Отчёт и снимки](../reports/matrixgame-e2e520-first-frame.md).
Это контрольный Windows-рубеж; Switch runtime и полный бой остаются открытыми.

**E2E-5.21 cascade:** прежние 209 alpha/null случаев закрыты в проверенной матрице;
1476 D3D9 и 1476 same-GLSL GPU сравнений совпали. FVF lighting/TCI и native
map/VO submission подключены и слинкованы с оригинальным Init/frame.
`ReadCaps` реализован с проверкой shader pair. Residency-продолжение добавило
настоящую texture arena, учёт свободных диапазонов и original mip upload/bindings;
`AvailableTextureBytes` подключён. CaptureState/restore реализованы и проверены
на host/ARM64/Windows CPU; SDK original closure слинкован. Native applet/window
lifecycle реализован: Show/Update, focus/exit, restore visibility и ожидание
foreground перед acquire. CPU проверки с original CForm прошли на host/ARM64/
Windows; обновлённый полный SDK ELF слинкован. Native cursor/input provider
и ExchangeCursor реализованы, icon upload/overlay и HID route подключены;
CPU и SDK проверки прошли. Узкая Windows Init/frame регрессия сохранила снимки.
Уточнён declaration order: m_DI объявлен раньше m_Cursor. Бывший guard
`CMatrixDebugInfo → debug_font::CreateFor` закрыт последующим font-этапом.
Native allocation/upload/state/Init/map/frame на Switch не выполнены.
[Подробные границы](../reports/matrixgame-e2e521-cursor.md).

Подробные результаты: [Path/линковка](../reports/matrixgame-e2e519-path-full-windows.md),
[загрузка карты и текущая граница](../reports/matrixgame-e2e519-map-loaded.md).

**E2E-5.24 UI text:** прежний guard `Text::Render → DeviceIdentity` снят для
исходного Top/mm. Оригинальный Rangers.ttf проверен побайтно через GDI;
Rangers10 и Verdana12/13/14 дали 17 940 точных сравнений с D3DX HAL.
Исходный Text::Render исполняется до bitmap на host/ARM64. Upload в CPU
проверках — fault model; deko upload только SDK link, HARDWARE_PENDING.
Следующая исходная ветка при projected shadows=1 — StaticPrepare2 →
RNeed(MR_ShadowProjTex) → CVectorObject::CalcShadowTextureWOMat →
offscreen::GetTarget, guard `offscreen target query unavailable`. Далее нужны
shadow targets/readback, minimap render texture и StoredStateBackend.
Font linking вне покрытия исходных лиц остаётся unsupported.
[Результаты и границы](../reports/matrixgame-e2e524-text.md).

Продолжение UI text: устранено чтение выходного `*out` в native font/surface/
renderer factories; добавлены проверки ненулевых выходных слотов и отказов.
Результаты хранятся отдельно; Windows font goldens используются повторно,
общий native Init и первая карта остаются неподтверждёнными.
[Контракт и следующие зависимости](../reports/matrixgame-text-contract-followup.md).

**E2E-5.25 offscreen:** GetTarget/depth, standalone color/depth allocation,
bindings, lock/readback/writeback и target lease подключены к настоящему
deko3d. Ownership/scope/OOM прошли host ASan/UBSan и ARM64/QEMU; original
MakeSolidTexture и shadow cleanup получили проверенный RAII. Два полных
Windows Init/frame/Deinit сохранили изображения. На Switch исполнение отсутствует.
Следующая исходная ветка — multi-frame/list shadow → CreateRenderTexture(A8,
usage=1, default pool), guard `shadow render texture SDK unavailable`.
Windows HAL подтвердил настоящий A8, поэтому RGBA подмена не подходит.
[Отчёт и границы](../reports/matrixgame-e2e525-surfaces.md).

## Подсистемы и приёмка

`PARTIAL` означает доказанную часть; компиляция не означает исполнение.
`HARDWARE_PENDING` требует нового запуска на физическом Switch.

| Подсистема | Подтверждено сейчас | Что закрывает подсистему для Switch |
| --- | --- | --- |
| Источники и ABI | Pin, typed bridge и пересылка 16 callbacks в fixture | Настоящие вызовы основной игры, корректные аргументы/счётчики; совместимость поведения с установленной игрой |
| Пакеты, файлы, CFG, heap/cache | PARTIAL: реальные данные и cleanup в CPU тестах и Windows Init | Полная карта из пользовательских ресурсов на Switch, ошибки/повторный вход без утечек и изменения входов |
| Указатели и владение | PARTIAL: исправлены конкретные UAF/double-free; Path перенос/отказ allocation проверены | Все исполняемые полные сценарии ARM64, отказ памяти и teardown; отсутствие новых sanitizer дефектов |
| VO/анимация/геометрия | PARTIAL: оригинальные CPU методы, frame ranges, staging, владельцы | Реальные модели в полном Switch-кадре, анимации, повторная загрузка, освобождение GPU памяти |
| Материалы/skin manager | PARTIAL: оригинальные callback tables и lifetime в Windows PrepareMap | Нативные shader bindings для side/gloss/mask/back и всех используемых multipass путей; визуальная сверка |
| Нативные Window/Device | OPEN для полного CGame на Switch; Windows HAL контроль есть | Typed native entry, реальные caps/display/time/window semantics, без фиктивных D3D identities |
| Жизненный цикл устройства | Windows: два Init/frame/Deinit; native owned presentation reset/rollback/lease программно выполнены, HARDWARE_PENDING | Switch Init/Deinit/recreate и отказ каждого шага с освобождением default resources; безопасный возврат хозяину |
| GPU allocation и базовый draw | HARDWARE FAIL прежнего smoke до draw; SDK-кандидат исправления есть | Физический Switch: allocation/upload/indexed draw/readback/recreate без assert; новые логи и хэши |
| Текстуры и GPU buffers | PARTIAL: original union assembly + BC1/шесть mips, typed lifetime/OOM, deko upload compile/link; HOST/ARM64/HAL proof | Реальные форматы/mips/samplers, deko3d alignment/cache/barriers, загрузка и освобождение на Switch |
| Terrain/water/карта | Windows: реальный PrepareMap и кадр с terrain/water/objects; PARTIAL для Switch | Native Switch PrepareMap, terrain/water submissions и первый корректный кадр карты |
| Visibility/culling | Полные исходные CalcVis/CalcMapGroupVisibility линкуются; runtime NOT RUN | Исходные расчёты на рабочей копии карты, проверка видимых групп/объектов; исходную карту не менять |
| Тени и эффекты | PARTIAL: CPU geometry/owners; Path runtime без GPU | Все используемые stencil/projected shadows, particles/billboards/Path в полном GPU кадре |
| Цикл/ввод/камера | E2E-5.33: original native form/loop с applet/HID pump программно подключены; HOST/QEMU scheduler и WINDOWS HAL form/input/focus tests PASS | Реальное выполнение на Switch: frame loop, RTS/direct control, камера, пауза и продолжительный бой |
| AI/навигация/объекты | PARTIAL: отдельные исходные algorithms/owners, RoadNetwork oracle | Полная симуляция карты: объектный порядок, RNG, движение и цели; сопоставление наблюдаемых результатов |
| Бой/физика/результат | OPEN end-to-end | Настоящие оружие/попадания/урон/коллизии, победа/поражение/отмена без автоматического исхода |
| UI/текст/миникарта/курсор | PARTIAL: native debug font/cursor/input и original UI text программно выполнены; Top/mm bitmap совпал с D3DX, minimap/offscreen и Switch hardware открыты | Читаемый интерфейс, picking, подсказки, minimap и управление на Switch handheld/docked |
| Звук и callbacks хозяина | PARTIAL: contracts/fixtures | Настоящие звуки/музыка/прогресс и callbacks основной игры, корректное восстановление её audio/input/renderer |
| Встраивание и выпуск | OPEN; в основной NRO MatrixGame не включён | Основная игра → бой → действительный результат и 6 счётчиков → возврат; повторный бой, производительность/VRAM, сохранность сейвов |

Число подключённых TU или зелёных CPU-тестов нельзя переводить в процент
готовности игры: наиболее крупные GPU и интеграционные этапы пока не приняты.
Общий процент и срок завершения **не установлены**. Измеримые рубежи ниже
позволяют видеть прогресс без произвольных весов и ложной точности.

## Порядок интеграции и зависимости

1. **Контрольная точка завершена в E2E-5.20.** Path, полная линковка, реальные
   Window/Device операции, успешный Windows Init, кадр карты, readback и Deinit
   подтверждены. Остановка ResizeOuter снята настоящим resize/Reset и явным
   owned presentation контрактом; произвольное borrowed устройство не сбрасывается.
   Воспроизводимые команды, хэши и снимки сохранены. Windows-релиз не является
   отдельной целью; цикл/бой и все режимы устройства этим кадром не принимаются.
2. **Native entry и providers Switch — PARTIAL E2E-5.21.** Путь native CGame
   отделён от Windows DLL identities. `InitializeNativeGame` и настоящие
   libnx/deko device/queue/images/swapchain включены в отдельный SDK ELF вместе
   с исходным frame closure; native runtime ещё не выполнен. ReadCaps проверяет
   настоящую загруженную shader pair; FVF lighting/TCI и draw routing подключены.
   Реализованы image-backed residency budget и mip upload/resource bindings;
   их аппаратное исполнение ещё не проверено. Реализованы CaptureState/restore
   с удержанием texture/VB/IB bindings. Applet/window Show/Update реализованы;
   cursor/input, ExchangeCursor и debug font реализованы. E2E-5.22 добавил
   genuine union assembly/BC1 и acquired-frame Clear/Scene/Present/rollback.
   ReadCreationParameters и presentation/window/reset для owned NWindow закрыты
   программно в E2E-5.23; физическая VI/GPU проверка ещё отсутствует.
   UI Text::Render/DeviceIdentity и original Rangers10/Verdana12–14 подключены
   в E2E-5.24: CPU raster target, реальные texture upload и ownership;
   original parser/layout/cache сохранены. Следующая source-traced ветка —
   StaticPrepare2 → projected shadow → offscreen::GetTarget (снят в E2E-5.25).
   Standalone targets и readback scopes подключены; следующий guard —
   shadow CreateRenderTexture(A8), texture surfaces и system readback сняты
   программно в E2E-5.26; A8 HAL/GLSL oracle — 9184 cases без расхождений.
   Native output/cache provider подключён в E2E-5.27: private root принадлежит
   runtime, Unicode/separators согласованы с CFile/filesystem; Windows HAL
   исполнил original cold-render/store/warm-load mini-map cache и два кадра.
   Exception-safe minimap scopes/cache ownership подключены в E2E-5.28:
   original cache/PNG и scopes проверены ASan/UBSan и ARM64/QEMU; Windows
   cold/store/warm Init/Draw сохранили прежние pixels.
   Original StoredStateBackend подключён в E2E-5.29: полный scalar capture/
   restore 103 render/13×8 sampler, raw lights/material, без удержания default
   resources. HOST/ARM64 исполняют исходный CDeviceState; Windows HAL сохраняет
   1476 material и 84 vertex cases. Native probe теперь вызывает исходный
   CheckLostDevice до acquire, release/store/reset ветка реально линкуется.
   Scalar storage не означает реализацию всех GPU states: активные неподдержанные
   неподдержанные scissor/sampling запросы явно отказывают до GPU draw/clear.
   [Подтверждения E2E-5.29](../reports/matrixgame-e2e529-stored-state.md).
   Native RasterCaps и pixel/table fog подключены в E2E-5.30: W/Z depth,
   LINEAR/EXP/EXP2, original 0.5/0.7 start/end, RGB-only fog перед blending.
   HAL/production GLSL: 1344 cases/168 gradients, 193536 pixels, delta0.
   Найден и исправлен native physical viewport pixel-center mismatch;
   logical viewport/projection/clear не менялись. HOST/ARM64 проверили 840
   HAL goldens и original scalar/COW restore; A8/R8 и material/FVF regression
   прошли. Switch GPU/map/frame всё ещё NOT RUN.
   [Подтверждения E2E-5.30](../reports/matrixgame-e2e530-fog.md).
   Native FLAT/GOURAUD и полный CW/CCW stencil pipeline подключены в E2E-5.31.
   WINDOWS HAL/production GLSL: 2310 stencil, 72 shading/primitive и восемь
   source-derived shadow sequences без расхождений. Это тестовые последовательности,
   а не исполнение полного original DrawShadows на карте. HOST ASan/UBSan и
   ARM64/QEMU проверили HAL goldens и исходный scalar/COW restore;
   devkitA64 связал полную native цель. Аппаратное выполнение ещё не проверено.
   [Подтверждения E2E-5.31](../reports/matrixgame-e2e531-stencil.md).
   Original CFormMatrixGame::Takt подключён в E2E-5.32: неизменное тело,
   native Init → Takt → acquire → Draw. Windows HAL исполнил две сессии:
   paused Takt/Draw, настоящий HINT_OK, десять шагов по 25 мс с Draw, Deinit.
   SDK удерживает полную map/side simulation closure без strong undefined.
   Это не полный native form/loop и не Switch runtime.
   [Подтверждения E2E-5.32](../reports/matrixgame-e2e532-simulation.md).
   Original CFormMatrixGame и L3GRun подключены в E2E-5.33: настоящая vtable,
   constructor/Enter/Leave, Mouse/Keyboard/SystemEvent, native applet/HID pump,
   measured delta/FPS и acquire/rollback вокруг исходного Draw. Windows HAL
   выполнил две form/loop сессии с Enter/Pause/Escape/scroll/MouseMove и
   release/recreate video resources через исходный focus/recovery путь.
   Реальный stored-light capture blocker устранён без fake SDK success;
   HOST ASan/UBSan и ARM64/QEMU scheduler/scalar/lease tests прошли.
   F9 screenshot явно Unsupported; Switch map/frame/control пока NOT RUN.
   [Подтверждения E2E-5.33](../reports/matrixgame-e2e533-form.md).
   [Подтверждения E2E-5.28](../reports/matrixgame-e2e528-minimap.md).
   Первый реальный отказ полного Switch Init без его запуска не установлен.
   [Подтверждения E2E-5.27](../reports/matrixgame-e2e527-output.md).
   [Подтверждения E2E-5.26](../reports/matrixgame-e2e526-render-textures.md).
   Switch offscreen/stored-state/reset runtime и восстановление после ошибки
   самой GPU queue открыты. Source-traced guards DrawShadows → SHADEMODE=FLAT
   и StencilCaps сняты программно. Следующий первый runtime-отказ пока не наблюдался.
   Следующий кодовый приоритет — оставшиеся реально вызываемые native paths
   и проверка полного native map/frame/recovery. Ограничение одноразового
   Init/Takt/Draw снято: native entry выбирает исходную форму и запускает
   исходный цикл. Его исполнение на Switch ещё не подтверждено.
   [Подтверждения и граница](../reports/matrixgame-e2e523-presentation.md).
3. **GPU фундамент.** Проверить на устройстве существующий allocator-fix
   кандидат. Проверить на устройстве подключённые texture/buffer bindings, включая
   `Device::BindStagingVertex/BindStagingIndex`, submission, synchronization,
   resource recreation и readback. Существующий standalone `NativeDekoSession`
   и ordinal `NativeSinglePass` не являются полным Device/material backend.
   Без аппаратного прогона сохранять HARDWARE_PENDING.
4. **Первая карта и кадр на Switch.** Свести native Init/RobotPreload/storage/
   PrepareMap с backend. Сохранить исходные GetSkin и material callbacks;
   реализовать используемые shader/material paths, terrain/water/объекты,
   visibility, необходимые тени/эффекты. Принимать по реальному кадру и
   управлению камерой; успешная CPU-загрузка отдельно от GPU-изображения.
5. **Первый бой.** Подключить исходный цикл и ввод, всю симуляцию/AI/оружие,
   интерфейс и звук. Выполнить сценарии победы, поражения, отмены и повторного
   входа; проверить настоящие результаты и освобождение всех ресурсов.
6. **Интеграция основной игры.** Вызвать бой через настоящий native bridge,
   вернуть результат и счётчики, восстановить устройства хозяина. Только затем
   кандидат интегрированной NRO и аппаратная приёмка. До отдельного разрешения
   основную NRO и существующие комплекты не заменять.

Без нового аппаратного запуска можно продолжать реализацию providers,
компиляцию SDK и адресные CPU-проверки. Это не закрывает пункты приёмки GPU,
полного боя или Switch-интеграции. Следующие кодовые этапы — необходимые
native paths исходного цикла, затем первая карта и кадр на Switch.
Минимальные контракты вынесены в [native providers](matrixgame-native-providers.md).

E2E-5.35: standalone entry получил реальную настройку Npad No1/Handheld и
FullCtrl до создания Window/Device. Borrowed SwitchInput policy не меняется.
Новая полная SDK ELF — COMPILE/LINK PASS; HID runtime HARDWARE_PENDING.
Актуальный порядок constructors/Init/RobotPreload/stor.Load/PrepareMap/Draw,
providers, ownership и cleanup сведён в [единый план Init](matrixgame-init-sequence.md).
[Отчёт E2E-5.35](../reports/matrixgame-e2e535-standalone-input.md).

E2E-5.36: из реального original Windows map draw stream установлен следующий
обязательный source-confirmed frame blocker: CMinimap::Draw, LINESTRIP рамки
камеры, LASTPIXEL=1. Production validators отвергают его на HOST/QEMU;
line rasterization не подменена и guard сохранён. Следующий кодовый приоритет —
точная реализация этого пути, а не объявление native кадра готовым.
[Полный capture и evidence](../reports/matrixgame-e2e536-actual-draw.md).

E2E-5.37 закрыл этот source-confirmed minimap blocker программно: настоящие
native aliased lines при LASTPIXEL=TRUE; 398 addressed + 20 original draw
HAL/GLSL exact, HOST ASan/UBSan / QEMU map replay и full WINDOWS HAL PASS.
SDK ELF связана; первая карта/кадр Switch всё ещё HARDWARE_PENDING.
LASTPIXEL=FALSE, AA и прочие неподдержанные режимы не объявляются готовыми.
[Отчёт и следующий адресный сценарий](../reports/matrixgame-e2e537-lines.md).

Дополнение E2E-5.34: исправлен source-proven Reset guard для renderer-only
default resources; внешние owners/snapshots/leases сохранены. HOST ASan/UBSan,
ARM64/QEMU и SDK link PASS; raw WINDOWS HAL state-block retry имеет 2 FAIL.
Статус этапа PARTIAL; первая карта на Switch по-прежнему HARDWARE_PENDING.
[Evidence и следующий открытый вопрос](../reports/matrixgame-e2e534-reset-owners.md).

Актуальный native text gate программно закрыт через оригинальный Font.2Ranger:
[свежие HOST/QEMU проверки и SDK link](../reports/matrixgame-text-current.md).
Следующий адресный сценарий original form достиг реального движения и стрельбы
на Windows HAL; исправлен доказанный mouse/key alias ownership в native Router.
[E2E-5.38](../reports/matrixgame-e2e538-selection.md).
Первая карта/Present на Switch по-прежнему HARDWARE_PENDING.

Новый actual line pixel oracle после движения робота воспроизвёл 6/114 FAIL
на правой границе миникарты. E2E-5.38 сохранил implicit logical viewport clip
при physical half-pixel shift: те же 114 cases / 54720000 pixels теперь exact,
398 line cases и 24 triangle viewport cases PASS. Baseline FAIL сохранён.
Native GPU результат остаётся HARDWARE_PENDING.

E2E-5.39 убрал source-confirmed native hard cap всего кадра в 2048 draws:
real HAL measurement показал 4038 primary draws в одном original frame.
Вместо пропуска draw используется submit batches с completion перед release,
одной исходной сценой и одним итоговым Present. HOST/ARM64 policy и SDK PASS;
GPU allocation, command capacity, VRAM/FPS и первый кадр Switch не проверены.
[Evidence E2E-5.39](../reports/matrixgame-e2e539-batches.md).

E2E-5.40 добавил реальные command backing pages и SDK cbAddMem. Pressure drain
сохраняет original scene/image; allocation/quota failure запрещает submit всего
ошибочного списка, а spills освобождаются после completion и clear/rebind base.
HOST/ARM64 protocol и SDK PASS; аппаратного overflow/growth/VRAM proof нет.
Постоянный discard reserve — 16 MiB, общий максимум command backing — 32 MiB.
[Evidence и оставшиеся gates](../reports/matrixgame-e2e540-command-memory.md).

## Параллельный P0 основной игры

- Общий GI cache: дефект освобождения при retain другого control доказан.
  Для `DATA/FormRuinsBG/2RCbg.gi` тип ресурса и точные frozen-исходники аппаратной
  сборки проверены. Известный Ruins eviction использует `TCGaiEC`, тогда как
  проблемный GI — `TCGiEC`; occurrence нужного вызова на этом экране не доказан.
  Причина повторяющегося текста остаётся OPEN.
- Исторические `manual-publish ENOENT` и зависания Switch остаются OPEN.
  Host-исправления и дополнительная диагностика их не закрывают.
- [Frozen cache audit](../reports/e2e5/hardware-20261023-cache-frozen-audit.md) и
  [аппаратные ограничения](../reports/e2e5/hardware-20261023-analysis.md).

Основная NRO, старые пакеты, ресурсы, аппаратные входы и пользовательские сейвы
должны сохраняться. Последний запрос пользователя разрешил commit/push исходников
и отчётов MatrixGame. Новых аппаратных проверок в этом срезе нет.

E2E-5.41 PARTIAL: explicit resource root прошёл HOST ASan/UBSan, ARM64/QEMU и SDK.
Первый полный original Windows Init/PrepareMap/Draw прошёл из чужого cwd, но
повторный отрицательный package-open Init завершился heap corruption. Windows
регрессия FAIL, этап не закрыт; следующий адресный блокер — этот failure path.
[Доказательства и ограничения](../reports/matrixgame-e2e541-resource-root.md).
