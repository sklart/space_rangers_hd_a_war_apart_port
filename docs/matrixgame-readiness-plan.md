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

E2E-5.42 закрывает этот Windows failure path: original DestroyPneumaticData
оставлял освобождённый static pointer. Исправление обнуляет owner/count;
baseline ASan double-free, candidate HOST ASan/UBSan, ARM64/QEMU, два полных
Windows сеанса и SDK PASS. Следующая подготовительная граница — ранний
долговечный journal и ограниченные stages одной diagnostic NRO; текущий probe
main ещё не готов для ограниченных аппаратных запусков.
[Доказательства и оставшиеся требования](../reports/matrixgame-e2e542-pneumatic.md).

E2E-5.43: portable durable journal, error policy и bounded-run policy проверены
HOST/ARM64; actual Windows Init пишет полный nested journal; отдельный diagnostic
ELF проходит SDK compile/link. Главная цель одной готовой NRO ещё не выполнена:
далее standalone GPU smoke/BC1/status/readback, окончательный entry safety audit
и пакет с manifest/assets/инструкцией. Native Switch execution HARDWARE_PENDING.
[Подробный статус](../reports/matrixgame-e2e543-journal.md).

Текущая передача E2E-5.27: подготовлена одна отдельная NRO
`dist/matrixgame-e2e527-diag/matrixgame-e2e527-diag.nro` с manifest,
SHA-256, оригинальными embedded font/shader assets и инструкцией.
Smoke/status/readback программно подключены, доступные CPU компоненты и
SDK compile/link проверены в E544. Настоящий native Init/карта/кадр/cleanup
по-прежнему SWITCH HARDWARE NOT RUN. До анализа первого ручного запуска
вторая сборка и второй аппаратный запуск не назначаются.
Текущее ТЗ запрещает новые commit/push; прежняя публикация является исторической.
[Инструкция](matrixgame-e2e527-first-run.md),
[аудит передачи](../reports/matrixgame-e2e527-diagnostic-delivery.md).

После первого аппаратного журнала: реальный GPU prefix дошёл до BC1 upload,
но до Init/Draw не добрался из-за отсутствующего раннего window attachment.
В E546 diagnostic owner исправлен без изменения original Init/Draw; адресные
ASan/UBSan, ARM64/QEMU и SDK PASS. Подготовлен отдельный пакет run2,
кандидат остаётся HARDWARE_PENDING.
[Разбор первого запуска](../reports/matrixgame-e2e545-hardware-run1.md),
[исправление и второй пакет](../reports/matrixgame-e2e546-window-handoff.md).

Второй аппаратный запуск: attachment уже подключён, BC1 upload выполнен;
до первого draw/Present/Init ещё не дошло. Отказ — 59905 (Kernel TimedOut)
при nonblocking appletGetMessage, ошибочно считавшийся фатальным. В E547
исправлена обработка пустой очереди с адресным воспроизведением исходной
функции libnx на HOST/ARM64. Новых аппаратных запусков сейчас нет;
продолжается офлайн-доработка без создания третьей NRO.
[Аппаратная граница и исправление](../reports/matrixgame-e2e547-applet-poll.md).

Офлайн после run2: E548 обеспечивает command-buffer-before-backing при
constructor unwind; E549 даёт раннему smoke explicit-session readback без
фиктивного ReadyDevice. E550 устраняет воспроизведённую staging ownership
утечку, проверяет known-lost queue до transfer dispatch и удерживает pending
staging до channel close. E551 подключает существующий diagnostic RunControl
к управляемому foreground wait, сохраняя HOS exit и original attachment flow.
Адресные HOST/ASan/UBSan, ARM64/QEMU и SDK PASS; третья NRO не создана,
исправления аппаратно не проверены. Последняя реальная граница — run2 до draw.
[E548](../reports/matrixgame-e2e548-command-unwind.md),
[E549](../reports/matrixgame-e2e549-preinit-readback.md),
[E550](../reports/matrixgame-e2e550-transfer-admission.md),
[E551](../reports/matrixgame-e2e551-drawable-checkpoint.md).

E552 устраняет временный heap vector на каждую уникальную вершину в native
indexed gather. Прежний batch decode и остальное тело indexed draw сохранены;
проверены 864 layout/stride cases и 6144 gathers с побайтным совпадением данных.
HOST ASan/UBSan, ARM64/QEMU и SDK compile/link PASS. Это сокращение выделений
памяти, без измерения FPS и без подтверждения GPU-кадра. Новая NRO не создана;
последняя аппаратная граница остаётся run2 до draw/Present/original Init.
[Проверки E552](../reports/matrixgame-e2e552-vertex-decode.md).

E553 убирает повторный applet pump из AcquirePrimary внутри открытого frame.
Исходная цепочка focus loss может через minimap backup войти в FlushRecording;
адресный CPU-тест воспроизводит сброс acquired frame и отказ для открытой scene.
Кандидат обрабатывает queued focus/exit на следующей границе; оригинальное
освобождение ресурсов сохранено. HOST ASan/UBSan, ARM64/QEMU и SDK PASS;
полный original callback на карте и Switch focus/resume остаются неподтверждёнными.
[Разбор и пределы E553](../reports/matrixgame-e2e553-frame-focus.md).

E554 объединяет geometry/indices/descriptors каждого native draw в один
owned GPU block. Uniforms и packet byte budget сохранены; indexed draw
требует три allocations вместо шести, non-indexed — три вместо пяти.
HOST ASan/UBSan и ARM64/QEMU подтверждают payload/offsets/lifetime; полный
SDK compile/link PASS. Это сокращение запросов memory blocks, без измерения
native FPS, VRAM savings или подтверждения первой карты.
[Проверки E554](../reports/matrixgame-e2e554-draw-upload.md).

P0 общего GI-кэша: E555 заменяет ручной binding прежнего baseline настоящим
AcquireCachedGi с двумя вложенными retains на реальном 2RCbg.gi. ASan
подтверждает use-after-free после чужого EvictData. Caller-retained guard,
exact-class lookup и peer-aware TrimToBudget проверены отдельно. Forced policy
не менялась; реальная eviction проблемного GI в аппаратном переходе и причина
повторяющегося текста остаются OPEN.
[Контракт и пределы E555](../reports/e2e5/cache-retain-contract-e555.md).

E556 добавил стандартную отдельную `matrixgame-native-diagnostic-nro` target.
Пользовательский logs2 (E557) подтвердил hardware BC1 draw/readback/Present и
original Init до `g_Config.ApplyGammaRamp`, где отсутствовал native provider.
E558 реализует настоящий presentation LUT с отдельным scene backing, gamma
snapshot/restore и проверкой выходного GPU image в diagnostic smoke. HOST
ASan/UBSan, ARM64/QEMU, Windows gamma reference, точный GLSL на Mesa и полная
SDK/NRO сборка PASS. Новая NRO находится в `build/matrixgame/e2e558-gamma/sdk/diagnostic-nro`;
она аппаратно не проверена. Полный native Init/PrepareMap/map frame остаются
PARTIAL / HARDWARE_PENDING. [Текущий gamma candidate](../reports/matrixgame-e2e558-gamma.md).

E559 проверяет целиком original CMatrixConfig::ReadParams на настоящем data.txt:
HOST ASan/UBSan, ARM64/QEMU и Windows CPU reference PASS, четыре полных прохода
на платформу с совпадающими numeric/string observations и repeated Clear.
Уточнено, что первый gamma caller в E557 — ReadParams, а не ApplySettings.
Это закрывает прежнюю CPU prefix-only границу parser; full native Init/map/GPU
не объявляются исполненными. Production E558 inputs и NRO не менялись;
новая NRO не создаётся. [Доказательства E559](../reports/matrixgame-e2e559-config-tail.md).

E560 — ограниченный source audit после config: sampler ApplySettings сам не
выполняет GPU dispatch; найденный общий bitmap-decode guard не установлен как
обязательный caller просмотренных Init/map/transition paths. Zero-count vertex
bindings gamma pass сверены с официальным deko3d v0.5.0 и не требуют исправления.
Новой runtime-проверки нет. Следующая фактическая граница требует пользовательского
аппаратного журнала E558; самостоятельного доступа к устройству нет.
[Source audit и пределы продолжения](../reports/matrixgame-e2e560-next-init-boundary.md).

После E560 офлайн-разработка продолжена без устройства. Текущие программные
дополнения: E561 настоящий unmanaged bitmap loader; E562 single-sample
enable/mask state без ложного отказа UI; E563 optional wireframe selection;
E564 исправление controller typematic, воспроизведённое на настоящем роботе
в Windows HAL игре. Для E564 HOST ASan/UBSan, ARM64/QEMU, original Form/Input,
два actual original Windows Init/map/selection/recovery sessions и отдельная
стандартная SDK/NRO PASS. Это не расширяет последнюю Switch runtime границу.

Последний hardware run остаётся ReadParams/ApplyGammaRamp до map constructor.
Standalone diagnostic также ещё не имеет настоящего Rangers resource/audio
provider для CSound symbolic names: наличие полного callback bridge и отдельного
SDL PCM output не доказывает их подключение. Эти компоненты, native map/image/
VRAM/HID/focus и queue-loss recovery остаются явными gates.
[Текущая сборка, воспроизведение и точные пределы E564](../reports/matrixgame-e2e564-pad-repeat.md).

E565 продолжил офлайн-интеграцию: optional copied SRobotsSettings в native entry,
проверки actual ApplySettings/AF clamp и Init prefix rollback, исправление
native minification AF policy без изменения Windows reference branch.
HOST ASan/UBSan, ARM64/QEMU, Windows scalar consumers и отдельная стандартная
SDK/NRO PASS. Полный native Init, Rangers resource/audio services и перечисленные
аппаратные gates остаются открытыми.
[Новая отдельная сборка и воспроизводимые проверки E565](../reports/matrixgame-e2e565-native-settings.md).

E566 закрыл удержание primary readback callback после AbortNativeFrame, включая
неисправную/отсутствующую queue. Host ASan/UBSan и ARM64/QEMU actual prefix
regression PASS, отдельная SDK/NRO PASS. Полный GPU recovery этим не доказан.
Выявлен следующий конкретный программный путь: native F9 screenshot с настоящим
кадром, оригинальным CBitmap PNG и private output provider; пока Unsupported.
[Точные доказательства и незакрытая семантика F9](../reports/matrixgame-e2e566-readback-abort.md).

E567 реализует программный native F9: completed scene slot, SDK readback,
RGB conversion, original CBitmap PNG/CFile и private Screenshots directory.
Host ASan/UBSan, ARM64/QEMU, Win32 scalar/file reference, независимое PNG decode
и отдельная SDK/NRO PASS. Captured pixels в этих тестах синтетические; настоящий
GPU capture и F9 runtime остаются HARDWARE_PENDING. Предыдущая граница native
Init и потребность в реальных Rangers resource/audio services не закрыты.
[Текущая NRO и точный scope E567](../reports/matrixgame-e2e567-screenshot.md).

E568 добавил session ownership полной Rangers callback table в native entry,
сохранение binding через original cleanup и preflight всех 16 slots.
Host ASan/UBSan, ARM64/QEMU original progress/Init-prefix rollback и отдельная
SDK/NRO PASS. Это даёт путь подключения готового EngineCallbacks bridge; сами
реальные standalone resource/audio/text/music services остаются незавершёнными.
Native Init/map и аппаратные gates по-прежнему открыты.
[Точная граница интеграции и воспроизведение E568](../reports/matrixgame-e2e568-native-services.md).

E569 продвинул реальные host services: callback producer из всех оригинальных
Robot functions и owner оригинального sound/cache context. Real-resource sound
host run и независимый PCM PASS; новые TUs ASan/UBSan PASS, старые original units
не инструментированы. SDK archive compile и existing-bridge link PASS; новые
host symbols в SDK probe не достигаются. Standalone text/music/progress/render-
release context и full SDK host link ещё открыты; частичная таблица в NRO не
устанавливалась. Последняя диагностическая NRO E568 сохранена.
[Результаты и точные пределы E569](../reports/matrixgame-e2e569-rangers-sound.md).

E570 продвинул actual text/free-text callbacks через original AFT font/cache.
Исправлен real null-clip crash только в native host producer. Host differential,
частичный ASan/UBSan (новые TUs) и SDK archive compile PASS; существующий bridge
ELF не достигает нового host code. Music/progress/render-release context,
full SDK host link и native session wiring ещё требуют работы. Последняя NRO
E568 и аппаратная граница ReadParams/ApplyGammaRamp сохранены.
[Точные text gates и reproducer E570](../reports/matrixgame-e2e570-rangers-text.md).

E571 закрыл отдельную host проверку трёх music callbacks с настоящей Music.Robot
playlist, CaseConv из языкового config и Vorbis stream через SDL dummy. Оба
режима effects+music/music-only и cleanup workers PASS; ASan/UBSan только новых
TUs. Progress/release подтверждены в original standalone ветках без GUI device
и texture caches. SDK archive compiled, но full SDK host link, общий config
owner и native runtime composition ещё необходимы. Новая NRO не выпускалась;
E568 NRO и прежняя аппаратная Init-граница сохранены.
[Доказательства и перечень недостающих компонентов E571](../reports/matrixgame-e2e571-rangers-music.md).

E572: full SDK host link PASS с реально достижимыми producer/owners и full16
EngineCallbacks. Matching host/partial-ASan full16 execution PASS с настоящими
sound/AFT/music resources; SDL dummy. SDK probe не содержит CGame::Init и не
выполнялся. Следующие software gates: общий package/config/language/mod owner,
native runtime composition, единая Init/map-цепочка и обновлённая diagnostic NRO.
Последняя NRO E568 и hardware boundary ReadParams/ApplyGammaRamp сохранены.
[Отдельная build-цель и доказательства E572](../reports/matrixgame-e2e572-rangers-host-sdk.md).

E573: общий package/config/language/mod owner реализован и сверён побайтно с
оригиналом на пяти cases, включая настоящий SR2LoadingScreen. Borrowed audio/
music/full16 composition на host PASS; данные живут до конца service teardown.
Устранена доказанная DAT-loader exception leak в generated copies, оба loaders
имеют ASan failure regression. Full SDK link PASS, SDK execution NOT RUN. Далее:
native runtime/Init/Deinit и package binding, audio-disabled settings, единая
Init/map-цепочка и обновлённая diagnostic NRO. Hardware gate не продвинут.
[Результаты, воспроизведение и границы E573](../reports/matrixgame-e2e573-rangers-data.md).

E574: owning RangersHostSession и режимы 00/01/10/11 sound/music PASS на host и
ASan новых TUs, включая backend failure rollback. Два ранних original native Init
rollback цикла используют real callbacks/roots и сохраняют host до конца Deinit.
Owning-host SDK ELF linked, NOT RUN; CGame::Init в этот SDK ELF не входит.
Далее — стандартный diagnostic entry с owning host, joint native/deko+Rangers
SDK link и новая отдельная NRO. Hardware boundary не изменился.
[Точная граница и воспроизведение E574](../reports/matrixgame-e2e574-rangers-host-session.md).

E575: joint native/deko+original Rangers diagnostic entry и стандартная отдельная
NRO подготовлены. SDK compile/link/package/audit PASS; real host lifetime охватывает
original Deinit/error/cancel. Ресурсы описаны отдельным read-only manifest.
Combined NRO и полный Init/map ещё не исполнялись. Следующие gates: актуальная
runtime граница, first map frames, hardware graphics/audio/HID/focus/F9/recovery;
ARM64/QEMU и Windows execution новой hosted цепочки также открыты.
[Новая NRO и доказательства E575](../reports/matrixgame-e2e575-hosted-diagnostic.md).

E576: ARM64/QEMU execution hosted services закрыт: 331 effective original +7
native units +probe compiled, четыре audio modes PASS; real AFT pixels exact x86
match и два early native Init rollback cycles PASS. Workspace-only static
SDL dummy/Ogg/Vorbis prefix не меняет SDK/system. ARM ASan, GPU/полный Init/map
и Switch execution не заявляются. E575 NRO сохранена; Windows reference hosted
цепочки и аппаратные gates остаются открытыми.
[Доказательства и воспроизведение E576](../reports/matrixgame-e2e576-rangers-arm64.md).

E577: Windows hosted service reference закрыт настоящим MSVC x64 исполнением:
four audio modes/full16/failure/retry/teardown и exact AFT pixels PASS. Реальный
Windows lock ABI исправлен в private generated runtime и сверен с SDK;
recursive/contended locking и clock writer probes PASS. Проверены только
необходимые новые delta compiles на host-ASan/ARM64/SDK; большие старые suites
не повторялись. Следующий software gate — Windows original native Init prefix.
Полный native Init/map, аппаратные graphics/audio/input/recovery gates открыты;
сохранённая E575 NRO не пересобиралась.
[Результаты, воспроизведение и границы E577](../reports/matrixgame-e2e577-rangers-windows.md).

E578: Windows original native Init/rollback prefix закрыт реальным исполнением,
без изменения probe assertions. Fresh x64 production MatrixGame archives
(159 actual objects) совместно с E577 Rangers host прошли два early caps-refusal
cycles и cleanup/reentry. Цепочка владения проверена на host, ARM64 и Windows.
Дальше остаются реальные map/renderer зависимости полной Init-цепочки, complete
Init/map/frames и hardware gates; новый GPU success не моделировался. E575 NRO,
upstream/frozen/game/saves и прежние артефакты сохранены; commit/push нет.
[Воспроизведение и точные границы E578](../reports/matrixgame-e2e578-hosted-windows-native.md).

E579: original full Init/map/visible frames с настоящим Rangers host проверен
на Windows HAL в двух сессиях с cleanup/reentry. Устранены original/full-cache
type mismatch и два pointer-width дефекта сортировки; addressed factory tests
на Windows/ASan/ARM64 PASS. Стандартная отдельная E579 NRO собрана и проверена,
без изменения основной/прежней NRO. Дальнейшая обязательная software работа —
native transition framebuffer capture: GDI capture branch на Switch пока явно
Unsupported. Full native/deko Init и hardware gates не закрыты PC evidence.
[Результаты и точный remaining component E579](../reports/matrixgame-e2e579-hosted-windows-map.md).

E580: обязательный native transition capture интегрирован: настоящий завершённый
status frame сохраняется до Reset, original transition потребляет owned pixels
один раз. Windows HAL Reset/RGB oracle и full hosted map regression PASS;
ASan/UBSan/ARM64 CPU contracts, failure cleanup и 108 PNG PASS. Новая отдельная
SDK NRO собрана, full native/deko execution NOT RUN. Следующий фактический native
отказ без запуска не установлен; аппаратные Init/image/audio/HID/recovery gates
открыты. Upstream/frozen/resources/saves и прежние NRO сохранены.
[Доказательства и воспроизведение E580](../reports/matrixgame-e2e580-loading-capture.md).

E581: совместный original form/loop + owning Rangers host исполнил два Windows
HAL interactive цикла: Enter/Pause/Escape/scroll, по 208 Draw/5125 ms unpaused
simulation accounting, focus recovery, changing frames и полный cleanup PASS.
Это новая композиция настоящих production TUs, без изменения production source.
E580 NRO сохранена; host/ARM64/SDK suites не повторялись. Native/deko execution,
реальный HID/audio и полный бой/результат/возврат пока не доказаны.
[Результаты и следующие gates E581](../reports/matrixgame-e2e581-hosted-windows-loop.md).

E582: исправлен доказанный ASan overlap в original AI ignore FIFO и пропущенный
timestamp новой цели. Generated source delta ограничен MatrixEnvironment.cpp;
два настоящих world-identity oracle/Windows HAL teardown PASS. Host ASan/UBSan
и ARM64 здесь проверены только компиляцией изменённых TUs. Новая отдельная SDK
NRO; прежняя E580 сохранена. Полный native/deko runtime и hardware gates открыты.
[Воспроизведение и точная область доказательства E582](../reports/matrixgame-e2e582-ai-ignore.md).

E583: original RemoveOrder использует безопасный overlapping shift, original
ProcessOrdersList читает ровно count-1 приказов. Обе ошибки воспроизведены
раздельными ASan baselines; genuine world robot queue/refcount oracle и два HAL
cleanup цикла PASS. Generated delta — только MatrixRobot.cpp. Host-ASan/ARM64
новых TUs compile-only, отдельная SDK NRO; прежняя E582 сохраняется. Full native/
deko runtime и hardware/battle acceptance остаются открытыми.
[Доказательства E583](../reports/matrixgame-e2e583-robot-orders.md).

E584: настоящая player-group MoveTo команда и original navigation/Takt доказали
движение живого world robot в двух hosted Windows HAL loop сессиях: 94.624159
world units за 5000 мс, distance² 100→5, queue empty. Original input/menu/camera,
focus recovery/frames/cleanup также PASS. Production source не менялся;
сохранена E583 NRO, host-ASan/ARM/SDK suites не повторялись. Это command API
probe без physical HID; full native/deko и battle outcome/return ещё открыты.
[Точное новое runtime доказательство E584](../reports/matrixgame-e2e584-hosted-movement.md).

E585: два hosted Windows HAL цикла доказали настоящую arcade-стрельбу через
configured input: selection/UI, 4 original Fire/11 WeaponHit callbacks за цикл,
отпускание/остановка, recovery/frames/cleanup PASS. Наблюдение только в test copy,
original weapon body проверен на точное сохранение. Production source и E583
NRO не менялись; hardware HID/audio/native-deko и полный бой остаются открытыми.
[Доказательства и fixture preconditions E585](../reports/matrixgame-e2e585-hosted-firing.md).

E586: подготовлена отдельная interactive SDK цель для будущего ручного теста
без конфликта B/right-click и Plus/menu с диагностическим выходом. Phase deadline,
frame gating после transition, monotonic/reuse protection и final frame telemetry
реализованы. Windows/ASan/ARM64 CPU policy PASS; interactive и quick NRO стандартно
собраны с zero strong undefined/fonts/package PASS. Original game sources и
E583 NRO сохранены. Обе NRO NOT RUN, hardware/full battle acceptance открыт.
[Артефакты, воспроизведение и ручной тест E586](../reports/matrixgame-e2e586-interactive-diagnostic.md).

E587: original WeaponHit→virtual Damage действительно изменил состояние двух
map objects в каждом из двух Windows HAL hosted loops: поджог и TTL 0→10000.
На сессию 7 Damage, HP decrease/удаления не наблюдались. Test-only guards и
read observers, точное сохранение original bodies, recovery/cleanup и PNG
audit PASS. Обе E586 NRO сохранены; новые ASan/ARM/SDK suites не запускались.
Enemy HP/full battle и native/deko hardware execution остаются открытыми.
[Прямые измерения Damage E587](../reports/matrixgame-e2e587-hosted-damage.md).

E588: original PGOrderAttack → AI → WeaponHit → virtual Damage реально снизил
HP hostile robot. Два одинаковых Windows HAL сценария, по два Damage события
с уменьшением HP на 25 getter units каждое. Дополнительные original form
ticks ускоряют симуляцию до первого HP decrease; realtime/FPS не измерены.
Cleanup/journals/PNG/source/protected hashes PASS. Production и E586 NRO
сохранены. Остались полный бой/возврат результата и native/deko hardware gates.
[Сценарий и доказательства E588](../reports/matrixgame-e2e588-hosted-battle.md).

E589: SaveOriginalGameResult вызывает исходный CGame::SaveResult при живом
full-game owner; до Init/после Deinit отклоняет чтение без изменения output.
Native diagnostic пишет шесть signed snapshots до cleanup и принимает
штатную сдачу state4. Windows original UI surrender/return/stats/cleanup PASS
дважды, policy Windows/ASan/ARM64 PASS; новые quick/manual SDK NRO NOT RUN.
Full battle win/loss, galaxy transition и native/deko hardware gates открыты.
[Результат, NRO и оставшиеся проверки E589](../reports/matrixgame-e2e589-original-result.md).

E590: настоящий полный бой до поражения выполнен дважды на Windows HAL без
подставных HP/positions/side status/result. State2 и шесть actual statistics
переданы в оригинальный Robot global result. Исправлены callback pointer
truncation огнемёта и совместимость actual Sound.Flame RIFF. Новые SDK quick/
manual NRO собраны с отдельным archive (заменён только EC_CacheSound.o).
Galaxy context действительно отсутствует: Galaxy/Player/government screen/
Robot Run dispatch/device = 0, callbacks = 1. Требуется реальная in-memory
galactic session + borrowed native battle dispatch; UI/reward/history path
не исполнялся и остаётся PARTIAL. Победа/hardware acceptance открыты.
[Бой, артефакты и недостающий galactic caller E590](../reports/matrixgame-e2e590-completed-battle.md).

E591: reusable native TRobotRun entry готов и исполнил три полных original
mission losses в Windows HAL reference mode под partial ASan. Caller settings
readonly, output сохранён при отказе, context roots/defaults copied, MatrixGame
cleanup завершён до возврата статистики при живом host. Host/ARM compile-only и
SDK standard library + retained API ELF link PASS; новой NRO нет, E590 сохранены.
Остались native Initialize/Finalize/Support и complete Robot table, настоящие
Galaxy/Player/Gov renderer session и исходные FRun/OnOpen reward/history/transition.
[Реализованный Run и точные границы E591](../reports/matrixgame-e2e591-native-rangers-run.md).

E592: все четыре native RobotInterface methods реализованы; Install проверяет
реальные callbacks и renderer Support до публикации. Настоящий Run завершил
полный Windows HAL бой, original FinalizeRobotRuntime освободил binding,
reinitialize и failed Init rollback PASS. Policy Windows/host ASan/ARM QEMU
runtime, полные TU cross compile и standard SDK library/API ELF zero-U PASS.
Новая NRO не создавалась. Остались настоящая Galaxy/Player/Gov renderer/UI
session, исходные FRun/OnOpen reward/history/transition, победа и Switch runtime.
[Complete native interface и оставшаяся цепочка E592](../reports/matrixgame-e2e592-native-rangers-interface.md).

E593: настоящий FRun caller теперь исполнен на Windows HAL до естественного
поражения и возврата six statistics, с реальными base GI receiver/SDL foreign
window/SDK Reset/gamma/volumes/invalidation/cwd cleanup/Finalize. Partial ASan
exit0; Win64 Reset adapter сохраняет 56-byte original records и связывает
opaque window token с полным SDK HWND. Отдельные SDK Reset failure/retry и
canaries PASS, devkitA64 configure-only exclusion PASS. NRO не менялись.
Это ещё не TfGov::OnOpen или Galaxy return: необходимы настоящий in-memory
Galaxy/Player, government GI-tree/renderer, rewards/history и transition.
Естественная победа и native/deko FRun/hardware остаются NOT RUN.
[FRun, ABI-исправление и точная оставшаяся цепочка E593](../reports/matrixgame-e2e593-rangers-caller-reset.md).

E594: настоящий Gov GI-tree/layout доступен как scoped production owner:
395 controls, original callbacks, atomic publication, original destruction
и borrowed ML/Data/ZPos restoration. Windows/host sanitizer/ARM64 QEMU runtime
PASS; CMake government target и отдельный retained-API SDK ELF zero-U PASS,
новая NRO не создавалась. Для original OnOpen остаются реальные in-memory
Galaxy/Player/Planet, TCCInterface/Buffer, HangarScreen и script runtime owners,
затем battle request/result/reward/history/galactic transition. Draw/победа/
native-deko FRun/hardware открыты. External logs2.7z absent, hash NOT VERIFIED.
[Состояние правительственного экрана и точные gates E594](../reports/matrixgame-e2e594-government-session.md).

E2E-5.95: original Script host lifecycle PASS на Windows partial ASan,
host ASan/UBSan и ARM64/QEMU. Port owner останавливает дополнительный
ScriptRequestThread и освобождает 12 builtin-контейнеров, пропущенных исходным
FinalizeScriptEngine; прежний POSIX/QEMU exit timeout устранён. Два worker,
14 событий, VM result42/path-pool recycle/foreign owner guards проверены.
Отдельная standard SDK library и retained API AArch64 ELF zero-U PASS.
Общая цель PARTIAL: настоящие Galaxy/Player/Planet generation, Gov OnOpen
с наградами/историей/возвратом, естественная победа и native/deko FRun остаются
открытыми. NRO E590 сохранены; новые NRO не создавались; Switch NOT RUN.
[Подробности и evidence E595](../reports/matrixgame-e2e595-script-session.md).

E2E-5.96: real Hangar config/layout (341 control), TCCInterface и неизменённые
TGalaxy_Create/Destroy PASS в двух циклах Windows partial ASan, host ASan/UBSan
и ARM64/QEMU. SDK standard library/retained API AArch64 ELF zero-U PASS.
Глобальные владельцы публикуются после успешной инициализации; foreign/duplicate
guards, partial-layout rollback и borrowed-root cleanup проверены. Generation/
Player NOT RUN: нужны оригинальные gameplay definitions и ship/script templates,
затем полный original campaign generator. Gov result/reward/history/return,
победа, native/deko FRun и аппаратный gate остаются открыты. NRO не менялись.
[Подробности и evidence E596](../reports/matrixgame-e2e596-galaxy-prerequisites.md).

E2E-5.97: полный original InitializeGlobalUiRuntime подключён через scoped
owner: 36 экранов, 10826 controls, 40 script templates, 32 global variables,
207 planet templates и original gameplay/visual/map/greeting definitions.
Два цикла, parser-exception rollback и cleanup PASS на Windows partial ASan,
host ASan/UBSan и ARM64/QEMU; SDK library/retained API ELF zero-U PASS.
Пропущенные оригиналом AchievementsScreen/SpaceObjectUiLoop освобождаются
настоящими деструкторами. Перед полной генерацией требуется LP64 allocation
fix TStorageEntry (literal12 vs actual sizeof20 подтверждён devkitA64).
Campaign/Player/Gov result/награды/возврат/победа и native FRun остаются открыты.
Игровые данные, сохранения и прежние NRO сохранены; Switch NOT RUN.
[Подробности и evidence E597](../reports/matrixgame-e2e597-global-ui.md).

E2E-5.98: настоящая кампания stage8/turn301 с 73 звёздами и Player/Star/Planet
сгенерирована original worker и warmup300; Windows partial ASan и ARM64/QEMU
exit0. Host functional stage8 выполнен, но LeakSanitizer FAIL:226478 bytes/
4272 allocations. Native StorageEntry allocation исправлен в overlay;
подключены original QuestMessages и localized PlanetQuest cache entries.
SDK compile/link обновлённой original dependency и campaign owner PASS,
zero strong undefined. Утечки оригинального cleanup, Gov result/rewards/
history/return, победа и native/deko FRun остаются открытыми. NRO и сохранения
не менялись; новый пакет до закрытия текущих gates не создавался.
[Подробности, source chain и отрицательный sanitizer gate E598](../reports/matrixgame-e2e598-campaign.md).

E2E-5.99 закрывает выявленный E598 LeakSanitizer failure: реальная полная
кампания и cleanup ASan/UBSan/LSan exit0; VM borrowed-alias/lifetime probe
PASS. Windows partial ASan и ARM64/QEMU PASS; SDK standard target, отдельный
архив с семью заменёнными TUs и retained API ELF zero-U PASS. Дополнительная
instrumentation обнаружила и закрыла surface grid OOB и signed seed overflow;
960 независимых low32 arithmetic compile assertions PASS host/devkitA64.
Upstream/frozen/game inputs/старые NRO сохранены; аппаратный gate открыт.
[Cleanup и аудит E599](../reports/matrixgame-e2e599-campaign-cleanup.md).

E2E-6.00: Windows reference partial ASan подтвердил оригинальные взлёт,
перелёт и посадку в сгенерированной кампании, затем настоящие Gov layout/
OnOpen/OnClose с исходным диалогом. Это пока CPU navigation/Gov entry proof,
не полный GUI или battle return. Original memory snapshot round-trip выявил
недостающую инициализацию achievement definitions; соответствующий native
owner и поддержка замены мира в campaign owner проверяются. Реальный FRun
из Gov, обработка исходов/наград/истории, победа, native/deko runtime и новая
интегрированная NRO остаются незавершёнными.
[Навигация и текущая граница E600](../reports/matrixgame-e2e600-campaign-navigation.md).

E600 memory-only snapshot Windows PASS: actual world recreation, player identity,
money/storage/location/turn и persistent counters сохранены. Добавлены scoped
original achievement definitions/local loader и campaign-slot lifetime вместо
stale Galaxy pointer. SDK campaign/achievement/loader cleanup archive zero-U
PASS, но software readiness всё ещё PARTIAL: host snapshot LSan30200/1214,
две constructor-resource replacement fixes проверены Windows; повторный host
поймал независимый signed revolution-seed overflow в warmup. Candidate low32
oracle77 PASS host/devkitA64, runtime и ARM64 snapshot продолжаются. Победа,
полный battle return, native/deko и новая интегрированная NRO не закрыты.

E601 закрывает host snapshot LSan: полная original campaign/navigation/Gov/
memory snapshot/cleanup ASan/UBSan/LSan PASS. Revolution low32 seed oracle77
host/devkitA64 PASS; SDK aPlanet update + standard owner closure zero-U PASS.
ARM64 snapshot PASS предшествующей версии; seed delta compile-only. Проверен
actual production generator: исправлены five early-return routes и source-cache
helper fingerprints; все8 полных generated CPP совпали с compiled SDK inputs.
[Component evidence E601](../reports/matrixgame-e2e601-revolution-seed.md).

E602: настоящий интегрированный бой на Windows D3D9 HAL дошёл до natural defeat
и original Gov reward/history/dialog/requested return после настоящего snapshot
restore. Partial MSVC ASan, original CGame::Init journal, actual stats и SDK
gamma/readback/cleanup PASS в нескольких сценариях. UI StartPlanetBattle auto-save
handler не запускался; main Galaxy UI Run loop и физический Gov UI display этим
fixture не доказываются. Победа остаётся NOT PROVEN; текущий обычный Winter1_1e
сценарий выбирается из real catalog, без подстановки результата или характеристик.
Native/deko FRun и обновлённая отдельная NRO остаются следующими gates.
[Natural defeat, настоящий возврат и оставшиеся границы E602](../reports/matrixgame-e2e602-campaign-frun.md).

E603: C++ exception после настоящих Init/Draw теперь проходит original caller
restore и memory snapshot restore, сохраняя исходный тип ошибки. Windows
partial ASan exit0, gamma SDK readback, неизменные money/history и полная
cleanup PASS. Production generator включает Robot/fGov overlays; отдельный
SDK archive меняет только эти два member, retained campaign API ELF zero-U
PASS. Сам инвариант владельца campaign не ослаблялся. Ошибка восстановления
snapshot/device отдельно не инжектировалась; natural victory/native-deko/new
diagnostic NRO по-прежнему открыты.
[Exception recovery и пределы E603](../reports/matrixgame-e2e603-campaign-exception.md).

E604: две новые отдельные standard libnx/deko3d NRO собраны, проверены и
упакованы без игровых ресурсов/сохранений. Основная и E590 NRO сохранены.
Normal natural-defeat return с exception overlays E603 PASS. Следующий основной
gate: integrated native/deko caller/presentation/return; текущая diagnostic
entry его не вызывает. Победа NOT PROVEN, hardware HARDWARE_PENDING.
[Артефакты и воспроизведение E604](../reports/matrixgame-e2e604-current-diagnostic.md).

E605: программная зависимость shared native presentation добавлена: настоящий
deko RGB565 presenter и scoped owner в native Robot interface. Host/Windows/
ARM64 CPU pixel gates PASS; SDK renderer/presenter/interface compile + retained
campaign API link zero-U PASS. Full native caller FRun всё ещё открыт из-за
D3D9 reset/gamma/window операций и отсутствующей общей native Gov entry.
GPU runtime NOT RUN; E604 diagnostic artifacts сохранены.
[Компонентная проверка и конкретные следующие шаги E605](../reports/matrixgame-e2e605-rangers-canvas.md).

E606: caller reset/window handoff адаптирован через настоящие provider leases;
native FRun restore больше не требует D3D9 Reset, caption или OS cursor loops.
Windows HWND/D3D9 reference exception и natural-defeat result-return PASS;
SDK production Robot/native owner/caller compile + retained link zero-U PASS.
Проверены реальные baseline target/viewport/gamma и window reacquisition.
Остаются full native original UI/script/campaign/Gov context, input mapping,
целостная native diagnostic entry/NRO и hardware acceptance. Победа NOT PROVEN.
[Точная цепочка E606](../reports/matrixgame-e2e606-caller-presentation.md).
<!-- E607 evidence extension; preceding milestones retain their version scope. -->

E607: production UI context реализован без записи CFG/сохранений. Реальные
config/TCC/canvas/palette/cursors/script/global UI проходят host sanitizers,
ARM64/QEMU и Windows; SDK compile/link zero-U PASS. Исправлен renderer rollback
после позднего allocation failure. Общая native UI/campaign/Gov entry, input,
новая интегрированная NRO и GPU runtime остаются открытыми. Полный бой с этим
новым контекстом — следующий программный gate; победа NOT PROVEN.
[Точная область доказательств E607](../reports/matrixgame-e2e607-ui-context.md).

E608 закрывает совместимость production UI context с полным Windows-боем:
original campaign/navigation/Gov/snapshot/FRun/Init/Draw/natural defeat/return/
reward/history/cleanup PASS. Это новая интеграционная проверка E607-owner,
не повтор прежнего fixture с ручными UI owners. Native HID → Rangers message
loop, общая native entry и отдельная integrated NRO остаются следующими gates;
победа NOT PROVEN, native GPU runtime NOT RUN. Сохранения не записывались.
[Цепочка и машинный аудит E608](../reports/matrixgame-e2e608-ui-campaign.md).

E609: UI input queue и opt-in EngineSink route реализованы; host sanitizers,
ARM64/QEMU, actual Windows GI mouse dispatch/fallback/cleanup и SDK compile/
link PASS. Постоянный native caller ещё не включает этот target. Следующие
обязательные зависимости: real window polling и original GI dispatch, общий
async-key/cursor source, отмена controls на FRun handoff, controller/text
mapping, затем общая native entry и отдельная NRO. Старые большие бои не
повторялись; Switch HID/GPU runtime NOT RUN.
[Точные границы E609](../reports/matrixgame-e2e609-rangers-input.md).

E610: UI polling/dispatch owner, общий async-key/cursor source, real warp,
FRun suspend/resume, physical-held gates и reset при смене UI owners реализованы.
Production user directory направлен в отдельный user_root/config с RAII restore.
Host ASan/UBSan/LSan, ARM64/QEMU, Windows original GI/imports и devkitA64
compile/link PASS; runtime граница HID сценарная. Теперь требуется общая native
campaign/Gov entry и отдельная NRO, затем пользовательская аппаратная проверка.
WM_CHAR/OSK остаётся открытым; победа NOT PROVEN. Прежние NRO сохранены.
[Доказательства E610 и оставшиеся компоненты](../reports/matrixgame-e2e610-native-ui-pump.md).
