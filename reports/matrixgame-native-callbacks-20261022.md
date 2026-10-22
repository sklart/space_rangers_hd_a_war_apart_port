# MatrixGame: оригинальные skinned LoadSpecial → BeforeDraw → Draw

22.10.2026, локальный master, без Switch. База: c1fc063.
Код: `34e84ff816b510e796630d17012a1cf9074e1810`. Без push.

## Новая работающая граница

В CPU overlay добавлена **настоящая структура SSkin из pinned VectorObject.hpp**,
без фиктивного d3d9.h. Теперь исполняются исходные методы:

`NativeInitRuntime → Cache::Get(VO) → LoadSpecial(OLF_SIMPLE / OLF_AUTO /
OLF_MULTIMATERIAL_ONLY) → назначение SSkin → BeforeDraw → DrawBegin → Draw`.

LoadSpecial сохраняет исходные ветви NO_TEX и MULTIMATERIAL_ONLY, включая пропуск
назначения skin у одноматериальной модели. В остальных режимах исходный цикл
вызывает SKIN_GET для каждого surface. Scoped provider возвращает shared owner;
VO удерживает его вместе с исходным raw SSkin field. Ошибка material callback
вызывает Unload и повторно выбрасывается: модель не остаётся «загруженной»
с незавершёнными skins. Неверный/непривязанный provider отвергается явно.

BeforeDraw — исходный preload loop, DX_Prepare и reminder Use. Draw сохраняет
исходный порядок surfaces/passes/unions, default-skin fallback и callbacks
SetupStages/SetupTex/SetupClear. Изменения portable boundary:

- stack alloca заменён vector<bool>; release-путь, без SHOW_PICK_OBJECTS;
- D3D indexed call заменён типизированной native emission через ранее проверенный
  VectorObjectRange, сохраняя обе ветви m_IBase;
- DrawBegin сбрасывает original BigVB/BigIB selection и передаёт VO_FVF native
  sink, который принимает только реальный поддерживаемый layout;
- добавлены проверки frame/callbacks; неудачный Select теперь даёт исключение,
  а не пустую «успешную» выдачу.

Это новые **оригинальные методы в выбранном overlay**, не compile-pass всей TU
VectorObject.cpp и не эквивалент всех методов класса.

## Реальные callbacks и GPU-связь

NativeSkinPipeline создаёт владеющий SSkin с действительными preload/stage/
texture/clear callbacks. Они получают NativeSkin resources и формируют полный
поддерживаемый native draw state; callback counters увеличиваются в самих
callbacks. При Clear каталога raw поля моделей не превращаются в dangling
pointers: сохранённые owners живут до Unload, invalid resources дают исключение.

Prepare формирует CPU batch и проверяет реально привязанные vertex/index buffers.
**Это подготовка GPU-команд, не host-отрисовка.** В devkitA64 слинкована следующая
связка: `NativeDekoSession::LoadSkinnedObject → DrawSkinned → pipeline.Prepare
→ SetTextureAsset → DekoBufferRenderer::Draw → настоящий DrawFrame/readback`.
Entry MatrixGameNativeCallbackDraw offline не вызывался.

Поддерживается **явный native ordinal/base shader contract**, а не полная
эквивалентность исходного SMatrixSkin::Prepare. Side/gloss/mask/back, burn/object
user_param и projected shadow отклоняются явно. Исходный проход не подменяется
фиктивным результатом боя, пустым renderer или успешным no-op.

## Проверки

- **Host ASan/UBSan runtime PASS**, включая leak detection.
- **ARM64/QEMU runtime PASS**, тот же новый сценарий.
- **devkitA64 compile/link PASS**, strong undefined = 0.
- **GPU/Horizon execution HARDWARE_PENDING**; новая NRO не выпускалась.

Два lifecycle-цикла проверяют SIMPLE/AUTO/MULTIMATERIAL_ONLY, default fallback,
preload и последовательность stage/texture/clear, два surface с общей текстурой,
неверный frame, отсутствующий второй материал, cleanup после частичной загрузки,
Clear каталога при живой модели и последующую перезагрузку. GNU wrap _Znwm
вызвал **настоящий bad_alloc внутри второго skin callback**, когда первый уже
назначен: по два отказа/успешных retry на каждой архитектуре.

Вход — read-only настоящий wood05.vo:
`04497dcc94ede26f183d847a4dd094d3ba05bc825d09fcfa73fa36bfae80981e`.
Derived VO в отдельной папке теста меняют surfs/texs на контролируемую PNG;
multi-fixture добавляет пустую вторую material surface, не новую геометрию.
PNG fixture не является игровой текстурой:
`d81cd93a0fccbc517719f9cb6563f6837327dc32a412f59238a95699831ba210`.

Первый host assert ошибочно ожидал 480 индексов из прошлого snapshot общего
выделенного index buffer. Независимый VectorGeometry STRG-reader подтвердил
**240 индексов**, и original Draw выдаёт те же 240, surface/minimum/vertex count.
Игровой код под ошибочную константу не менялся. Raw FAIL сохранён в evidence;
также исправлена трактовка snapshot в предыдущем frame-отчёте. Generic driver
дополнен подсчётом нового формата allocation-fault записи; только финальный
короткий runtime повторён для machine-readable counter, без пересборки.

Финальный SDK ELF:
`build/matrixgame/native-callbacks/sdk/cmake/matrixgame-native-callbacks-probe.elf`.
SHA-256 `fec2af1968747704b3f46d2ceb231c7ec0ae6a8276faac70c543ebb6182ce70b`.
Build ID `de4399e330ed9a9cfdc3c8f47fee532a218227f5`.
Generated effective sources трёх сборок совпали. build_dirty=true: сборки до
коммита; нормализованные изменённые файлы сверены с его Git blobs. GPU TU только
SDK, allocation wrapper только host/Linux ARM64. Следующий коммит — docs.
[Машинное свидетельство](matrixgame-native-callbacks-20261022-evidence.json).

## Что остаётся

Полные original material combiners, CVectorObjectAnim/shadows и полный
VectorObject.hpp/.cpp остаются зависимыми от D3D. L3GInitAsDLL требует исходный
borrowed device/window contract: WndProc, caps/gamma и последующее video state.
Offscreen deko-сессия ещё не присоединена к CGame::Init. Init, CFormMatrixGame,
RunGameLoop и настоящий бой **не выполнялись**. Full compile inventory не повторяли.
Rollback доказан в фазе material callbacks, не для произвольного отказа в ранней
геометрической части legacy LoadSpecial. GPU allocation failure/reentry и image
parity требуют аппаратного исполнения.

Воспроизведение: CMake source port/matrixgame/native-engine, target
matrixgame-native-callbacks-probe; host RelWithDebInfo и ASan/UBSan flags.
`tests/probe_matrixgame_cached_frame.py --skin-callbacks --binary <binary>
--model <wood05.vo> --texture <plain.png> --output <fresh-build-dir>`;
ARM64 добавляет --qemu. SDK: `tools/matrixgame_init_sdk.py --skin-callbacks
--output <fresh-build-dir>`. Paths/flags/hashes в evidence; логи в
build/matrixgame/native-callbacks/. Старые standalone suites не повторялись.

Сейвы, ресурсы, upstream/gitlink и основная NRO не менялись: совпали SHA всех
13 защищённых файлов, включая оба аппаратных ZIP и G:/pack.zip. Для Switch
остаётся [прежний вечерний сценарий](../docs/e2e517-evening-switch-check.md).
Новые callbacks не входят в замороженные NRO. Push не выполнялся.
