# E2E-5.19 — интеграция оригинального DLL bootstrap

Актуальное продолжение: **полная оригинальная `DeviceState.cpp` переведена
из COMPILE_FAIL в COMPILE_PASS и исполняется внутри существующего device owner**.
Windows SDK readback подтверждает восстановление всех 103 render + 13×8 sampler
значений после `S3D_Default`, Detach и частичного отказа. Коммит
`1d5b8c851a6e3d77c32fa604dd545a1054195bf6`, актуальные хэши —
`matrixgame-e2e519-device-state-evidence.json`; подробности в последнем разделе.
Этап S3D_Default (`8fda1d4`, defaults evidence) сохранён ниже как история.
Предыдущий type-boundary этап (`35ac1ab`) сохранён ниже как история.
Ранее полная `MatrixSampleStateManager.cpp` переведена из COMPILE_FAIL в PASS
и исполняется в bootstrap (`246c6ef`, sampler evidence сохранён как история).
Ниже сохранён исторический снимок первого DLL-этапа: его экспериментальные
бинарники в общем build directory заменены новым кандидатом. Замороженные
основная NRO/ELF и аппаратные ZIP этим не затронуты.

Дата: 23.10.2026. Ветка `master`, исходный SHA
`0e81c2f45762d6088b53df1b4a54899ccd1d715b`.
Коммит реализации: `ba7d797b5490b113c56672210e1346775d7fee9b`.
Push и fetch не выполнялись; локальная ссылка `origin/master` остаётся
`df48ae6eb77eff72cccedf9a23e2264fc0e28cff` и не является проверкой сервера.

**PARTIAL_ENGINE_INTEGRATION.** Новое достижение: оригинальная стадия
`L3GInitAsDLL`, включая настоящий оконный hook и `GetDeviceCaps`, исполняется
в существующем владельце cache/VO/material на Windows x86. Исполняется также
оригинальная маршрутизация `L3G_WndProc → processInput → CForm`.
Полный `CGame::Init`, карта, `CFormMatrixGame` и бой пока не запущены.
Это не принятие полного запуска MatrixGame и не доказательство GPU draw.

## Точная цепочка и граница

Pinned upstream: `218115a72fa9c5d43269f4ca84a8596a0bcb82f1`, без изменений.
Строки ниже относятся к нему, а не к сгенерированным частичным cpp.

| Узел исходной цепочки | Оригинальный источник | Фактический статус |
|---|---|---|
| `GetRobotInterface → Run` | `MatrixGame/src/MatrixGameDll.cpp:96`, `:75` | NOT RUN: полный DLL entry не скомпонован в native driver |
| `CGame::Init → random::seed → static_init` | `MatrixGame/src/MatrixGame.cpp:172`, `:138` | COMPILE_BLOCKED: полный `MatrixGame.cpp`; Init не вызывался |
| `CMain::BaseInit`, `CVectorObject::StaticInit`, shadow/map/interface static init | `MatrixGame.cpp:140–167` | Полная последовательность NOT RUN; принятые ранее проверки отдельных компонентов не повторялись |
| Cache и ресурсы перед device boundary | `MatrixGame.cpp:274`, `MatrixLib/3G/Cache.cpp`, `VectorObject.cpp`, `MatrixSkinManager.cpp` | EXECUTED через существующий NativeInitRuntime: cache → реальная VO → SSkin/material; это проверка prerequisites, а не тело полного CGame Init |
| `L3GInitAsDLL` | `MatrixGame.cpp:297`, `MatrixLib/3G/3g.cpp:346` | **EXECUTED на Windows SDK/D3D9 NULLREF**, новая стадия текущего этапа |
| `L3G_WndProc → processInput → CForm` | `3g.cpp:587`, `:524`, `Form.cpp` | **EXECUTED**: по одному key/mouse сообщению и ровно двум явно проверяемым system events |
| Render pipeline, config/device states, `CMatrixMapLogic`, загрузка/`PrepareMap` | `MatrixGame.cpp:299–365` | RUNTIME_BLOCKED: эти prerequisites ещё не портированы; вызов из CGame не достигнут |
| `CFormMatrixGame → RunGameLoop → L3GRun` | `MatrixGameDll.cpp:81–83`, `MatrixGame.cpp:778`, `3g.cpp:439` | COMPILE_BLOCKED / NOT RUN |
| Новый путь на физической Switch | — | HARDWARE_PENDING |

Первая настоящая ошибка компиляции полного `MatrixGame.cpp` остаётся:

```text
MatrixLib/3G/VectorObject.hpp:8:10: fatal error: d3d9.h: No such file or directory
```

У полной `MatrixLib/3G/3g.cpp` первая ошибка —
`MatrixLib/3G/Helper.hpp:9:10: fatal error: windows.h: No such file or directory`.
Точные логи и строки единиц находятся в evidence JSON / `build/matrixgame/e2e519/{baseline,final}`.
Стек исполнения полного `CGame::Init` отсутствует: его предусловия не выполнены,
и вызов не подменялся частичным телом с успешным возвратом.

## Что изменено в интеграции

`tools/matrixgame_platform_3g_overrides.py` извлекает из проверенного `3g.cpp`
исходные `L3GInitAsDLL`, `processInput` и `L3G_WndProc`.
Оконные/device операции перенаправлены в типизированные `Window` и `Device`.
Имена двух функций изменены пространством имён, алгоритм доставки событий сохранён.
`3GPlatformNative.cpp` — **выборка исходных функций, а не COMPILE PASS полной TU**.
SHA-256 выборки: `f478761485b99d626dedb43c687b759472a8ae81310912a964e8a04ef817fd1f`.

CPU-prefix и device-фаза используют один `platform3g::State` внутри прежнего
`NativeInitRuntime`; второй cache/device manager не создан. Порядок:
buffer context/scope → CacheInit → CPU binding → AttachDevice → hook → caps →
gamma при поддержке → доступная texture memory. Существующий deko session
использует тот же runtime; перед уничтожением renderer добавлен DetachDevice.

Заимствованные HWND и D3D9 device не уничтожаются и не Release-ятся runtime.
Ошибка согласования снимает hook и восстановленные gamma bindings,
возвращая прежнюю CPU/cache identity. Полный Deinit затем освобождает cache
до buffer context. Skin owners и CPU submission leases уничтожаются до cache;
после Deinit старый MaterialTexture отвергается, retained texture bytes равны нулю.

Windows backend использует настоящий SDK `d3d9.h` в отдельной TU. SDK-типы
не смешиваются с portable CPU `D3DMATRIX`/DirectXMath. Сохраняется именно
результат **предварительного GetWindowLongPtrW**, как в оригинале; возврат
SetWindowLongPtrW на этой системе оказался другим. Точное восстановление
исходного WndProc проверено после Detach/Deinit и отказа caps negotiation.

Осмысленные ограничения/отклонения:

- Указатели HWND/WndProc сохраняются без исходного сужения до DWORD.
- Gamma сохраняется/восстанавливается лишь для реально существующих swapchain,
  вместо восстановления непрочитанного `g_StoreRamp1`. NULLREF сообщил 0 swapchain;
  **gamma runtime branch не проверена**.
- `DO_SMART_COLOROPS` явно отвергается компилятором; EXE-owned window mode,
  `S3D_Default`, полный render/device state и `L3GRun` не реализованы этой выборкой.
- На Linux/QEMU/Switch driver выбрасывает `Unsupported` при отсутствии native
  window provider. Для deko не выдаются выдуманные legacy caps или RAM вместо VRAM.
- Никакой device reset/lost-device, physical GPU parity или полной Windows x86
  CGame parity не заявляется. NULLREF query runtime не означает draw runtime.

## Проверки кандидата первого DLL-этапа (исторический снимок)

| Проверка | Результат и предел доказательства |
|---|---|
| Windows x86, настоящий HWND + D3D9 NULLREF | PASS, exit 0; два входа, GetDeviceCaps, Win32 сообщения, точный hook restore, Detach/reattach, отказ caps после установки hook, cleanup/reentry |
| Host Linux GCC15, ASan/UBSan + leak detection | PASS, exit 0; реальные cache/VO/material операции, явный unsupported window, очистка и повторный вход; положительная GPU-фаза не выполнялась |
| Linux ARM64 GCC15 / QEMU | PASS, exit 0; тот же CPU/отказной путь; это не Switch runtime |
| devkitA64 GCC16.1/libnx4.12 | COMPILE/LINK PASS общего driver; undefined strong = 0; NRO не упаковывалась и на Switch не запускалась |
| Изменённый NativeDekoSession | отдельный адресный SDK compile/link существующего frame target PASS; GPU runtime NOT RUN |
| Регрессия затронутого cache/CPU owner | PASS, включая foreign-owner rejection и 2 настоящих calloc failure cases; прежние CList/BigVB/Road/VO suites не повторялись |
| Полный compile inventory | baseline **14 PASS / 92 FAIL**, final **14 PASS / 92 FAIL**, одинаковые 106 оригинальных единиц; сменивших статус TU **нет** |

Сняты window-hook/device-query блокеры выбранной DLL-стадии **для Windows backend**.
Полный первый header blocker MatrixGame не снят. Новая стадия исполняется в
интеграции с прежним resource owner; доля компилируемых full-engine TU не выросла.

Оба inventory использовали тот же host build/cache и
`-fsanitize=address,undefined -fno-omit-frame-pointer`, RelWithDebInfo.
Prepared engine manifest одинаков:
`177fdcddac185fab73fb4f28083854f8ffc3f599896bcdffbb560d36cbc49d09`.
Отдельный снимок compile commands в момент baseline не архивировался;
окончательные 106 recipes сохранены в evidence. Flags между двумя прогонами
не менялись. Все generated native-core outputs побайтно совпадают между
host, Linux ARM64, SDK и Windows; platform_win32.cpp подключается только на Windows,
а положительный/отказной путь driver различается по `_WIN32`.

SHA-256 бинарников и все source/manifest hashes приведены в
`matrixgame-e2e519-engine-bootstrap-evidence.json`. SDK ELF общего driver:
`e37c863a41bac9fb48c263a8a2816f940be99e38f0c2cbc64fd8a2626c613d8f`.

## Воспроизведение

Из корня репозитория в WSL (аргументы — read-only существующие ресурсы):

```sh
cmake -S port/matrixgame/native-engine -B build/matrixgame/e2e519/host \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer'
cmake --build build/matrixgame/e2e519/host --target matrixgame-native-bootstrap -j4
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  build/matrixgame/e2e519/host/matrixgame-native-bootstrap \
  build/e2e515/big-buffer-models/model-1.vo \
  build/mg-merge-host/core/texture-request-fixtures/assets/plain.png
```

ARM64 использует этот же target, `aarch64-linux-gnu-g++-15` и уже проверенные
static zlib/png/jpeg из E2E-5.18; точные include/library пути и flags в evidence.
Запуск: `qemu-aarch64 -L /usr/aarch64-linux-gnu <ARM64-driver> <VO> <PNG>`.
В Windows существующий `windows/engine` — VS2022 Win32 Release, `/EHsc`,
единый CRT `/MT` для engine/zlib/png/jpeg (для старого libpng включена
`CMAKE_POLICY_DEFAULT_CMP0091=NEW`). Target тот же, вывод `windows-runtime.log`.
SDK: `python tools/matrixgame_init_sdk.py --bootstrap --output <fresh-build-dir>`;
для данного уже созданного output доступен `--resume`. И `DEVKITPRO`, и `DEVKITA64`
задаются SDK script; при прямом CMake build нужны те же переменные.

## Сохранность и следующий шаг

Все **14 защищённых файлов** сохранили SHA-256 до/после: пользовательские
saves/config/log, provenance, `G:/pack.zip`, два VO, PNG и оба hardware ZIP.
Upstream clean/pin и gitlink сохранены. Основная E2E-5.16 и старый GPU комплект
не пересобирались; новые функции в них не входят. Коммерческие файлы и сейвы
не добавлялись в Git. Аппаратные статусы остаются HARDWARE_PENDING;
сценарий основной игры — `docs/e2e517-evening-switch-check.md`.

Не более трёх следующих приоритетов по исходному call graph:

1. Закрыть native type dependencies полной `MatrixGame.cpp`: первый include
   `VectorObject.hpp:8` и обязательные shadow/map типы, сохраняя оригинальные
   классы и static_init. Выборку функций не выдавать за full TU PASS.
2. После реального closure static_init связать CGame resource/config prerequisites,
   `CacheInit` и новый device boundary в исходном порядке; не создавать второй
   cache поверх уже активного NativeInitRuntime и не обходить проверки Init.
3. Реализовать подтверждаемый deko platform/device-state contract для
   `S3D_Default`/render pipeline. До него Keep Unsupported для неизвестных caps,
   затем отдельная физическая GPU проверка; `CFormMatrixGame/RunGameLoop` ещё pending.

## Продолжение: полный sampler-manager и действующие device calls

База продолжения: `2a3b4000721ad995c22a6b11d347e83d3c20269f`.
Реализация: `246c6efedd1ed485d0803c63a60ec6f6be023db5`, локальный `master`,
без push/fetch. Полная исходная единица
`MatrixGame/src/MatrixSampleStateManager.cpp` сохранена целиком, включая
конструктор, деструктор, `ApplySettings:16`, `SetState:20`,
`EligibleForAnisotropy:31` и единственный глобальный `g_Sampler:35`.
Это **не** новая выборка переименованных CPU-функций.

`tools/matrixgame_engine_sampler_overrides.py` заменяет Windows/D3D типы
на существующий typed sampler register и signed 32-bit DeviceResult.
Исходные ветвления и порядок двух вызовов при AF сохранены:
SetSamplerState(filter, ANISOTROPIC), проверка результата, затем MAXANISOTROPY.
Глобальные device/caps обращения направлены к тому же negotiated device,
которым владеет NativeInitRuntime; новый manager/session не создавался.
Windows provider вызывает настоящие `IDirect3DDevice9::SetSamplerState` и
`GetSamplerState`. Unsupported backend не возвращает фиктивный успех.

Текущая исполненная цепь driver:

```text
NativeInitRuntime.Begin -> Cache/VO/SSkin -> AttachDevice -> реальные caps
  -> ApplySamplerSettings -> оригинальный g_Sampler.ApplySettings
  -> оригинальный g_Sampler.SetState/EligibleForAnisotropy
  -> Device.SetSamplerState -> настоящий D3D9 Set/GetSamplerState
  -> Detach/отказ backend/повторный Attach -> Deinit
```

`ApplySamplerSettings` вызывает тот же исходный метод, что строка 323
`CGame::Init`; это не вызов всего CGame Init. На Windows запрос AF=64
ограничился реальным caps=16; чтение MAG/MIN/MIP и MAXANISOTROPY из SDK device
подтвердило значения. Второй вход применил AF=0 и вернул linear filters.
AddressU прошёл через неизменённую ветвь без AF. После Deinit логическое
`g_Sampler.m_AFDegree` равно 0, SetState без negotiated device отвергается.
Само borrowed device, как в оригинале, остаётся живым; общего восстановления
всех его sampler registers не добавлялось.

Проверен адресный отказ backend sampler operation после получения настоящих
caps и установки настоящего hook: операция выбрасывает Unsupported, hook
снимается, затем тот же owner повторно подключает исходный device и успешно
применяет состояние. Тестовый отказ не создаёт фиктивных capabilities/HRESULT.
**Отрицательная HRESULT-ветвь не проверена**: NULLREF вернул успех даже для
некорректного enum; такой вызов не считается доказательством обработки ошибки.

Адресная компиляция на одном recipe (ASan/UBSan, те же include/flags):

| Полная TU | Сохранённое старое source view | Кандидат |
|---|---|---|
| `MatrixSampleStateManager.cpp` | COMPILE_FAIL, `windows.h` в строке 6 | **COMPILE_PASS**, object SHA `c2aa72ea70c8d472d93ff35ae4bc1f66e1324b7bfd6e3431130d89fb06198f45` |
| `MatrixGame.cpp` | прежний full inventory | Адресно перепроверена: COMPILE_FAIL, `VectorObject.hpp:8:10: d3d9.h` |

Full sampler source SHA после overlay:
`ebbb4acbcb1c2ff9fd80919a9eb31d393b6e88ebdf747bef03416523e3f01806`.
Полный inventory 106 единиц повторно не запускался. Числа 14/92 выше относятся
к предыдущему снимку; новое точное доказательство — **одна полная TU FAIL→PASS**.
Суммарный актуальный full inventory заново не измерялся.

Windows runtime PASS; host ASan/UBSan и ARM64/QEMU PASS в доступной им
CPU/Unsupported-фазе; devkitA64 bootstrap и существующий deko session target
COMPILE/LINK PASS, undefined strong=0. Сгенерированные core sources совпадают
по всем output hashes на четырёх платформах. Тот же driver дополнен проверкой
sampler; отдельного исполняемого smoke не создано. Старые большие suites не
повторялись. Сборки сделаны до коммита из dirty worktree; соответствие коммиту
подтверждено source hashes, embedded Git SHA в этих экспериментальных бинарниках нет.

Воспроизведение — прежний target `matrixgame-native-bootstrap` и те же VO/PNG.
Новый sampler helper не пишет файлы. Точечный audit:

```sh
python3 tools/matrixgame_engine_compile_audit.py \
  --build-root build/matrixgame/e2e519/host --output <fresh-build-dir> \
  --target matrixgame-native-engine --source MatrixSampleStateManager.cpp --jobs 1
```

Для сравнения старого source добавляется
`--source-view build/matrixgame/e2e519/sampler-before/source`; валидные результаты
этого этапа — `sampler-verified-before` и `sampler-verified-after`.
Новый SDK ELF SHA:
`064907599d6508ac357b0c52fa40d712b05bebf4806740d22f1e98a1fab76fbe`.
Точные recipes, логи, остальные хэши и ограничения — в отдельном sampler evidence JSON.

Повторная проверка всех 14 защищённых SHA прошла; upstream clean, pinned commit
и gitlink прежние. Игра и сохранения не изменены. CGame Init, S3D_Default,
планетарная форма, draw и физическая Switch остаются NOT RUN/HARDWARE_PENDING.
Общая цель запуска MatrixGame остаётся открытой; три приоритета выше сохраняются.

## Продолжение: общая VO ABI и полные декларации без лишнего SDK

База: `0ac922137ae46a87e37569df37468a2a9e5a1fc1`.
Коммит реализации: `35ac1ab6da1fbb00fa05e31762cb396b3083eadc`, без push/fetch.

Первый барьер полной `MatrixGame.cpp` действительно был в `VectorObject.hpp`,
но простая подстановка старого CPU subset оставила бы различные определения
одного CVectorObject в engine и cache. Теперь:

- `VectorObjectShared.hpp` содержит одно полное объявление CVectorObject из
  уже работающей native реализации. CPU header стал facade; полный engine
  header использует то же объявление и сохраняет весь исходный animator/group tail.
  Общий macro guard защищает от повторного определения при двух generated views.
- Полный `CVectorObjectAnim` сохранил `m_Lights`, `m_LightsCnt`, InitLights,
  DrawLights и shadow API. Отдельный NativeVectorObjectAnim не подставлен вместо него.
  SColorInterval/SLightData перенесены в VectorObject.cpp без изменения содержимого;
  поиск по pinned исходникам показал, что используются они только там.
- D3DControl/ShadowProj/CBillboard includes перенесены из VO header в cpp.
  Все прочие тела VectorObject.cpp совпадают со старым source view побайтно.
- CBillboard header содержит настоящие SDK pointers как неполные типы.
  Release pointer types и debug D3DResource wrappers сохранены; методы SDK
  не выдуманы. BeforeDraw с прежним `m_SortableTex->Preload()` перенесён в cpp;
  прочие тела CBillboard.cpp не изменены. Полная GPU-реализация не портирована.
- Billboard/sound declarations используют единый math_types, а не дублирующий
  Math3D из второго generated view. Нужная inline SetAngle операция RotationZ
  реализована реальной DirectXMath XMMatrixRotationZ; проверен поворот на pi/2.
- Windows/D3D includes Helper.hpp находятся внутри того же debug guard,
  что и CHelper. Release не тянет SDK только ради отсутствующего debug-класса.
  Исходные debug declarations не вырезались. Native VO по-прежнему release-only.

Исправлена конкретно обнаруженная ошибка генератора на Windows: чтение UTF-8
cpp/hpp через системную CP1251. Теперь чтение явно UTF-8, включая MatrixGame.cpp.
Это подготовка исходников MatrixGame; основная игра и её рендер не изменялись.
**Все 234 файла prepared engine tree проверены по фактическим SHA-256 и совпали
побайтно на host, ARM64/QEMU, devkitA64 и Windows**, как и generated core outputs.
Это сравнение содержимого всех файлов, а не разрешение различий по именам.

Тот же bootstrap одновременно включает полный VO/billboard header и API cache.
На x86/x64/ARM64 проверены сохранённые light pointer/count в полном animator,
24-byte SBillboardVertex и отсутствие повторного определения CVectorObject.
Его прежний cache загружает настоящую VO через это же объявление класса;
реальные Windows DLL/sampler/CForm вызовы, ошибки, очистка и повторный вход PASS.
Host ASan/UBSan и ARM64/QEMU PASS в CPU/Unsupported scope. SDK bootstrap и
существующий deko session target COMPILE/LINK PASS, undefined strong=0.

Новая адресная проверка полной MatrixGame.cpp остановилась на:

```text
MatrixLib/3G/3g.hpp:16:10: fatal error: d3d9.h: No such file or directory
```

Цепочка: MatrixMap → MatrixSide → MatrixFlyer → MatrixProgressBar → Texture → 3g.
Здесь нужны действительные device/caps/presentation и state APIs, а не только
векторы или указатели. В процессе были адресно сняты промежуточные лишние
includes в MatrixSoundManager и debug-only Helper. Тело MatrixGame.cpp не менялось
относительно предыдущего native view: SHA остаётся
`19c1485c608202ff088c4b37a62ca2508ee9b064f655017799e6bae27fae672b`.

**Граница исполнения не расширилась до новых игровых методов:** выполняется
прежняя цепь Cache/VO/SSkin → DLL bootstrap → sampler → CForm callbacks.
Полные VectorObject.cpp, CBillboard.cpp, CVectorObjectAnim methods,
CGame::Init/static_init, S3D_Default и бой не исполнялись; full TU/link PASS им
не приписывается. Продвинуты реальные декларации и совместимость типов на пути
к Init. Нового standalone CPU/GPU target не создано, inventory 106 TU не повторялся.

Точные исходные/генерируемые хэши, сохранённые тела, actual-source compile error,
проверки и бинарники — в `matrixgame-e2e519-types-boundary-evidence.json`.
SDK ELF SHA: `3bc17a4b58d88183e1d6de50fdf969c6795928ea4b23ea4ba2aef4072535f633`.
В последующем documentation commit два Python комментария/docstring уточнены;
их executable AST совпадает с проверенным implementation commit.

Все 14 защищённых файлов снова совпали с исходными SHA. Upstream/pin/gitlink,
сейвы, ресурсы и оба hardware ZIP сохранены, NRO не выпускалась.
Основная E2E-5.16 и GPU hardware scenario остаются отдельными HARDWARE_PENDING.
Общая цель активна. Следующие три узла:

1. Полный `3g.hpp` contract: реальные caps/presentation, texture-stage/render-state
   и transform операции через общий device owner. Не заменять его fake D3D9 header.
2. Texture/DeviceState и обязательные static_init/map/render-pipeline зависимости,
   после которых возможен настоящий CGame::Init в исходном порядке.
3. CFormMatrixGame/RunGameLoop и физический deko draw после выполнения prerequisites;
   до этого не заявлять работающий планетарный бой.

## Продолжение: оригинальный S3D_Default и сохранность borrowed device state

База: `f0a1d3bd16de7035a2251a1c38602a8574d224c2`.
Коммит реализации: `8fda1d4b7d5b1074e95dfc7ba005697f4b51b9ec`, локально, без push.

**Новая исполненная исходная стадия:** полное тело `S3D_Default`
из `MatrixLib/3G/3g.cpp:656`, которое оригинальный
`CGame::ApplyVideoParams` вызывает в `MatrixGame.cpp:620`.
`tools/matrixgame_device_defaults_overrides.py` извлекает именно эту функцию
из pinned source и направляет вызовы в тот же typed Device. Имя исходной
функции сохранено; нового самостоятельного test executable не создано.
Это выбранная функция, **не compile PASS полной 3g.cpp**.

Сохранены исходный порядок и все операции: identity world matrix;
восемь итераций sampler/texture/TSS; двенадцать render registers.
Настройки AF применяет ранее перенесённый полный g_Sampler.
Преобразование LOD bias из float в DWORD заменено на std::bit_cast без aliasing UB.
Identity вычисляет настоящая DirectXMath XMMatrixIdentity. Native result checking
явно выбрасывает DeviceError при неуспехе; это строже исходного ASSERT_OFF режима,
успешная последовательность не сокращена.

Device contract расширен world transform, render state, ClearTexture и TSS.
Windows provider вызывает настоящие Set/Get методы IDirect3DDevice9.
Сериализованные IDs проверены настоящим SDK; MeshRasterState не стал молча
принимать Lighting/ShadeMode/CullMode — отсутствующая shader поддержка всё ещё
отвергается. Deko provider для этих legacy операций не придумывался.

NativeInitRuntime теперь считает устройство готовым только после получения
owned state lease. Lease захватывается после фактического caps negotiation,
до применения sampler/defaults; настройки sampler обязательны до S3D_Default.
Detach/Deinit сначала восстанавливают состояние и освобождают lease, затем
возвращают gamma/window hook и освобождают cache/buffer context в прежнем порядке.
Результат Restore хранится как реальный signed device result; failure не выдаётся
за подтверждённое восстановление. CPU cache может пережить отказ GPU-фазы для
повторного входа, но GPU/default readiness сбрасывается.

### Выявленный реальный дефект restore и исправление

Усиленный readback показал: NULLREF вернул success из StateBlock::Apply,
однако world.x остался 0 вместо 137.5, blend — 0 вместо 1.
Лог сохранён как `defaults-stateblock-failed-readback.log` и включён в evidence.
Прежняя проверка identity/default blend могла бы этого не заметить.

State lease поэтому дополнительно сохраняет реальные GetTransform/GetRenderState/
GetSamplerState/GetTextureStageState и COM-ссылки GetTexture. Restore явно
применяет сохранённые значения через Set API; StateBlock оставлен дополнительным
механизмом, его HRESULT не считается достаточным доказательством.
Точная поддержанная область: world, 14 render registers, 8 sampler registers
на стадиях 0..7, TEXCOORDINDEX и восемь texture bindings. Операции вне этой
области отвергаются Unsupported. Это не универсальная реализация всех D3D9 states.

Windows helper создал настоящую managed 2x2 SDK texture и привязал её к stage 0.
Перед захватом заданы world translation (137.5,-11), blend enabled,
AddressU clamp и texcoord=1 на всех восьми стадиях. После S3D_Default readback
подтвердил identity, ожидаемые AF/linear filters, wrap, LOD=-1 bits, отсутствие
texture bindings, texcoord=0 и все двенадцать render values. После Detach
восстановлены недефолтные caller values и исходная ненулевая texture binding.
Owned state leases после освобождения равны нулю.

Адресные ошибки: отказ render backend после настоящего world и восьми
sampler/texture-stage записей; отказ CaptureState после настоящих caps и hook.
Оба отказа очищают частичный GPU state, сохраняют CPU owner и допускают новый
Attach/ApplySampler/S3D_Default. Restore на частичном отказе также проверен
реальным readback; фиктивные HRESULT/caps в этих fault providers не возвращаются.

| Проверка | Фактический результат |
|---|---|
| Windows x86/D3D9 NULLREF | PASS, exit 0; два входа с AF=16 и AF=0, S3D_Default, state readback, реальные COM leases и восстановление после отказов |
| Host ASan/UBSan + leak detection | PASS в CPU/Unsupported scope; S3D_Default на device не исполнялся |
| ARM64/QEMU | PASS того же CPU/Unsupported scope; не Switch runtime |
| devkitA64 bootstrap | COMPILE/LINK PASS, символ S3D_Default присутствует, undefined strong=0 |
| Существующий deko session link target | COMPILE/LINK PASS, undefined strong=0; GPU runtime NOT RUN |
| Адресный MatrixGame.cpp audit | COMPILE_FAIL: прежний `3g.hpp:16:10: d3d9.h`; общий inventory не повторялся |

Generated S3D_Default SHA:
`929b194a459cef4cd8262827ad5d7ca800cbd5342114d123c08581b2951d7901`;
core outputs проверены по фактическим байтам и совпадают на всех четырёх платформах.
Новый SDK ELF SHA:
`2f737751326dc145d19412318511043fa7f4b1c505426c419952ca578af1733f`.
Команды прежние: target matrixgame-native-bootstrap с VO/PNG и
`tools/matrixgame_init_sdk.py --bootstrap`; точные файлы/логи/хэши — в новом evidence.
Предыдущие экспериментальные бинарники в общих build paths заменены этим кандидатом.

Точная граница теперь: Cache/VO/SSkin → DLL bootstrap/caps/state lease →
оригинальный sampler → **оригинальный S3D_Default с настоящими Windows state calls**
→ CForm callbacks → Restore/Deinit. Сам CGame::Init и ApplyVideoParams не вызваны;
Reset, presentation/viewport, lost-device, полный render pipeline и CFormMatrixGame
пока не реализованы/не исполнены. Взаимодействие state lease с будущим Reset
потребует отдельного решения. State/query runtime NULLREF не означает GPU draw.

Все 14 защищённых SHA снова совпали; upstream clean/pin/gitlink прежние,
основная NRO и hardware ZIP не изменялись, saves/resources не коммитились.
Физическая Switch и deko drawing остаются HARDWARE_PENDING. Общая цель активна.
Следующие три узла: полный 3g.hpp contract для caps/presentation/Reset;
затем обязательные Texture/DeviceState/static_init/map зависимости настоящего
CGame::Init; затем CFormMatrixGame/RunGameLoop и физический draw.

## Продолжение: полная оригинальная DeviceState.cpp в общей цепочке

База этого продолжения: `47ec8065b7d99a0c9a7146cfd7e5bba934830763`.
Реализация: `1d5b8c851a6e3d77c32fa604dd545a1054195bf6`, локальный master,
без push/fetch. Evidence: `matrixgame-e2e519-device-state-evidence.json`.

Это реальная зависимость подготовки карты: исходный
`MatrixGame/src/MatrixMapPrepare.cpp:1954` создаёт CDeviceState, строка 1956
вызывает StoreState, строка 1941 — RestoreState. Сами методы MatrixMapPrepare
и CGame::Init пока НЕ вызваны. Новая интеграция применяется уже сейчас внутри
существующего Windows StateLease: полный оригинальный CDeviceState сохраняет
render/sampler до изменения устройства, восстанавливает при Detach/ошибке.
Самостоятельный executable/probe не добавлен, второй cache/device owner не создан.

### Реальный переход compile и границы адаптации

Оригинальный input DeviceState.cpp SHA:
`f627dbaa15a1e2a75304469ccbb0cea13f0f10a13810bd30d13fbb3b915aefa3`.
Взята полная TU, оба метода и все четыре группы states сохранены; это не
извлечение отдельных функций. Порядок таблиц 103 render и 13 sampler,
циклы восьми sampler stages, lights и material branches сохранены.
Указатель устройства заменён типизированным StoredStateBackend, DWORD/BOOL —
fixed-width storage. Регистры представлены порядковыми ключами исходных таблиц;
Windows backend разрешает их в настоящие SDK constants, не в придуманные caps.

Осмысленные изменения семантики: проверяются реальные результаты backend;
данные инициализированы; null backend запрещён; RestoreState проверяет успешный
захват запрошенных групп. StoreState сбрасывает валидность перезахватываемых
групп до операций, отмечает успех после всего захвата. Частичный отказ не
превращает недополученные данные в готовый снимок.

Адресный baseline старого prepared tree, тем же реальным matrixlib recipe:
**COMPILE_FAIL**, `3g.hpp:16:10: d3d9.h: No such file or directory`.
Финальный кандидат: **COMPILE_PASS**, object SHA
`c84f84e2197f308c7e2784d20cc45c77da907fda12af0840c68f9d595fea60f3`.
Промежуточная ошибка упаковки общего generated header сохранена в evidence:
DeviceState собиралась в bootstrap, но audit target не находил registry header.
Он добавлен в полноценный MatrixLib/3G source view; стабильный include guard
предотвращает повторное объявление одинаковых типов между generated views.
Compile flags не ослаблялись. Полный inventory 106 единиц не повторялся;
старое число 14/92 остаётся историческим, новый итог 106 не вычислялся.

### Исполнение и адресные проверки

| Проверка | Подтверждено |
|---|---|
| Windows x86/D3D9 NULLREF | Полные StoreState/RestoreState для render/sampler внутри общего bootstrap; два входа, SDK readback всех 207 значений |
| Выборочные группы | Restore render не меняет sampler; отдельный BORDERCOLOR сохраняется до явного восстановления |
| Частичный захват | StoreState(ALL) реально читает 207 значений, затем явно отказывает на lights; Restore недозахваченных групп запрещён, recapture успешен |
| Частичный GPU init | Прежний адресный отказ после world/sampler writes восстанавливает все 207 значений, world/TSS/texture binding и ноль owned state leases |
| Host ASan/UBSan/leak detection | PASS CPU/Unsupported scope; restore до capture и после отказа запрещён; положительные SDK state operations не исполнялись |
| ARM64/QEMU | PASS того же CPU/Unsupported scope; не Switch runtime |
| devkitA64 | COMPILE/LINK PASS того же bootstrap, StoreState/RestoreState присутствуют; undefined strong=0; NRO не выпускалась |

Новые дополнительные мутации FILLMODE и sampler-stage-7 BORDERCOLOR проверяют
восстановление за пределами прежних 14 render/8 sampler registers. Сравниваются
все 207 реально прочитанных значений, также сохранены проверки недефолтного world,
COM texture, hooks, cleanup и повторного входа. Никаких успешных HRESULT или
функционирующего D3D9 backend на Linux/Switch не имитируется.

**Ограничение:** lights/material branches компилируются, но backend этих операций
явно Unsupported. Поэтому default StoreState(ALL) ещё НЕ поддержан; проверенное
владение здесь использует исходную маску RENDERSTATES|SAMPLERSTATES. Это не
доказательство готовности map lifecycle, полного GPU state или lost-device/Reset.
NULLREF подтверждает настоящие SDK state calls, физический draw не выполнен.

Generated DeviceState.cpp SHA:
`2a95fafeeb7700d37d6d181c87a4792cf805ce70320afea6789c4d326e3c76ba`.
Все core outputs и 235 prepared engine files сверены по фактическим байтам между
host, ARM64, Windows и SDK. Новый SDK ELF:
`68a6ecdf2001e96c205b26a0a52fc4c032ca81dc096208d98e795731c4b2a796`.
Build dirty во время компиляции отмечен; встроенного Git SHA у bootstrap нет.
Связь с implementation commit устанавливается source hashes/manifest/evidence.
Прежние экспериментальные файлы в общих build paths заменены этим кандидатом.

Команды воспроизведения: существующий target `matrixgame-native-bootstrap`
с `build/e2e515/big-buffer-models/model-1.vo` и
`build/mg-merge-host/core/texture-request-fixtures/assets/plain.png`;
`tools/matrixgame_init_sdk.py --bootstrap --resume --output build/matrixgame/e2e519/sdk`.
Адресный audit: `tools/matrixgame_engine_compile_audit.py --build-root
build/matrixgame/e2e519/host --output <fresh build directory> --target
matrixgame-native-matrixlib-audit --source /DeviceState.cpp --jobs 1`.
ASan/UBSan flags, actual recipe, binary hashes и логи находятся в evidence.

Все 14 защищённых SHA совпали, pinned upstream чист и прежний, saves/resources,
production NRO и hardware ZIP не менялись. Физическая Switch: HARDWARE_PENDING.
Точная новая граница: единый Cache/VO/SSkin owner → DLL/caps → оригинальные
CDeviceState capture → sampler/S3D_Default → CForm callbacks → оригинальные
CDeviceState restore/Deinit/reentry. Полный CGame::Init не запущен; цель активна.

Следующие приоритеты (не более трёх):

1. Общий 3g.hpp contract и настоящие presentation/Reset prerequisites перед
   CGame::ApplyVideoParams. Его прежний первый blocker `3g.hpp:16 d3d9.h`
   не изменился; повторная компиляция CGame в этом продолжении не проводилась.
2. Обязательные Texture/static_init/map owners и неподдержанные группы
   CDeviceState(ALL), только по реальному call graph, затем настоящий CGame::Init.
3. CFormMatrixGame/RunGameLoop после исполнения Init и отдельный физический draw.
