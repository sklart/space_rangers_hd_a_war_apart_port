# MatrixGame: оригинальный animation Takt → skinned Draw

22.10.2026, локальный master, без физической Switch. База 240a474.
Код: `98dc8a0ec584a37c6d00574d06d3eca3e579f4c3`. Push не выполнялся.

## Новый работающий срез

К предыдущему оригинальному VO Draw присоединён исходный frame/timing-путь
CVectorObjectAnim. NativeSkinPipeline::PrepareAnimated вызывает его BeforeDraw
и inline Draw: выбранный анимацией VO frame передаётся в настоящие SSkin callbacks
и исходный surface/union traversal. NativeDekoSession::DrawAnimated передаёт
полученный batch в реальный texture upload, indexed draw и readback deko3d.

`matrixgame_animation_overrides.py` использует уже проверенные pin hashes
VectorObject.cpp/.hpp и генерирует **явный вариант NativeVectorObjectAnim**:

- оригинальные constructor/destructor, Clear/Init для пути без billboard lights;
- исходный frame clock и переходы Takt, NextFrame, FirstFrame и inline selectors;
- исходные GetBound, BeforeDraw и inline Draw;
- не перенесены billboard state, InitLights/DrawLights и light-color часть Takt.
  Ненулевой tex_light отвергается до изменения состояния. Это не невидимое
  отключение запрошенных lights и не fake CBillboard.

Имя варианта отличается от полного CVectorObjectAnim намеренно. Полная TU и
полный класс не объявляются перенесёнными; picking/shadow API в варианте отсутствуют.
Добавлены явная инициализация/reset m_Skin, проверка загруженной VO с анимациями
в Init и guard отрицательного/переполняющего m_Time elapsed в Takt. Исходный
порядок переходов/выбора кадров сохранён; это не новый алгоритм анимации.

VO и default SSkin, как в исходном animator, **заимствованы**. Clear не выгружает
модель и не уменьшает cache refcount. Caller обязан остановить animator/Clear
перед Unload/перезагрузкой VO, уничтожением кэша или default skin. Это не новый
stale-pointer-safe handle API; низкоуровневые setters сохраняют precondition
валидных animation indices. Никакой имитации результатов боя не добавлено.

## Исполнение и проверки

**Host ASan/UBSan runtime PASS**, leak detection включён.
**ARM64/QEMU runtime PASS**, тот же новый сценарий.
**devkitA64 compile/link PASS**, strong undefined symbols = 0.
**GPU/Horizon runtime HARDWARE_PENDING**.

Read-only вход — настоящая Terron VO, model-2.vo, SHA-256
`b7cba8a78faf0c8cf32cf942288d2fdb0d5d21b047d0e9abaa212092c11d9ccd`.
Выбрана реальная animation 0: **41 кадр, период 4100 мс**. Для draw integration
отдельный derived VO меняет только surfs/texs на контролируемую PNG, не геометрию
и не animation records. PNG — fixture, не игровая текстура:
`d81cd93a0fccbc517719f9cb6563f6837327dc32a412f59238a95699831ba210`.

За два lifecycle-цикла проверены:

- исходная строгая граница `m_Time > m_TimeNext`: на точном deadline кадр ещё
  не меняется, после +1 мс выбирается следующий;
- NextFrame, выбор по имени/ID, отсутствие изменения при неизвестном имени,
  non-loop clamp/IsAnimEnd на последнем кадре;
- полный timed loop и исходная особенность: Takt возвращает false, если после
  полного оборота m_Frame снова равен прежнему. Draw от этого не пропускается;
- выбор VO frame 2, его bounds и **четыре draw-группы**, сверенные с независимым
  VectorGeometry STRG-reader; не заявляется визуальная проверка всех 41 кадров;
- default-skin preload, skinned/no-texture paths, Clear/reinit, отказ при
  неинициализированной модели, отрицательном elapsed и запрошенных billboard lights;
- Clear animator до cache teardown, сохранность загруженной VO после Clear,
  освобождение cache resources и повторный вход без ASan/UBSan/leak ошибок.

Новых heap-owning billboard ресурсов здесь нет; прежние allocation-fault и
standalone CVectorObject/CList/BigVB/RoadNetwork suites повторно не запускались.
Первый новый кандидат прошёл без неудачных runtime попыток.

SDK entry MatrixGameNativeAnimationDraw содержит три последовательных
Takt(100)/DrawAnimated через настоящий deko renderer. Offline entry не вызывался;
ELF main предназначен для CPU contract, не готового запуска GPU с hbmenu.
Новая NRO не выпускалась. SDK ELF:
`build/matrixgame/native-animation/sdk/cmake/matrixgame-native-animation-probe.elf`.
SHA-256 `0e294dd63f417bc8a32978335b2ee7646ff517f868a7de51c3b3edd9894a2713`.
Build ID `2245d9b67739339077bfcecb943ba6d6b48eb05c`.

Generated effective sources одинаковы в трёх сборках. build_dirty=true: сборки
выполнены до коммита; LF-normalized изменённые исходники сверены с его blobs.
GPU TU компилируются только SDK; host/QEMU проверяют frame state и draw commands.
Следующий коммит меняет документацию.
[Машинное свидетельство, команды и hashes](matrixgame-native-animation-20261022-evidence.json).

## Следующий технический барьер

Полный CGame::Init пока не вызывается: его static_init требует полного набора
shadow/map/interface классов, а L3GInitAsDLL — Win32 window hooks и borrowed D3D
device/caps/gamma contract. Собственный offscreen deko lifecycle ещё не заменяет
этот вход. Не вырезались static_init или тело CGame::Init ради ложного PASS.

Следующий связанный dependency — полный CVectorObjectAnim/VectorObjectGroup:
billboard InitLights/DrawLights и shadow branches в VectorObject.cpp, затем
legacy material/device consumers MatrixMap/MatrixFormGame. Native ordinal/base
shader не реализует полный side/gloss/mask/back pipeline. Lighting image parity,
GPU allocation/cleanup/reentry, CFormMatrixGame, RunGameLoop и бой остаются pending.
Полный compile inventory прежнего этапа повторно не прогонялся.

Воспроизведение: CMake source port/matrixgame/native-engine, target
matrixgame-native-animation-probe. Host: RelWithDebInfo и ASan/UBSan flags.
`tests/probe_matrixgame_cached_frame.py --animation --binary <binary>
--model <model-2.vo> --texture <plain.png> --output <fresh-build-dir>`.
ARM64 добавляет --qemu; SDK: `tools/matrixgame_init_sdk.py --animation
--output <fresh-build-dir>`. Paths/flags и команды — в evidence, логи —
build/matrixgame/native-animation/.

Все **14 защищённых файлов** сохранили SHA: saves/config/logs, provenance,
оба аппаратных ZIP, G:/pack.zip, исходные VO и PNG. Upstream/gitlink, основная
NRO и ресурсы не изменены. Для следующей Switch-сессии остаётся
[прежний сценарий](../docs/e2e517-evening-switch-check.md); новые методы не входят
в замороженные NRO. Push не выполнялся.
