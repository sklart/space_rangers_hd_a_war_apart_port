# E2E-5.19 — интеграция оригинального DLL bootstrap

Актуальное продолжение24.10: [полный MatrixHint и исправление владения](matrixgame-e2e519-hint.md).
Реальный Build на controlled bitmap воспроизвёл потерю caller-owned texture
при GPU refusal (registry2→3). Временный owner исправляет этот путь;
host ASan/UBSan и ARM64/QEMU проверили retry без роста registry. Полный Hint
компилируется MSVC, devkitA64 prefix compile/link PASS, новых sanitizer
exclusions нет. Full map owner LINK FAIL8 вместо10; успешный Hint GPU/text path
и CGame::Init NOT RUN.

Предыдущее продолжение24.10: [полный исходный модуль миникарты](matrixgame-e2e519-minimap.md).
Original Init allocation/refusal, Clear/reentry и геометрия исполнены в общем
host ASan/UBSan и ARM64/QEMU bootstrap. Полный модуль компилируется MSVC;
настоящий HAL проверил GetRenderTargetData и все пиксели system-memory texture.
devkitA64 prefix compile/link PASS. Full map owner LINK FAIL10 вместо11;
полный minimap GPU path и CGame::Init NOT RUN.

Предыдущее продолжение24.10: [original CTexture loss/reset](matrixgame-e2e519-unmanaged-texture.md).
Оригинальные solid/bitmap texture creation, default/system/default notifications
и все mip pixels исполнены на настоящем HAL. Общий host/ARM64 проверил actual
parent CacheDeinit и owner-state split; devkitA64 prefix compile/link PASS.
Full map owner LINK FAIL11; настоящий Device Reset, minimap и CGame::Init NOT RUN.

Предыдущее продолжение24.10: [static-object и Windows offscreen backend](matrixgame-e2e519-map-static.md).
Полный static-object TU подключён; original empty sort/remove и vertex predicate
исполнены на host/ARM64. Настоящий Windows HAL проверил target/depth, Clear pixels,
CPU pitch roundtrip и bindings/viewport restore. devkitA64 prefix compile/link
PASS. Full map owner LINK FAIL11; robot portrait, CGame::Init и Switch draw NOT RUN.

Предыдущее продолжение24.10: [группы и оригинальная очистка владельца](matrixgame-e2e519-map-group.md).
Полный group TU и три exact original cleanup methods подключены. Настоящий
visibility Release непустых heap arrays исполнен на host и ARM64/QEMU;
devkitA64 compile/common prefix link PASS. Full map owner LINK FAIL12;
GroupClear runtime, BuildBottom/BuildWater и CGame::Init NOT RUN.

Предыдущее продолжение24.10: [эффекты, billboard и point-light pool](matrixgame-e2e519-effects.md).
Полные оригинальные модули и original CVectorObjectAnim подключены к общей
цепочке. Реальные staging allocation/reuse/ClearEffects и line geometry
исполнены на host и ARM64/QEMU; devkitA64 compile/link PASS. Full map owner
LINK FAIL13; InitEffects на ресурсах мира, CGame::Init и GPU draw NOT RUN.

Предыдущее продолжение24.10: [полный skin manager](matrixgame-e2e519-skin-manager.md).
Оригинальный GetSkin исполнен до отказа Prepare без устройства; доказанный
ASan UAF повторного Clear исправлен сбросом ownership slots. Общий host и
ARM64/QEMU runtime PASS, devkitA64 compile/link PASS. Full map owner LINK FAIL15;
успешный материал, skin draw и CGame::Init не выполнены.

Предыдущее продолжение24.10: [water и named SDK cache](matrixgame-e2e519-water.md).
Полный water TU подключён; original ctor/Clear/dtor и inshore quad PrepareVB
исполнены в общем host/ARM64 bootstrap. Typed named cache сохраняет original
Find/Name/Add/Up и parent Deinit, конфликт portable типа отвергается без cast.
SDK ELF PASS; Windows actual priority resource runtime PASS отдельно.
Full map owner LINK FAIL16, Water map Init/Draw и CGame::Init не выполнены.

Актуальное продолжение24.10: [bottom texture/DXT1 ownership](matrixgame-e2e519-bottom-texture.md).
Полный original bottom TU подключён; UBSan доказал неверный CacheTexture→
CTextureManaged cast, retry baseline — потерю одного материала после Clear.
Исправлены factory и атомарная публикация. Общий host/ARM64 runtime и SDK link
PASS; Windows actual DXT1/six mip backend runtime PASS отдельным адресным тестом.
Full map owner LINK FAIL17, CGame::Init и бой не исполнены. Gates разделены.

Актуальное продолжение23.10: [terrain lifecycle](matrixgame-e2e519-terrain.md).
Полный MatrixTerSurface.cpp включён в общий bootstrap; исходный repeated Clear
воспроизвёл ASan double-free. Сброс ownership state проверен host/ARM64 runtime,
SDK compile/link и MSVC compile. Настоящий map owner LINK FAIL18 unique symbols,
геометрия/GPU terrain не загружены; CGame::Init и бой по-прежнему не исполнены.

Актуальное продолжение23.10: [CalcPick viewport ABI](matrixgame-e2e519-viewport.md).
Подтверждённый runtime failure из-за int/reference screen globals устранён
единым SDK-free accessor; исходная геометрия проверена в общем host/ARM64
bootstrap и против сохранённого x86 oracle. SDK ELF LINK PASS; Windows LINK
FAIL150, прежние два screen symbols исправлены. Full map/CGame::Init не выполнены.

Актуальное продолжение23.10: [Windows original debug font owner](matrixgame-e2e519-font-owner.md).
Исполнены исходные DebugInfo ctor/T/Takt/lost/reset/dtor после настоящей
InitAsDLL negotiation; исправлены DrawText macro ABI и shared color-op symbols.
Host/ARM64 общий bootstrap и SDK ELF PASS; полный Windows bootstrap LINK FAIL152,
настоящий derived map owner требует19 unique cleanup symbols. CGame::Init
не исполнен; Switch font backend Unsupported. Нижние этапы — история.

Актуальное продолжение23.10: [настоящий CMatrixMap ctor и debug font](matrixgame-e2e519-debug-font.md).
Прежние три отсутствующих symbols закрыты; общий host/ARM64 bootstrap реально
входит в конструктор и проверяет unwind/retry при отсутствии font backend.
SDK ELF LINK PASS; полного создания карты и CGame::Init ещё нет. Нижние записи
с прежними link blockers сохранены как история, текущая граница — font Create.

Последнее продолжение23.10: [полные Console/Sound dependencies](matrixgame-e2e519-console-sound.md).
Оригинальные Console ctor/dtor и Sound Init/Clear исполняются в общем bootstrap
host/ARM64; SDK ELF и MSVC compile gates отдельны. Настоящий map-owner link
теперь ограничен DebugInfo ctor/dtor и Transition::Clear. Картой/боем это не
объявляется; Console ASan global redzones исключены с доказательством причины.

Последнее продолжение23.10: [полная MatrixMap и device bridge](matrixgame-e2e519-map-device.md).
Map TU компилируется на host/ARM64/SDK и включена в общий archive. Настоящий
ctor/dtor link ещё FAIL: CDevConsole, CMatrixDebugInfo, CTransition, CSound::Init;
старый manual recipe также не подключал уже перенесённый navigation archive.
CGame::Init, карта и бой ещё NOT EXECUTED; fresh source/CMake gates отдельны.

Последнее продолжение23.10: [plane/line contract](matrixgame-e2e519-plane-line.md).
Математическая зависимость MatrixMap снята с проверкой против настоящей D3DX9
x86 DLL и общего bootstrap host/ARM64. Следующая compile-граница —
DrawLandscapeSurfaces/SetSamplerState (prepared MatrixMap.cpp:1500).
Полная карта, CGame::Init и бой ещё NOT EXECUTED.

Последующее продолжение23.10: [original input и Map dependencies](matrixgame-e2e519-map-input.md).
Общий bootstrap теперь исполняет original input.cpp с настоящими game defaults;
host/ARM64 runtime и MSVC/SDK compile/link gates разделены в отчёте. Полная
MatrixMap.cpp дошла до D3DXPlaneIntersectLine, но карта/CGame::Init ещё NOT RUN.

Актуальное продолжение23.10: [полная MatrixLogic.cpp](matrixgame-e2e519-map-logic.md)
скомпилирована и включена в общий bootstrap; оригинальный path helper исполнен
на host/ARM64, SDK ELF скомпонован. Карта и полный CGame::Init ещё не запускаются.
Ниже сохранена история этапов; начальная сводка cursor-stage историческая,
её D3DDISPLAYMODE blocker уже снят позднейшим full-CGame этапом.

Актуальное продолжение: **на Windows выполняется безопасный исходный cursor
constructor/Clear через настоящий SetCursor/GetCursor, game heap и cursor
принадлежат общему owner; исходный control flow CGame сохранён**.
Первая ошибка теперь в ApplyVideoParams: `MatrixGame.cpp:438 D3DDISPLAYMODE`
(effective source; исходная строка 525).
Коммит `6e024b8495cd9545361b2d74593c3af0948141a7`, актуальные хэши —
`matrixgame-e2e519-cursor-lifecycle-evidence.json`; подробности в последнем разделе.
Config/resources этап (`72a49dd`, config evidence) сохранён ниже как история.
На Windows выполняются прежние оригинальные DLL/sampler/S3D_Default/DeviceState
с реальными SDK state calls. Полный CGame::Init ещё не вызван.
Header-boundary этап (`6c947f2`, header evidence) сохранён ниже как история.
Предыдущий DeviceState этап (`1d5b8c8`, state evidence) сохранён как история.
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

## Продолжение: 3G/Texture header contract и следующий реальный CGame blocker

База: `72c66d40a04806a7fce14b357336eab143ae0f34`.
Реализация: `6c947f22e05bc569a0482153a9661c1c6f46aaae`, локальный master,
без push/fetch. Evidence: `matrixgame-e2e519-header-boundary-evidence.json`.

Прежний первый барьер `3g.hpp:16 d3d9.h` снят. Полный 3g.hpp теперь включает
полные объявления оригинальных CBaseTexture/CTexture/CTextureManaged и ранее
перенесённый DeviceState, не импортируя concrete graphics SDK. Поля и хвосты
классов сохранены. SDK interfaces и struct tags объявлены настоящими opaque
типами; фиктивных device methods/успешных HRESULT/возможностей GPU нет.

Оба исходных варианта color/alpha operations (`DO_SMART_COLOROPS` и обычный)
перенесены из inline header в существующий 3g.cpp. В существующий Texture.cpp
перенесены исходные managed destructor, Preload, CreateLock, LockRect и UnlockRect,
включая managed/non-managed branches. Native format/pool value identifiers
преобразуются в SDK enum явно на границе реализации. Сами 3g.cpp/Texture.cpp
ещё **НЕ COMPILE_PASS и НЕ EXECUTED**: SDK-зависимые тела сохранены для
продолжения переноса, а не заменены пустыми функциями.

Выявлен риск перегрузки: старый SDK enum формата после изменения объявления
мог бы выбрать LoadFromBitmap(..., bool), изменив convert_to_16bit. Deleted enum
overload требует явного переноса такого вызова. C++ concepts в существующем
bootstrap подтверждают: bool/native format допустимы, старый enum недопустим.

Компиляция полного header вместе с cache/VO обнаружила дубли Cache/Math/Form/
bitmap declarations в двух generated views. Стабильные guards задают идентичность
логического исходного header между физическими путями. Для реконструируемого
VectorObject.hpp сохранён прежний отдельный shared ABI guard. Существующие
native core cpp bytes при этом не менялись; новые guards относятся к headers.

Дополнительно удалена транзитивная SDK-зависимость ID3DXFont pointer из
MatrixDebugInfo.hpp, конкретный include остался в MatrixDebugInfo.cpp.
DWORD/WORD/BYTE fields явно нормализуются в uint32/uint16/uint8 **после** проверки
pinned algorithm overlays. Проверки исходного navigation SHA не ослаблены.
Сохранены оригинальные release tracer definitions; его Windows/x86 debug helpers
ограничены исходной debug/trace конфигурацией. MatrixMapStatic получает явный
Texture.hpp include для ETexSize. Ни новых эффектов, ни нового draw path нет.

### Что проверено, а что ещё не достигнуто

| Проверка | Фактический результат |
|---|---|
| Windows x86 bootstrap | PASS: full 3G/Texture headers совместимы с общими cache/VO/Form types; прежние оригинальные методы исполняются, 207 SDK state values восстановлены, cleanup/reentry успешны |
| Host ASan/UBSan/leak detection | PASS CPU/Unsupported scope; full headers компилируются, GPU prefix явно unsupported |
| ARM64/QEMU | PASS того же CPU/Unsupported scope и полного header contract |
| devkitA64 | COMPILE/LINK PASS, undefined strong=0; без NRO и без hardware runtime |
| MatrixGame.cpp | По-прежнему COMPILE_FAIL; первая ошибка переместилась в MatrixEffectKonus.hpp:35:12, D3D_VB does not name a type |
| 3g.cpp / Texture.cpp SDK bodies | NOT COMPILED / NOT EXECUTED; header PASS не означает их готовность |

Это адресное продвижение compile boundary, **не новая выполненная стадия
CGame::Init**. Наблюдаемая runtime цепочка остаётся прежней: общий Cache/VO/SSkin
owner → DLL/caps → CDeviceState capture → sampler/S3D_Default → CForm callbacks →
DeviceState restore/Deinit/reentry. Исходное тело MatrixGame.cpp, включая Init,
ApplyVideoParams и game loop, совпадает с предыдущим effective cpp SHA:
`19c1485c608202ff088c4b37a62ca2508ee9b064f655017799e6bae27fae672b`.
Тело Init не вырезано и не заменено. После первой ошибки следуют настоящие
resource cleanup macros и camera viewport/projection/matrix dependencies.

Полные legacy texture declarations ещё не означают готовность texture backend:
эти CTexture/CTextureManaged объекты не создавались. Действующий native cache
использует ранее проверенный NativeCacheTexture; его интеграцию с настоящими
потребителями legacy texture API требуется завершить до вызова полного Init.

Сверены реальные байты всех core outputs и 235 prepared engine files на host,
ARM64, Windows и SDK, а также в source view адресного CGame audit. JSON marker
SHA может отличаться из-за порядка directory enumeration Windows/WSL:
полные структурированные объекты manifests равны, все file hashes подтверждены
чтением файлов. Это не разрешение расхождений по именам.
Host manifest SHA `c95a3ea86b4f28d14a45ec5d3c72a024ec51de0bb6996ea0edf3941e89660ee1`;
audit marker SHA `447cb99fdf32a560e24b4c9349f791c2747c4f6dcaaa040ec76879a6d7827af9`.
SDK ELF SHA `5ce9ef0bbfb740bb52dacdf90d748def2e8a578eac2fa95d77ab2c722ea40bc2`.
Все binaries/logs/source hashes и ограничения build_dirty находятся в evidence.
Общий inventory 106 единиц не повторялся, новый общий счёт PASS не заявляется.

Все 14 защищённых SHA совпали. Upstream чист, pin/gitlink прежний; saves/resources,
production NRO и hardware ZIP не менялись. Switch/GPU draw — HARDWARE_PENDING.
Ветка master остаётся локальной, цель полного входа в игру активна.

Следующие три приоритета:

1. Resource declarations и исходный buffer cleanup эффектов/карты, начиная с
   `MatrixEffectKonus.hpp:35 D3D_VB`, только как зависимости настоящего Init.
2. Camera viewport/projection math и типизированный device presentation/Reset
   contract, без имитации SDK операций.
3. Замкнуть реальные texture/map/static-init owners перед CGame::Init, затем
   CFormMatrixGame/RunGameLoop; не вызывать Init при незавершённых предусловиях.

## Продолжение: исходные config defaults и общие effect/map resources

База: `b15bbf606c7c1f6645f102fc24b92f60d61f78df`.
Реализация: `72a49dd223457ccf5e298b612beb4042e660e403`, master, без push/fetch.
Evidence: `matrixgame-e2e519-config-resources-evidence.json`.

### Новая фактически исполненная стадия

`NativeInitRuntime::InitializeGameDefaults` вызывает полное исходное тело
`CMatrixConfig::SetDefaults` из MatrixConfig.cpp:177. Это та же операция,
которую настоящий CGame::Init вызывает в MatrixGame.cpp:319. Используется полный
исходный класс CMatrixConfig и его единственный настоящий global g_Config,
не сокращённая модель настроек. g_MaxFPS=1000, camera distances/angles,
shadow/gloss/software-cursor flags, capture times и serialized key actions
проверены после исполнения. VK identifiers перенесены в их исходные численные
значения; никакие input/device функции не имитируются.

Метод требует этот живой CPU/cache owner, не включает GPU readiness и не позволяет
повторно публиковать уже принадлежащие ему defaults. На Windows новая стадия
исполняется после настоящего DLL/device bootstrap, перед sampler/S3D_Default.
На host/ARM64 это независимо исполняемая CPU-стадия; неподдержанный window/device
prefix остаётся явным отказом. Это не притворное продолжение CGame после отказа
GPU и не выполнение всей последовательности Init.

Новый адресный отказ: уже существующий real cursor-label array на cache heap
не позволяет SetDefaults потерять прежнее владение. Отказ сохраняет array/string
и не публикует readiness; после исходного Clear повторная инициализация успешна.
Затем два реально созданных std::wstring pairs освобождает исходный
`CMatrixConfig::Clear` (MatrixConfig.cpp:322) при owner Deinit, до device/cache
teardown. Owner инвалидирует освобождённые pointer/count: double Deinit и второй
полный вход не используют dangling array. ASan leak detection прошёл.
ReadParams/ApplySettings не реализованы этим selected-methods TU и не вызваны;
это **не COMPILE_PASS полной MatrixConfig.cpp**.

### Resource и camera boundary

Поля D3D_VB/D3D_IB эффектов/карты переведены на те же реальные typed staging
VertexBuffer/IndexBuffer, что используются принятыми BigVB/BigIB, через приватные
native buffer macros. Второй context/manager не создан. Pool/dynamic/map modes
сохранены; Normal остаётся прежним default, новые Discard/NoOverwrite передаются
реальному Resource::Map. Изменение не объявляет staging allocation GPU-ready.
Реальный legacy D3DControl SDK source сохранён, counterfeit d3d9.h не добавлялся.

В том же driver исполняются исходные inline CMatrixEffectKonus::StaticInit
(MatrixEffectKonus.hpp:44) и MarkAllBuffersNoNeed (:49), с исходными statics.
Проверяется освобождение настоящих 96 staging bytes в текущем общем context,
обнуление handle, повторный cleanup без повторного освобождения. Конус как объект
не создаётся, его virtual gameplay/draw methods не подменяются и не вызываются.
Это lifecycle проверки реальной зависимости, а не rendered effect.

Полный MatrixCamera.hpp теперь компилируется вместе с настоящими config/effect
declarations. D3DXVECTOR4 хранит реальный XMFLOAT4; matrix multiplication,
perspective, TransformCoord и Project выполняются DirectXMath. Viewport —
собственный POD value, без имитации D3DVIEWPORT9 interface. Проекция с ненулевым
viewport origin, world/view translations и depth range сверена с независимой
аналитической формулой; in-place TransformCoord и порядок world*view*projection
проверены. Camera object/constructor/Takt/Draw **не исполнялись**.
Serialized vertex layout bits перенесены отдельно от capability/backend API.
Callback ABI сохраняет stdcall на x86 и native calling convention на ARM64.

### Подтверждённые результаты и новая граница

| Проверка | Фактический результат |
|---|---|
| Host ASan/UBSan + leak detection | PASS: реальные SetDefaults/Clear, refused ownership/retry, effect cleanup, projection; GPU prefix Unsupported |
| ARM64/QEMU | PASS того же реального CPU-кода; положительный GPU prefix не исполнялся |
| Windows x86/D3D9 NULLREF | PASS новой config стадии плюс прежние DLL/window/input/sampler/S3D_Default/DeviceState calls и restore 207 values |
| devkitA64 | COMPILE/LINK PASS; undefined strong=0; настоящие config wrappers присутствуют в symbol table; GPU runtime NOT RUN |
| Адресный MatrixGame.cpp | COMPILE_FAIL переместился с MatrixEffectKonus.hpp:35 D3D_VB на MatrixCursor.hpp:19 HICON |

SetDefaults/Clear могут быть встроены оптимизатором в их cross-TU wrappers;
SDK проверяет реально требуемые InitializeOriginalGameDefaults и
ClearOriginalGameDefaults, не требует искусственно удерживать неиспользованный
standalone symbol. Новый executable/probe не создан, прежние большие suites и
полный inventory 106 единиц не повторялись.

Новый effective MatrixGame.cpp SHA:
`0039afb72023870af1543e9a6a006a8c4c60e67a13a28a9d8280cf1e5d77e0d9`.
Обратная замена только native viewport type и двух explicit includes возвращает
прежний SHA `19c1485c608202ff088c4b37a62ca2508ee9b064f655017799e6bae27fae672b`:
весь control flow Init/ApplyVideoParams/Run остаётся исходным, тело не вырезано.
Generated GameDefaultsNative.cpp SHA:
`3aea1deeef59b04202a4b25430a32b6d8bd81927b0f47daf5cd3b7d34d0a0e9c`.
Сверены реальные байты всех core outputs и 236 engine prepared files между четырьмя
платформами и audited view, а не только список имён. JSON ordering difference
остаётся документированным; structured manifests и source bytes равны.
SDK ELF SHA: `a700c252ba0750e7af8e38ebcfb2713ee2ee03f05ecf3fd84e94cf425c5ebcc4`.
Точные остальные хэши, logs/commands, build_dirty и исходные pin hashes — в evidence.

Точная новая runtime граница: общий cache/VO/SSkin owner и платформенный bootstrap
→ **оригинальные config defaults + owned Clear**; на Windows далее прежние
sampler/S3D_Default/DeviceState/input/Deinit. Full CGame::Init, загрузка карты,
CFormMatrixGame и RunGameLoop не выполнены. Следующая compile проблема cursor
не ограничивается typedef: исходный constructor также вызывает SetCursor и
обнуляет нетривиальный std::wstring через memset, что требует настоящего
window/resource lifecycle, а не пустого SDK вызова.

Все 14 защищённых SHA совпали. User saves/resources, main NRO/ELF/hardware ZIP,
upstream/gitlink не менялись. Switch/GPU draw — HARDWARE_PENDING, цель активна.

Следующие три приоритета:

1. Настоящий MatrixCursor/window lifecycle и оставшиеся interface declarations,
   начиная с подтверждённого MatrixCursor.hpp:19 HICON/SetCursor.
2. ReadParams/ApplySettings и legacy texture/map ownership на настоящих ресурсах,
   без смены действующего cache owner и без фиктивных настроек.
3. Presentation/Reset и обязательные static-init/map зависимости перед реальным
   CGame::Init, затем CFormMatrixGame/RunGameLoop.

## Продолжение: настоящий cursor lifecycle и owned game heap

База: `91fdc120225bb375b36e854c538746d7bb6ff488`.
Реализация: `6e024b8495cd9545361b2d74593c3af0948141a7`, локальный master,
без push/fetch. Evidence: `matrixgame-e2e519-cursor-lifecycle-evidence.json`.

### Исполненный код и исправленная ошибка

Исходный constructor MatrixCursor.hpp:33 делал memset всего объекта, включая
уже созданный std::wstring. В portable overlay все исходные scalar/pointer
members получают те же нулевые значения явно, string сохраняет корректный
стандартный lifetime. Затем выполняется исходная операция скрытия системного
курсора через настоящий Win32 SetCursor(NULL), с сохранением старого handle.
Полный класс сохранён; добавлен borrowed typed Window pointer для cleanup.

Constructor и исходное полное тело `CMatrixCursor::Clear` (MatrixCursor.cpp:212)
скомпонованы в MatrixCursorLifecycle.cpp. Clear вызывает Window backend для
SetCursor/DestroyIcon, сохраняет genuine virtual texture Unload contract и
освобождение icon array на game heap. Новых фиктивных HICON/HCURSOR typedefs,
SDK functions или успешных значений не добавлено: native handle values принадлежат
типизированной границе, Win32 provider реально вызывает SetCursor/GetCursor.
Icon/texture selection и Draw не исполнены и не объявляются готовыми.
Это **selected lifecycle TU, не COMPILE_PASS полной MatrixCursor.cpp**.

`NativeInitRuntime` теперь владеет отдельным настоящим CHeap/g_MatrixHeap,
соответствующим исходному CGame::Init:181; это не alias g_CacheHeap. Game heap
создаётся до CacheInit и освобождается после cursor/cache/buffer teardown.
Begin отказывает при чужом существующем game heap, Deinit не удаляет и не
обнуляет чужой pointer. Это проверено на реальном CHeap object.

InitializeCursor требует живой negotiated Window/Device и исходные config
defaults, не публикует cursor при отказе constructor/backend и отвергает
вторую инициализацию в том же owner. Cursor уничтожается до window provider
при DetachDevice/Deinit. Borrowed Cursor() pointer действителен лишь до detach.

На Windows SDK helper загрузил настоящий shared system arrow и сохранил cursor
самого caller. Readback подтвердил: constructor скрывает курсор, Clear и второй
Clear восстанавливают тот же ненулевой handle; detach, reattach и второй полный
вход корректны. Refusing backend не создаёт объект и не меняет прежний OS handle.
Перед owner Deinit намеренно оставлен живой cursor: он очищается до heap/window,
caller HWND/WndProc остаются живы; системный arrow не уничтожается.

На host/QEMU window prefix по-прежнему Unsupported. Проверен именно реальный
constructor exception/unwind с корректным string lifetime и сохранённым game
heap, а не имитирован успешный системный курсор. Прежние config/VO/SSkin CPU
стадии проходят в том же owner. Game heap/cursor не разрешают вызывать весь
CGame::Init поверх уже созданного cache/heap: prefix ownership ещё требуется
согласовать при подключении полного Init, двойной инициализации быть не должно.

### Следующий compile barrier теперь внутри video method

Сняты header барьеры cursor/shadow/interface. Реальный CVOShadowProj объявлен
полностью, SDK-зависимые BeforeRenderAll/Render/RenderCustom bodies перенесены
в существующий ShadowProj.cpp; исходный CMultiSelection::DrawAll — в его CPP.
Тела сохранены, **не скомпилированы/не исполнены** как GPU implementation.
POD RectI заменяет только rectangle value, ZeroTrivialBytes реально обнуляет
trivially-copyable storage и запрещает обнуление нетривиального объекта.
Притворных graphics methods здесь нет.

Настоящий CGame Init использует явные pointer→uintptr conversions на прежней
raw device границе. ShowWindow(SW_SHOWNORMAL)/UpdateWindow подключены к
реальному typed Window backend, эти provider operations проверены на Windows.
Полный algorithm/control flow не вырезан. После обратной замены только
документированных type/includes/memory/window boundary changes получается
прежний effective CGame cpp SHA
`0039afb72023870af1543e9a6a006a8c4c60e67a13a28a9d8280cf1e5d77e0d9`.
Новый SHA `cc776094dce5eea7eac791e5ab8f1a1ecd4b7a04c74d028945a21039a7d91edd`.

Адресный audit теперь впервые проходит эти declarations и останавливается
в самом `CGame::ApplyVideoParams`: effective MatrixGame.cpp:438:5,
`D3DDISPLAYMODE was not declared in this scope` (pinned original line 525).
Далее остаются настоящие creation/display mode, presentation/Reset, viewport,
material/light и loop-timer/platform contracts; их успех не имитируется.

| Проверка | Фактически подтверждено |
|---|---|
| Windows x86 SDK/NULLREF | PASS реального ctor/Clear/window show/update, refusal/no mutation, repeated Clear, detach и два полных reentry; прежний SDK state restore PASS |
| Host ASan/UBSan + leak detection | PASS CPU/game-heap/constructor rejection/unwind, сохранность чужого heap, config cleanup; positive cursor window backend отсутствует |
| ARM64/QEMU | PASS того же CPU/Unsupported scope; не Switch runtime |
| devkitA64 | COMPILE/LINK PASS, undefined strong=0; реальные ctor/Clear/InitializeCursor symbols присутствуют |
| MatrixGame.cpp | COMPILE_FAIL с новым первым барьером внутри ApplyVideoParams; CGame::Init не вызывался |

Нового standalone executable не создавалось. Общий inventory 106 units и старые
большие suites не повторялись. Все core outputs и 237 prepared engine files,
включая audited view, сверены по реальным байтам между четырьмя платформами;
structured manifests равны, особенности JSON key order сохранены в evidence.
Generated cursor lifecycle SHA:
`2cfeaf9acff9fcd93540da5de7d24251c873a1325c196caa8a67c987f12c47d6`.
SDK ELF SHA: `836a1bde10a07ec29dd8d31dcfd83bd8db29ceec6caa2ce60dcdf2ef6c37f620`.
Остальные binary/source hashes, команды/логи, build_dirty и SDK build id — в evidence.

Точная новая выполненная стадия: общий game/cache/buffer owner → DLL/window/caps
→ config defaults и прежние sampler/S3D_Default/DeviceState → **настоящий cursor
constructor/Clear с OS restore** → owner cleanup/reentry. Полный Init/map/form/loop,
cursor assets и физический GPU draw остаются невыполненными.
Все 14 защищённых SHA совпали, saves/resources/main NRO/ELF/ZIP/upstream не
менялись. Switch — HARDWARE_PENDING, цель активна.

Следующие три приоритета:

1. Typed creation/display/presentation/Reset contract для полного ApplyVideoParams,
   начиная с подтверждённого D3DDISPLAYMODE, с настоящими provider results.
2. ReadParams/ApplySettings и реальные cursor/texture/map resources; не считать
   пустой cursor lifecycle доказательством готовности selection или draw.
3. Согласовать настоящий CGame::Init с единственным game/cache owner, замкнуть
   обязательные static-init/map dependencies, затем CFormMatrixGame/RunGameLoop.

## Продолжение 23.10.2026: оригинальный CGame::ApplyVideoParams

Implementation commit: `70444b5c5dfef4157d33c13bc1922af501bff799`, локальный master,
без push/fetch. Подробные SHA, результаты компиляции, полные runtime-логи и
protected-input checks: [video negotiation evidence](matrixgame-e2e519-video-negotiation-evidence.json).

В существующий NativeInitRuntime подключено полное оригинальное тело
`MatrixGame.cpp:521 CGame::ApplyVideoParams` через typed device/window contract.
В производственном Init вызов находится на строке 435, после подготовки карты;
**Init и подготовка карты здесь не выполнялись**. Под тем же владельцем cache,
game heap, config и sampler реально исполнена начальная часть метода, которой
карта ещё не нужна. Свет в оставшемся теле по-прежнему читает настоящие поля
g_MatrixMap; подставного объекта карты нет.

Новый Windows provider вызывает GetCreationParameters, GetDirect3D и
GetAdapterDisplayMode, GetClientRect/GetWindowRect. Полученные значения
проверены по всем полям против независимых прямых SDK-вызовов. Временный COM
reference на API освобождается RAII, включая ошибку неверного adapter ordinal.
CheckDeviceMultiSampleType также возвращает настоящий HRESULT; он сверён
отдельно с SDK, но оригинальный метод до этого вызова пока не доходит.
NULLREF сообщает нулевой hFocusWindow даже при переданном HWND создания;
адаптер сохраняет это фактическое значение.

Исполнены original bpp/refresh calculations, проверка FSAA >16 и обе ветви
выбора режима. Оконная ветвь доходит до ResizeOuter и получает Unsupported;
ветвь изменённой частоты — до SetFullscreenStyle и получает Unsupported.
До отказа провайдеры не изменяют окно/устройство. Владелец откатывает общие
flags/presentation; проверены неизменные RECT, world matrix и количество
живых state leases, повторные вызовы и два полных входа/выхода bootstrap.
Presentation — объект CPU-параметров, не измерение действующего видеорежима.

Reset, viewport, material/light и последующий S3D_Default в этом методе
**NOT EXECUTED**. Их ветви сохранены целиком; неподдерживаемые backend calls
бросают Unsupported. Для Reset требуется сначала закрыть владение default-pool
references/stateblock и восстановление чужих presentation/window/viewport.
Существующий state lease пока не reset-safe, поэтому ложного Reset PASS нет.
Оригинальные g_MatrixHeap/g_MatrixMap имеют единственное определение в общем
runtime; полная source view объявляет их extern. g_MatrixMap остаётся null.

| Проверка | Результат и граница |
|---|---|
| Windows x86 SDK/NULLREF | PASS нового участка оригинального video method, SDK readback/error, MSAA validation, window/fullscreen отказа, rollback/retry и owner reentry; физический GPU draw не проверялся |
| Host ASan/UBSan + leak detection | PASS integrated CPU lifecycle; ApplyVideoSettings явно Unsupported без устройства |
| ARM64/QEMU | PASS того же integrated CPU/Unsupported scope, не Switch runtime |
| devkitA64 | COMPILE/LINK PASS, undefined strong=0; реальные CGame::ApplyVideoParams/ApplyVideoSettings/g_MatrixMap присутствуют в ELF; runtime NOT RUN |
| Полная MatrixGame.cpp | COMPILE_FAIL: прежний D3DDISPLAYMODE:438 снят; новый первый error effective:698 timeBeginPeriod, original:783 RunGameLoop; также timeEndPeriod original:807 SafeFree |

`GameVideoNative.cpp` — выбранный полный метод, **не полная TU MatrixGame.cpp**.
Тот же адаптированный метод присутствует в полной compile-audit source view.
Проверка восстановлением прежнего тела и двух деклараций владельцев даёт
побайтово прежнюю MatrixGame.cpp; вне этих границ Init/loop/gameplay не менялись.
Все core outputs и 238 prepared engine files проверены по байтам между host,
Windows, ARM64 и SDK, включая сохранённую audited source view. Binaries собраны
до коммита (build_dirty=true, embedded Git SHA отсутствует); evidence связывает
их с implementation SHA и точными effective-source/binary SHA-256.

SDK ELF SHA-256:
`c850d8cbcc87051eee5ee4d9044690757ced2f26d6950ba65b1065b1ba869725`.
Build id: `b85acdc3e4b4c4784d426b940b732dd598026bb3`.
Новых executable targets, повторного inventory 106 и standalone suites нет.
Все 14 protected hashes совпали; saves/resources/upstream и frozen аппаратные
архивы сохранены. Основная NRO не пересобиралась. Switch — HARDWARE_PENDING.

Дальше: (1) реальные timer begin/end contracts для полного MatrixGame.cpp,
(2) reset-safe владение borrowed device/window, (3) ReadParams/map/static-init
и согласование единственного owner с настоящим CGame::Init. Этап E2E-5.19 активен;
полный запуск Init/form/loop и бой ещё не достигнуты. Вечерний аппаратный сценарий
основной игры и отдельного прежнего GPU-прототипа остаётся в
[инструкции](../docs/e2e517-evening-switch-check.md).

## Продолжение 23.10.2026: полная CGame TU в общем bootstrap

Implementation: `48b469f8a3936e28bfebcb75a372c6b4b2cdf18f`, локальный master,
без push/fetch. [Полное evidence](matrixgame-e2e519-full-cgame-timing-evidence.json)
содержит фактические логи, 99 Windows linker errors, effective-source/object/binary
SHA-256 и protected-input checks.

Снят последний compile blocker полной `MatrixGame.cpp`: исходные
`RunGameLoop:783 timeBeginPeriod` и `SafeFree:807 timeEndPeriod` теперь обращаются
к typed platform timer lease. Общий platform3g State владеет одним запросом,
отклоняет повторный Begin и несовпадающий End, освобождает запрос до teardown
заимствованного окна/устройства. Win32 provider содержит настоящие WinMM
timeBeginPeriod/timeEndPeriod; без провайдера запрос явно Unsupported.
Возвращаемое значение End в исходном SafeFree по-прежнему игнорируется, чтобы
ошибка release не пропустила дальнейший Deinit. Положительное выполнение WinMM
**NOT RUN**, поскольку новая Windows-компоновка заблокирована (см. ниже).

Полная TU теперь является настоящим input `matrixgame-native-cache` вместо
`GameVideoNative.cpp`; прежний generated файл и его archive member удалены из
управляемых source views. Новый executable/harness не создавался.
Оригинальный logger использует свой существующий ostream-конструктор со
std::clog, поэтому подключение полного объекта не открывает test.log в cwd.
Обратная подстановка только двух timer calls и logger constructor даёт
побайтово прежнюю полную MatrixGame.cpp; Init/Deinit/static_init/SaveResult и
остальной RunGameLoop не вырезались и не заменялись.

**Новая точная граница исполнения:** тот же CPU/cache/game-heap/config owner →
настоящий `CGame::ApplyVideoParams` из полного MatrixGame.cpp object → первая
creation-parameter операция получает Unsupported из ReadyDevice. На host и
ARM64 нет подставного устройства/режима экрана; flags/presentation/owner
сохранены, два цикла очистки и re-init проходят. Wrapper допускает этот
проверяемый отказ исходного метода при живом CPU/config owner; при наличии
GPU сохраняется обязательное предусловие sampler settings.

| Gate | Итоговая фактическая граница |
|---|---|
| Полная MatrixGame.cpp | **COMPILE_PASS**, независимый audit настоящего объекта; прежний baseline timeBeginPeriod COMPILE_FAIL сохранён |
| Host ASan/UBSan + leak detection | **PASS** integrated bootstrap: реальный вызов оригинального video method из полной TU, device refusal, owner cleanup/reentry; Init/Run NOT CALLED |
| ARM64/QEMU | **PASS** того же CPU/error scope; не Switch runtime |
| devkitA64 | **COMPILE/LINK PASS**, strong undefined=0; полный объект включён, ApplyVideoParams retained; Init/Run sections не входят в итоговый ELF |
| Windows x86 | Полная TU **COMPILE_PASS**, bootstrap **LINK_BLOCKED**, 99 настоящих unresolved symbols; новый runtime **NOT RUN** |
| Switch/GPU/map/battle | **HARDWARE_PENDING / NOT EXECUTED** |

ELF-компоновка использует function/data sections и --gc-sections: удерживает
реально вызываемый метод и отбрасывает незамкнутые Init/Deinit/Run/static_init.
Это не доказательство полной engine link closure. MSVC /OPT:REF сообщает 99
unresolved references полного объекта. Текущая Windows-конфигурация bootstrap
после перехода на полную TU временно не компонуется; её предыдущий selected-method
PASS не объявляется PASS новой сборки. Подавления unresolved errors и фиктивных
определений нет. Положительные Windows timer assertions скомпилированы, не запущены.

Конкретные подтверждённые зависимости Windows full-object link:

| Оригинальный узел | Источник / причина |
|---|---|
| CMatrixEffect::InitEffects | `MatrixGame/src/Effects/MatrixEffect.cpp:273`; нет связанного оригинального init-effects implementation |
| Legacy L3GInitAsDLL / L3GRun / L3GDeinit | `MatrixLib/3G/3g.cpp`, DLL init:346; текущий typed backend не реализует весь legacy entry ABI |
| CMatrixConfig::ReadParams / ApplySettings | `MatrixGame/src/MatrixConfig.cpp:334 / :155`; пока подключены только SetDefaults/Clear |
| CMatrixCursor::Select | `MatrixGame/src/MatrixCursor.cpp:45`; lifecycle портирован, selection/resources ещё нет |
| CRenderPipeline constructor | `MatrixGame/src/MatrixRenderPipeline.cpp:2300`; реальный render/map/material graph не замкнут |
| Static map/effect/interface/selection resources | Все точные decorated/demangled имена и контексты Init/Deinit/static_init сохранены в evidence; нулевые определения ради LINK PASS не добавлялись |

Перед настоящим вызовом Init также необходимо согласовать его исходные
alloc/static-init/cache действия с единственным NativeInitRuntime owner.
Текущий native bootstrap уже владеет heap/cache: прямой вызов полного Init
без этого согласования создал бы конфликт владельцев. Init не запускался.

237 prepared engine files и все core outputs сверены по реальным байтам на
четырёх платформах и с сохранённой audited view. Binaries построены до commit,
build_dirty=true, embedded Git SHA отсутствует; evidence связывает их с точным
implementation SHA. SDK ELF SHA-256:
`07aa6742d0bcdde77867aa32b1f4fdb398f42fd701d3109ab7e2fe2a734bd41d`.
Build id: `7fe180a4fbd562adea240108ac7472f97d6b9bdf`.
Windows executable как новый кандидат не публиковался.

Все 14 protected SHA совпали. Saves/config/logs/resources/upstream и frozen
аппаратные архивы сохранены; основная NRO не пересобиралась. Общий inventory
106 и большие standalone suites не повторялись. E2E-5.19 остаётся активным.

Следующие приоритеты:

1. Закрыть реальные entry/config зависимости полного CGame object: legacy
   L3G entry и оригинальные ReadParams/ApplySettings; восстановить Windows link.
2. Согласовать ownership/порядок исходного CGame::Init с общим owner и затем
   продвигать его настоящую map/static-init цепочку.
3. Закрыть reset-safe device/window/presentation и связанные реальные ресурсы;
   проверить WinMM lifecycle после восстановления Windows runtime.

## Продолжение 23.10.2026: полная MatrixConfig и настоящий data.txt

Implementation: `77fe348a1f4c0bddf2d108081b9eb38f75eb2db5`, локальный master,
без push/fetch. [Config integration evidence](matrixgame-e2e519-config-integration-evidence.json)
содержит baseline/candidate compiler results, runtime-логи, точные удалённые
Windows unresolved symbols, source/binary hashes и protected-input checks.

Полная оригинальная `MatrixGame/src/MatrixConfig.cpp` перешла COMPILE_FAIL →
COMPILE_PASS и вошла в существующий matrixgame-native-cache/bootstrap.
Baseline первого отказа — `VK_LBUTTON` в static key table. Полная таблица теперь
использует исходные числовые key identifiers; Windows keyboard API не эмулируется.
Gamma использует typed GammaRamp и реальный Win32 SetGammaRamp provider.
Provider разрешает запись только для того же устройства с сохранённой исходной
ramp; положительный gamma runtime **NOT RUN**. Его отсутствие не заменено успехом.
GameDefaultsNative больше не дублирует SetDefaults/Clear/global g_Config:
все эти методы и глобальный объект теперь приходят из полной исходной TU.

**Новые реально исполненные методы:**

- `CMatrixConfig::ApplySettings:155` целиком переносит supplied settings в
  настоящий g_Config: flags, shadow selection, три gamma structures. Его тело
  побайтово совпадает с прежним оригинальным effective source.
- Оригинальный `CBlockPar::LoadFromTextFile` загружает настоящий
  `upstream/matrixgame/MatrixGame/CFG/robots/data.txt` (134462 bytes), без
  сгенерированного блока конфигурации. SHA-256:
  `9579962ecdf9eef7dfca4dc70a3c96cbec9aa0fd2db2347ccb76e8e400b7b27c`.
- `CMatrixConfig::ReadParams:334` читает Config и пять исходных Cursors,
  копирует key/value strings, flags, MaxFPS=100 и RGB gamma values. Точный
  следующий барьер — **строка 427 ApplyGammaRamp → ReadyDevice Unsupported**.
  Изменения g_Config и все пять пар сверены с настоящим загруженным деревом.
- ReadParams повторён дважды в каждом из двух полных owner sessions; после
  отказа сохранены CPU/cache/data/config ownership, затем выполнены cleanup/re-init.

ApplySettings проверяется как самостоятельный исходный CPU-метод в том же
bootstrap. Это не утверждение о выполнении производственной цепочки после
успешного ReadParams: **ReadParams целиком ещё не завершался**, post-gamma
damage/weapon/camera/labels/model parsing в этих host/QEMU прогонах не исполнялся.
GenRamp body побайтово совпадает с прежним; ReadParams восстанавливается в
прежний текст после обратной подстановки typed byte и cursor construction block.
Полный MatrixGame.cpp остался побайтово прежним.

NativeInitRuntime теперь владеет реальным HNew(g_MatrixHeap) CBlockPar и
публикует g_MatrixData только после успешной загрузки. Отсутствующий файл
освобождает неопубликованный объект; duplicate load сохраняет существующее
дерево. Чужой g_MatrixData вызывает отказ Begin и сохраняется после Deinit.
Собственные data уничтожаются до cache/game heap teardown, глобальный указатель
обнуляется. Это исходный fallback text-data путь; storage robots.dat/package path
и полный CGame::Init не запускались.

Для безопасных retry исправлен lifetime, а не игровой parser algorithm:

- Full Clear обнуляет освобождённый cursor pointer/count. Labels/descriptions
  освобождаются тем же Clear по исходным LABELS_LAST/DESCRIPTIONS_LAST и game
  heap; прежний guarded cleanup в неизменённом CGame::Deinit увидит null.
- Конструируются целые cursor string pairs, число живых пар обновляется до
  копирования текста; отказ копирования не оставляет destructor над неинициализированными
  строками. bad_alloc injection внутри string copy здесь NOT RUN.
- Nothrow default construction std::wstring проверено static_assert. Отдельные
  cleanup-only arrays labels/descriptions, заполненные исходными cursor strings,
  освобождены прямым повторным Clear в существующем bootstrap. Это **cleanup
  fixture**, не доказательство выполнения post-gamma parser tail.

| Gate | Подтверждено |
|---|---|
| Полная MatrixConfig.cpp | COMPILE_PASS по тому же compiler-audit рецепту, что baseline; это не selected config methods |
| Host ASan/UBSan + leak detection | PASS ApplySettings, настоящий data load, ReadParams до gamma refusal, retries, foreign/duplicate/missing-file ownership, repeated Clear и полный reentry |
| ARM64/QEMU | PASS того же scope на том же original data.txt; не Switch runtime |
| devkitA64 | COMPILE/LINK PASS; strong undefined=0, реальные ReadParams/ApplySettings/LoadGameData symbols присутствуют; runtime NOT RUN |
| Windows x86 | Полные CGame/config TU компилируются; LINK_BLOCKED, **97 вместо 99** symbols; новый runtime NOT RUN |

Из Windows undefined set удалены ровно
`?ReadParams@CMatrixConfig@@QAEXXZ` и
`?ApplySettings@CMatrixConfig@@QAEXPAUSRobotsSettings@@@Z`.
Новых unresolved symbols нет. Остальные 97, включая legacy L3G entry,
render/map/static-init/selection dependencies, перечислены в evidence.
Windows bootstrap пока не восстановлен; прежний selected-method PASS не
переносится на эту конфигурацию.

237 prepared engine files и все core outputs сверены по реальным байтам между
host, Windows, ARM64 и SDK, включая сохранённый audited view. Source/binary
hashes привязаны к implementation commit; сборки сделаны до commit с
build_dirty=true, без embedded Git SHA. SDK ELF SHA-256:
`8fde90f84d0a26eaf6896b070ea9c937168d3fd2a855818713262954c19de11d`.
Build id: `1f60cb373af27247e6b716d169267a2eb55a5bf4`.
Нового executable/probe и полного inventory не создавалось.

Все 14 protected hashes совпали; исходный config input и upstream неизменны.
Сейвы, ресурсы, основная NRO и frozen аппаратные комплекты сохранены.
Full Init/Run/map/battle и реальный GPU runtime остаются невыполненными;
Switch HARDWARE_PENDING. E2E-5.19 активен.

Следующие три приоритета:

1. Настоящий legacy L3G entry и согласование его с общим window/device owner;
   продолжить закрывать 97 Windows link dependencies.
2. Согласовать source Init/static-init/cache/data ownership и продвигать
   настоящий вызов CGame::Init без повторного создания уже живых владельцев.
3. Выполнить ReadParams за gamma-границей на настоящем провайдере, проверить
   реальные следующие ресурсы и reset-safe presentation lifecycle.

## Продолжение 23.10.2026: legacy ABI entry и единый 3G context

Implementation: `f2f78f09db567277f9c9099efae466e2c940281a`, локальный master,
без push/fetch. [Legacy entry evidence](matrixgame-e2e519-legacy-entry-evidence.json)
содержит runtime-логи, source/binary hashes, точную Windows link delta и
результат отдельной x64-конфигурации.

Определены настоящие ABI symbols `L3GInitAsDLL(...)` и `L3GDeinit()`, которые
вызывает неизменённый полный CGame.cpp. Вход обращается к тому же singleton
NativeInitRuntime: требуется активный CPU owner и зарегистрированные borrowed
Window/Device providers. Проверяются module/window identity и действительные
API/device identities провайдера **до** снятия текущих hook/cursor/timer/state
leases. Произвольные raw ABI arguments не разыменовываются.

Win32 ReadLegacyHandles получает API через настоящий GetDirect3D и освобождает
временный COM reference RAII; borrowed device продолжает держать API живым.
После успешной проверки тот же owner выполняет DetachDevice → AttachDevice,
то есть прежний перенесённый оригинальный DLL algorithm и реальный CaptureState.
Второй resource manager не создан. Ошибка attachment сохраняет CPU/cache/data
owner и откатывает device/window фазу. Config/sysname/caption остаются unused,
как и в исходном DLL implementation.

**Что реально исполнено сейчас:** global legacy ABI на host и ARM64/QEMU
проверил unregistered-provider отказ и явный no-SDK-identity отказ; затем
повторные L3GDeinit, cold provider rebind/retry и два полных owner sessions.
Проверены неизменные CPU/cache/data/game-heap owners и настоящий config input.
Это **не положительный device init**: код оригинального typed DLL algorithm
через новый entry в этих прогонах не достигнут. Windows assertions правильных
SDK identities, wrong-API/no-mutation, live reentry и реального cleanup
скомпилированы, но **NOT RUN** из-за оставшегося link blocker.

Legacy scalar lvalues теперь непосредственно ссылаются на поля единственного
platform3g::Current: g_HInst, g_Wnd, g_Flags, g_ScreenX/Y, g_AvailableTexMem.
Host/QEMU подтверждают равенство их адресов адресам native state fields.
Flags видны тем же исходным call sites и typed WndProc. ScreenX/Y означают
configured dimensions, не измеренный display mode; AvailableTexMem — последнее
действительное provider query, не RAM и не придуманный GPU budget. Dimensions
и этот cached query сохраняются при Deinit, как исходные globals.

В prepared 3g.hpp объявления используют shared references; определения этих
скаляров в prepared полном 3g.cpp становятся matching extern declarations.
SDK pointers/caps objects не создаются и не подменяются. Полная TU 3g.cpp
по-прежнему не считается портированной. Её исходные InitAsDLL/Deinit bodies,
generated original typed DLL algorithm, полный CGame.cpp и полный Config.cpp
побайтово не изменены относительно предыдущего этапа; изменено только владение
скалярным состоянием и native ABI dispatch.

| Gate | Подтверждённый результат |
|---|---|
| Host ASan/UBSan + leak detection | PASS legacy ABI/provider refusal, общие scalar lvalues, retained real owners, repeated deinit/rebind и полный cleanup/reentry |
| ARM64/QEMU | PASS того же scope с настоящим data.txt; не Switch runtime |
| devkitA64 | COMPILE/LINK PASS, strong undefined=0; global legacy ABI и shared state symbols присутствуют; runtime NOT RUN |
| Windows x86 | Полные CGame/config/entry/provider objects компилируются; LINK_BLOCKED **92 вместо 97** |
| Windows x64 | Те же полные TU компилируются; LINK_BLOCKED **92**; положительный SDK runtime нового пути NOT RUN |

Из x86 undefined set удалены ровно L3GInitAsDLL, L3GDeinit, g_Flags и g_ScreenX/Y;
новых unresolved symbols нет. Оставшиеся 92 перечислены в evidence. Во время
отдельного x64 setup обнаружено и исправлено локальное libpng CRT/policy
несовпадение. Финальные x64 errors — зависимости движка, без CRT errors.
Переход x86 → x64 не закрыл engine dependency graph; повторять эту архитектурную
проверку без новых изменений не требуется. Отдельный CPU/GPU executable не создан:
использован тот же existing bootstrap target и те же оригинальные source views.

Все core outputs и 237 prepared engine files проверены по реальным байтам
между host, ARM64, SDK, Windows x86 и x64. Binaries построены до commit,
build_dirty=true, embedded Git SHA отсутствует; source hashes привязаны к
implementation commit. SDK ELF SHA-256:
`75465306867c77113ed21d908fe1243c02e844c59c41736373ed65b2de5e24b0`.
Build id: `c054d7fea98184cc5a262e2674fd84203ea92191`.
Windows executable как успешный кандидат не публиковался.

Все 14 protected SHA совпали; saves/resources/upstream/frozen аппаратные архивы
сохранены. Основная NRO не пересобиралась, общего inventory и больших standalone
suites не было. Full CGame::Init/Run/map/battle остаются NOT EXECUTED,
Switch/GPU HARDWARE_PENDING. E2E-5.19 активен.

Следующие три приоритета:

1. Закрывать реальные static-init/render/map/loop зависимости полного CGame
   object, начиная с исходных определений владельцев и методов; Windows link
   остаётся заблокирован 92 символами.
2. Согласовать исходный Init/static-init/cache/game-heap/data порядок с одним
   owner перед настоящим вызовом CGame::Init; повторная инициализация уже живых
   владельцев недопустима.
3. Проверить положительный legacy SDK entry после восстановления Windows link,
   затем реальную gamma/presentation/Reset границу и следующие ресурсы.

## Продолжение 23.10.2026: настоящая production static_init

Implementation: `08d05eb711f5de5472944ed2b7cf6f30c9aa5085`, локальный master,
без push/fetch. [Static-init evidence](matrixgame-e2e519-static-init-evidence.json)
содержит все 54 исходных определения с origin/file/line/hash, runtime-логи,
полную link delta и remaining method errors.

Оригинальная Release `MatrixGame.cpp:138 static_init` теперь реально исполняется
из полной исходной TU через небольшую wrapper-функцию в той же TU. Тело самой
static_init и тело полного CGame::Init побайтово совпадают с предыдущим source
view. Не удалялись классовые initialization calls и не добавлялись методы-заглушки.
Новый executable/probe не создавался.

Для этого перенесены **54 настоящих определения данных** из их оригинальных
prepared CPP: shadow/map/progress/flyer/water/terrain/skin/selection static state,
interface и DLL pointers/counters. GameStaticResources.cpp содержит точные
прежние effective definitions, включая ранее принятые portable resource pointer
типы. Новые нулевые значения не придумывались: сохранены исходные декларации,
array extents и initializers. Старые определения убраны из prepared original TU
(для globals оставлены extern declarations), чтобы последующее подключение
полных CPP не создавало второй экземпляр данных. Upstream не менялся.
Production preparer использует явный source manifest, не тестовый linker report.

**Новая фактически исполненная цепочка:** один NativeInitRuntime → пустой
native staging context → оригинальная static_init/CMain::BaseInit → game heap
→ CacheInit → CPU platform binding → прежняя VO/config/legacy error-chain.
Статическая стадия выполняется до game/cache allocations, как в исходном входе.
В конце owner освобождает свои ресурсы и вызывает CMain::BaseDeInit.

В тесте перед Begin оставляется настоящий GFLAG_EXITLOOP от прежнего loop state.
Исходная static_init очищает его; после Begin проверено OriginalStaticsReady,
после двойного Deinit — снятие этого состояния. Два полных sessions проходят
на host ASan/UBSan с leak detection и ARM64/QEMU на тех же настоящих VO/PNG/data.txt.
В существующем bootstrap также создана и зарегистрирована живая чужая CForm:
новый Begin отказывается до static reset, сохраняя g_FormFirst/Last/Cur и форму.
BaseInit вызывает исходные CFile::StaticInit и SRemindCore::StaticInit; registry
пакетов не обходится фиктивным успехом (real ReleasePackFiles отказывает при
активных package files). Debug resource/helper/tracer ветви здесь не проверялись.

| Gate | Подтверждено |
|---|---|
| Host ASan/UBSan + leak detection | PASS реально исполненной original static_init, stale EXITLOOP reset, foreign-form refusal, source-data/VO/config regressions, cleanup и два reentry |
| ARM64/QEMU | PASS той же production-stage/CPU scope; не Switch runtime |
| devkitA64 | COMPILE/LINK PASS, strong undefined=0; InitializeOriginalGameStatics retained; runtime NOT RUN |
| Windows x86/x64 | Полные CGame/config/shared-data TU компилируются; LINK_BLOCKED **38 вместо 92** |

Из x86 undefined set удалены ровно 54 data definitions. Новых unresolved нет;
в текущем remaining set — 38 функций, а не недостающие данные. Это закрытие
прямых data references текущей связки CGame, не утверждение о всей зависимости
всех ещё непортированных engine CPP. Положительные SDK/WinMM/gamma/device
проверки новой Windows-конфигурации по-прежнему NOT RUN. Full-engine link/GPU
runtime не объявляются на основании ELF section garbage collection.

**Ограничение перед следующим настоящим Init:** исходный CGame::Init пока
по-прежнему сам вызывает static_init и создаёт heap/cache/data. Его нельзя
без согласования вызвать поверх уже живой Begin-session: получатся повторная
инициализация и конфликт владельцев. Сейчас исполнена именно оригинальная
промежуточная static stage; полный Init ещё NOT CALLED. Его алгоритм не вырезан
для обхода оставшихся link dependencies.

По производственному порядку ближайший unresolved constructor —
`CLoadProgress lp` на исходной строке 186, после game heap и package opening.
Первый напечатанный linker error (например InitEffects) не означает первую
runtime-операцию. CLoadProgress methods, renderer/map/interface/history,
selection и L3GRun перечислены в evidence; package/storage/map loading через
полный Init ещё не исполнялось.

238 prepared engine files и все core outputs сверены по реальным байтам между
host, ARM64, SDK и Windows x86/x64. Binaries собраны до commit с build_dirty=true,
без embedded Git SHA; evidence связывает их с точным implementation SHA.
SDK ELF SHA-256:
`84d35229c582e3562fa1f8c077f085225796f4b4915ecb4ca34c0e0d6bcd2ae3`.
Build id: `5d5f260af4eaca64835baf8edd406ffeedb59d9e`.
Все 14 protected SHA совпали; saves/resources/upstream/frozen аппаратные
комплекты сохранены. Основная NRO не пересобиралась; общего inventory и больших
standalone suites не было. Switch/GPU HARDWARE_PENDING. E2E-5.19 активен.

Следующие три приоритета:

1. Портировать настоящие CLoadProgress и последующие initial-entry методы по
   порядку CGame::Init; закрывать оставшиеся 38 функциональных dependencies.
2. Согласовать полный source Init/static-init/game heap/cache/data с единым
   owner перед настоящим вызовом полного Init.
3. После восстановления Windows link проверить положительный legacy device
   lifecycle, затем gamma/presentation/Reset и настоящие map/form/loop ресурсы.

## Продолжение 23.10.2026: полная CLoadProgress и время жизни публикации

Implementation: `336d038881efad1cd24c871c5c5a4b65af1a34ca`, baseline:
`52ec42356992941979d9ac1f16d4cdd38704cbaa`. Локальный master, без push/fetch.
[Evidence](matrixgame-e2e519-load-progress-evidence.json) содержит команды,
runtime-логи, source/binary hashes и полную разницу unresolved symbols.

В существующий bootstrap включена **полная MatrixLoadProgress.cpp**. Constructor,
SetCurLP, SetCurLPPos и таблица весов 70/300/150/30 сохранены без изменения
алгоритма. Эта TU компилировалась и в baseline: COMPILE_PASS → COMPILE_PASS;
новый результат — реальная компоновка и исполнение её методов в общей session,
а не исправление ранее отсутствовавшей compile error. Новый executable не создан.

Обнаружен конкретный lifecycle дефект: CGame::Init публикует адрес локального
`CLoadProgress lp` в g_LoadProgress, а прежний пустой destructor оставлял этот
адрес после выхода/unwind. Теперь destructor обнуляет global **только если он
указывает на данный объект**. В native owner добавлены создание/завершение
progress после CPU/static stage и освобождение до остальных ресурсов Deinit.
В header лишь квалифицирован настоящий Base::CMain и объявлен destructor;
новых полей нет. Полная CGame::Init не менялась и по-прежнему NOT CALLED.

В двух bootstrap sessions исполнены исходный расчёт пяти callback-значений,
финальное 1.0, подавление повторного/регрессивного значения, null-interface без
последующего replay, отказ при чужом/дублированном publisher, сохранность живой
публикации при уничтожении другого объекта и stack unwind при исключении из
callback. После Finish/Deinit проверены отсутствие опубликованного объекта и
повторный вход. Используется настоящая структура SMGDRangersInterface с
контролируемым callback fixture; **реальная Rangers UI не проверялась**.

| Gate | Результат |
|---|---|
| Host ASan/UBSan + leak detection | PASS новых методов, callback/lifetime и двух общих sessions |
| Linux ARM64/QEMU | PASS того же CPU scope |
| devkitA64 | COMPILE/LINK PASS, strong undefined=0; runtime NOT RUN |
| Windows x86/x64 | C++ compile без ошибок, LINK_BLOCKED **35 вместо 38**; runtime NOT RUN |

Удалены ровно три прежних unresolved: constructor CLoadProgress, SetCurLP и
SetCurLPPos. Новых unresolved нет. ELF section GC по-прежнему исключает
незамкнутые методы полного Init/Run: SDK link PASS не означает full-engine link.
Оставшиеся зависимости относятся к renderer/map/interface/selection/loop,
effects/history и camera entry helper; полный список сохранён в evidence.

Сверены 238 prepared files и core outputs всех пяти конфигураций по реальным
байтам. Candidate effective manifest SHA-256:
`5df55b4be569330a2201ece044c500657dd5a0612cad40cc25042072863c1c75`.
SDK ELF SHA-256:
`e87b3a71340b7e9204a7045033d5b5f3bceb3bd6047ee48d568d2a6ea4b13c7e`,
build id `ce62cb0977fb749391c01988dd2d8806366d1956`, 4621888 bytes.
Сборки выполнены до commit с build_dirty=true, embedded Git SHA отсутствует;
evidence связывает точные source hashes с implementation SHA.

Все 14 protected hashes совпали; upstream pin и пользовательские данные
сохранены. Основная NRO и оба аппаратных комплекта не менялись. Общий inventory
и старые standalone suites не повторялись. GPU/Switch HARDWARE_PENDING.

Следующие три приоритета:

1. Закрыть следующие реальные вызовы original Init и renderer/map зависимости;
   CLoadProgress больше не является unresolved initial-entry constructor.
2. Согласовать исходный Init/static-init/heap/cache/data с единым owner перед
   вызовом полного Init: вызов поверх живой Begin-session пока недопустим.
3. После восстановления Windows link исполнить положительный legacy device
   lifecycle и определить следующую настоящую map/form/loop границу.

E2E-5.19 остаётся активным: полный Init, карта, loop и бой ещё не исполнены.

## Продолжение 23.10.2026: полная TU рендера и SDK state lifecycle

Implementation: `dafcb151bc461d9da7323209a1ec0271dac5589d`, baseline:
`7d56260c6e0b11698557b5c09b1ce99c07842f2d`. Локальный master, без push/fetch.
[Renderer evidence](matrixgame-e2e519-render-pipeline-evidence.json) содержит
полные ошибки Windows, source/object/binary hashes, границы исполнения и логи.

Снят следующий **реальный compile blocker**: полная исходная
MatrixRenderPipeline.cpp раньше останавливалась на incomplete IDirect3DDevice9
в первом WaterAlpha_t3/SetRenderState (prepared baseline:103). Теперь TU целиком
компилируется на host, Linux ARM64 и devkitA64. Сохранены все оригинальные
water/terrain функции, ветвления, число проходов и assignments callback table
конструктора. Алгоритмы не заменены указателями на заглушки. Header отличается
только квалификацией настоящего Base::CMain; полная CGame::Init не менялась.

Оригинальные ordinary SetColorOp/SetAlphaOp helpers взяты из прежнего полного
3g source view и адаптированы к тому же typed device; это выбранные оригинальные
inline algorithms, **не** перенос всей 3g.cpp. DO_SMART_COLOROPS явно отклоняется
при компиляции, а не подменяется ordinary branch. ASSERT_OFF сохраняет прежнюю
unchecked семантику; обычные ASSERT_DX/FAILED_DX проверяют тот же result !=0.
Все 45 используемых serialized value/register identifiers сверены с настоящим
установленным Windows SDK d3d9types.h (точный перечень/количество в JSON).

Конструктор использует отдельное фактическое MaxSimultaneousTextures из SDK
GetDeviceCaps. Оно не заменяется MaxTextureBlendStages, уже необходимым другим
стадиям. Для texture binding, FVF и texture matrices Windows provider вызывает
настоящие SetTexture/SetFVF/SetTransform. CPU staging vertex/index buffers
остаются typed resources; их загрузка и binding отклоняются явным Unsupported,
без reinterpret_cast в COM buffer и без фиктивного успешного GPU результата.

Для новых операций расширен существующий StateLease: явное сохранение FVF,
AddRef vertex declaration, девяти texture-stage registers для восьми stages и
восьми texture matrices. После stateblock Apply эти значения восстанавливаются
явно, вместе с прежними 207 stored states, world и texture bindings. COM-ссылки
texture/declaration освобождаются в destructor, включая частичный capture.
Новые setters требуют уже захваченную lease; изменение layout/matrix без неё
не допускается. Reset lifecycle по-прежнему не объявляется поддержанным.

В **существующий** Windows bootstrap добавлены реальные SDK проверки обоих caps,
отказа без capture, изменения/readback и восстановления FVF/declaration,
девяти stage values, texture matrix и исходного texture binding за два входа.
Они компилируются на x86/x64, однако **NOT RUN**: общая компоновка ещё закрыта
не полностью. Ни state restore, ни выбор renderer callbacks не получили
нового runtime PASS на основании одной компиляции.

| Gate | Новое доказательство и ограничение |
|---|---|
| Host full renderer object | COMPILE_FAIL → COMPILE_PASS под существующими ASan/UBSan flags |
| Linux ARM64 full renderer object | COMPILE_PASS, отдельный адресный object audit |
| devkitA64 full renderer object | COMPILE_PASS, CMake/Ninja object target, без runtime |
| Windows x86/x64 | Полная renderer TU включена в native-cache; LINK_BLOCKED **33 вместо 35** |
| Host bootstrap ASan/UBSan + leak detection | PASS прежних реально исполняемых стадий, cleanup/reentry; renderer NOT EXECUTED |
| ARM64/QEMU bootstrap | PASS прежней CPU цепочки; renderer NOT EXECUTED |
| devkitA64 bootstrap ELF | COMPILE/LINK PASS, strong undefined=0; renderer отсутствует в CPU archive/runtime |
| Switch/GPU, Windows новый state readback | HARDWARE_PENDING / NOT RUN |

Windows delta — ровно original CRenderPipeline constructor и destructor; новых
undefined нет. Это реальная интеграция целой TU в общую Windows связку CGame,
но ещё не исполнение конструктора и не full-engine link. Остались 33 функции
renderer/map/interface/selection/loop/effects/history/camera dependencies.

**Честная граница host-интеграции:** пробное включение рендера в общий bootstrap
с вызовом original constructor не прошло link. ASan/UBSan metadata требует
настоящие `typeinfo for CBaseTexture`, `typeinfo for CTextureManaged` и
`typeinfo for CMatrixMapObject`; реализации этих исходных классов ещё не
замкнуты. Уже работающий CacheTexture является отдельным CPU resource contract,
его нельзя выдать за оригинальный CTextureManaged. Вызов constructor поэтому
не оставлен в рабочем bootstrap. Кандидат owner/test integration сохранён
локально в `build/matrixgame/e2e519/render-owner-attempt.patch`, ошибки —
`render-host-build.log` и `render-host-final-build.log` этого же каталога.
vptr/ASan/UBSan не отключались и фиктивный RTTI не добавлялся.

Отсюда намеренное различие build recipes: Windows native-cache компилирует
полную renderer TU; host/ARM64/SDK исполняемый CPU bootstrap её пока не включает,
а полная TU проверяется отдельным object target **того же исходного движка**.
Новый executable или standalone CPU/GPU prototype не создавался. Одинаковые
effective files не означают одинаковый состав связанного бинарного кода.

Адресные команды новой стадии (Linux команды запускаются из WSL repo root):

```text
python3 tools/matrixgame_engine_compile_audit.py --build-root build/matrixgame/e2e519/host --output build/matrixgame/e2e519/render-candidate-audit --source /MatrixRenderPipeline.cpp --jobs 1
python3 tools/matrixgame_engine_compile_audit.py --build-root build/matrixgame/e2e519/arm64 --output build/matrixgame/e2e519/render-arm64-audit --source /MatrixRenderPipeline.cpp --jobs 1
cmake --build build/matrixgame/e2e519/sdk/cmake --target CMakeFiles/matrixgame-native-engine.dir/source/MatrixGame/src/MatrixRenderPipeline.cpp.obj --parallel 2
cmake --build build/matrixgame/e2e519/windows/engine --config Release --target matrixgame-native-bootstrap -j6
cmake --build build/matrixgame/e2e519/windows-x64/engine --config Release --target matrixgame-native-bootstrap -j6
```

238 prepared engine files и core outputs сверены по реальным байтам между
всеми пятью конфигурациями. Новый effective manifest SHA-256:
`0d278a94cd6822da01ffa1b4767d00c6609da2fca5f3ef9895f4948e81ab0fec`.
SDK ELF SHA-256:
`ea33563b346ea02f7935328bbdf41b97d62306954615a84bf4aba2d33196acff`,
build id `a71f9f60ee6173acd7be168d6557a3f7fcc04eeb`, 4622648 bytes.
build_dirty=true перед commit; embedded Git SHA отсутствует, точный commit
связан с исходными hashes в evidence. Команды адресных object audits и builds
сохранены в JSON audit/sdk results. Общий inventory не повторялся.

Все 14 protected hashes совпали. Сейвы, ресурсы, pinned upstream и оба
аппаратных комплекта сохранены; основная NRO не пересобиралась.

Не более трёх следующих приоритетов:

1. Замкнуть настоящие texture/map-object resource и RTTI зависимости, затем
   исполнить original renderer constructor в общем owner без ослабления UBSan.
2. Закрывать оставшиеся зависимости полной Windows CGame связки; запустить
   уже скомпилированные SDK state/caps readback проверки после восстановления link.
3. Согласовать полный Init/static-init/heap/cache/data lifecycle перед вызовом
   исходного CGame::Init поверх единого владельца.

Новая достигнутая граница — компиляция полной TU и снятие двух Windows link
зависимостей. **Граница исполнения пока прежняя:** static_init → heap/cache/CPU
binding → progress/VO/config prefix; полный Init, renderer ctor, map/form/loop
и бой не исполнены. E2E-5.19 остаётся активным.

## Продолжение 23.10.2026: оригинальный CTextureManaged в общем owner

Implementation: `0670bad8e549edbc684860f34e96f9b28bd73445`, baseline:
`2e69352f8fa7ccfc1d12ba658882be13c2f5dc70`. Локальный master, без push/fetch.
[Original-texture evidence](matrixgame-e2e519-original-texture-evidence.json)
содержит хэши всех 14 методов до/после, runtime-логи, реальную команду relink,
remaining errors, compiled-input hashes и protected hashes.

В native-cache добавлены **выбранные оригинальные методы** из Texture.cpp и
ранее inline Texture.hpp: CBaseTexture constructor/destructor/ParseFlags и
LoadTextureFromFile; CTextureManaged Init/Load/Unload/destructor/Preload,
CreateLock/LockRect/UnlockRect и два bitmap overloads. Это **не full Texture.cpp
port**. Используется исходный production USE_DX_MANAGED_TEXTURES branch.
Обычный default-pool CTexture и полный texture lost/reset path ещё не перенесены.

Constructor/destructor CBaseTexture и ParseFlags сохранены без изменения тела.
Настоящий исходный `_textures` registry принадлежит тому же модулю. Pixel/mip
циклы обоих bitmap методов побайтово сохранены (отдельные loop hashes в JSON).
GPU API обращения заменены типизированной границей; native LockedRect/LevelDesc
являются значениями, а не поддельными SDK structs. Windows bridge копирует поля
настоящих SDK значений без предположений об их layout. В full source view
сохраняются те же адаптированные определения выбранных методов, чтобы при
последующем переносе не возникала вторая реализация с другой семантикой.

Оригинальный временный texture при to16 PNG-конверсии теперь имеет RAII cleanup:
при исключении CBitmap/CreateTexture он уничтожается через CCache::Destroy.
Исходный код оставлял explicit Destroy после потенциально бросающих операций.
Новый fault path действительно исполнен без SDK, registry после unwind
возвращается к прежнему размеру. Также добавлены явная ошибка вместо null
dereference при неудачном decode и освобождение texture при ошибке GetLevelDesc;
эти положительные/отрицательные SDK ветви пока runtime NOT RUN.

NativeInitRuntime получил отдельный original-texture slot с корректной парой
HNew/CCache::Destroy. Успешный Init публикуется владельцу только после загрузки;
при исключении временный оригинальный объект уничтожается. Cleanup выполняется
до borrowed device/window/cache/heap teardown, Finish и Deinit идемпотентны.
Begin отказывает при живой чужой оригинальной texture **до** static reset;
чужой объект и его registry entry сохраняются. Это проверено в том же bootstrap.

**Новая реально исполненная цепочка:** прежний NativeInitRuntime/CPU owner →
оригинальные CBaseTexture/CTextureManaged constructors → ParseFlags (включая
Alpha/Trans flags) → Load/Init → оригинальный CFile/CBuf encoded-file read →
`original texture DecodeTexture: 3G device negotiation not complete`.
Для to16: оригинальное разрешение PNG → создание настоящего временного
CTextureManaged → CBitmap::LoadFromPNG → original LoadFromBitmap →
`original texture CreateTexture: 3G device negotiation not complete`.
Это явный отказ GPU backend, не фиктивно успешная texture загрузка.

Проверены два retry, отсутствие m_Tex после отказа, missing-file отказ до GPU
границы, registry 0→1→0, cleanup временной конверсии, отказ Native owner Init без
оставшегося slot/registry entry, repeated Finish/Deinit, foreign-object refusal
и два общих sessions. Pixel conversion loops, Lock/Unlock/Preload и создание
настоящего SDK resource на host/QEMU не исполнялись.

Windows backend использует реальные CreateTexture/GetLevelDesc/LockRect/
UnlockRect/PreLoad/Release и настоящий borrowed SDK device. Encoded загрузка
применяет уже существующие TextureAsset Decode/PrepareMips и настоящую загрузку
в SDK texture, включая RGBA→BGRA packing и BC1/2/3 payloads. **D3DX/GPU pixel/mip
parity не подтверждена.** В существующий Windows bootstrap добавлены actual
resource description/retention и 16-bit pixel readback, cleanup и reentry;
проверки компилируются, но NOT RUN из-за 33 unresolved полного CGame.

Важно: исходный CTextureManaged создаётся настоящим typed HNew factory и
принадлежит NativeInitRuntime. Ранее принятый CPU CacheTexture/MaterialTexture
factory остаётся отдельным контрактом. Он не трактуется как CTextureManaged;
полная миграция cache factory для map/material clients ещё не выполнена.

| Gate | Подтверждение |
|---|---|
| Host ASan/UBSan + leak detection | PASS нового original texture CPU-prefix, registry/ownership/failure cleanup и двух sessions |
| Linux ARM64/QEMU | PASS той же реально исполненной цепочки |
| devkitA64 | COMPILE/LINK PASS общего bootstrap, strong undefined=0; SDK texture runtime NOT RUN |
| Windows x86/x64 | Новые original methods/backend/readback tests компилируются; LINK_BLOCKED 33, новых unresolved нет |
| Full renderer relink под ASan/UBSan | Две настоящие texture RTTI зависимости сняты; остался только `typeinfo for CMatrixMapObject` |
| Renderer ctor, map/form/loop/battle, Switch/GPU | NOT EXECUTED / HARDWARE_PENDING |

Для relink использованы реальные test objects общего bootstrap, его библиотеки
и полный renderer object с теми же sanitizer flags. Точная команда и единственный
undefined сохранены в evidence. Исходный renderer object повторно не компилировался
после проверки совпадения effective-source manifest и recipes; ранее созданный
адресный object audit использован повторно. Запрос повторного audit в уже занятый
каталог был отклонён инструментом без перезаписи старого evidence. Исполняемый
артефакт из неудачной relink попытки не получен; новый prototype/target не создан.
RTTI/vptr проверки не отключались. Тела полного CGame и renderer CPP не менялись.

239 effective engine files, core outputs и реальные bytes всех пяти source views
сверены. Native cpp/header inputs дополнительно сверены с implementation commit
с нормализацией лишь CRLF/LF; оба вида hashes сохранены. Effective manifest SHA:
`66a946a1c515bfd049ab36ae40826dc5271b6b4296e1e6ffaf6afdaa0a1428fc`.
SDK ELF SHA-256:
`8429269525641dc7233d5f5454289e3339d147b5349265daa1036994d1bfe2dc`,
build id `ed53c008c97def9a78f4236a63532ba9d3a4a774`, 4654056 bytes.
build_dirty=true перед commit; embedded Git SHA отсутствует. Сборки запускались
только для существующего matrixgame-native-bootstrap; отдельный renderer object
audit/relink проверял конкретную новую ресурсную зависимость. Общий inventory
и старые standalone suites не повторялись.

Все 14 protected hashes совпали: пользовательские saves/config/logs, ресурсы,
архивы аппаратных комплектов и pinned upstream сохранены. Основная NRO не
пересобиралась и новый NRO не упаковывался.

Следующие три приоритета:

1. Замкнуть оригинальный CMatrixMapObject resource/type closure и исполнить
   constructor рендера в общем owner: в проверенной relink остался только его RTTI.
2. Закрывать оставшиеся Windows CGame dependencies, выполнить скомпилированные
   SDK texture/state readback проверки и согласовать настоящие cache factories.
3. Согласовать полные Init/static-init/heap/cache/data действия перед настоящим
   вызовом CGame::Init; карта, form, loop и бой пока не исполняются.

E2E-5.19 остаётся активным. Новая граница исполнения — настоящие managed texture
методы и чтение/bitmap-prefix до первой неподдержанной GPU операции, с проверенным
освобождением ресурсов; полного запуска игры ещё нет.

## Продолжение 23.10.2026: настоящий вход в constructor рендера

Implementation: `09975e98ee6b5255c2be1de55f1dfdf3b8276ee6`, baseline:
`39408a552bf37aec9f46d7a384bb69a5b2c4327c`. Локальный master, без push/fetch.
[Renderer-entry evidence](matrixgame-e2e519-renderer-entry-evidence.json)
содержит runtime-логи, проверку reference scope, source/binary hashes,
Windows errors и текущую доступность защищённых файлов.

**Уточнение предыдущего вывода о первом блокере:** сам CMatrixMapObject не
требуется активному constructor CRenderPipeline. Остаточная RTTI-ссылка шла из
`obj_ordinal_t4` и `obj_ordinal_gloss_t4` в MatrixRenderPipeline.cpp. В pinned
исходниках обе функции не имеют header/extern/export/call references вне этой
TU; после исключения комментариев единственное вхождение имени — определение.
Все назначения их в object callback table закомментированы уже в оригинале.
Ссылка появилась из-за удержания неиспользуемых внешних functions/UBSan metadata.
Начинать полный перенос map-object/shadow поведения только ради этого не нужно.

Обе функции получили `static` linkage. **Тела полностью сохранены.** Обратное
удаление ровно двух добавленных `static` даёт побайтово прежний prepared CPP;
constructor, destructor, callback assignments и остальные методы не менялись.
Объявления DLL API не затронуты. Ничего не заменено заглушкой и RTTI не
сфабрикован; sanitizer flags/проверки не отключались. Если позднее реальный
in-TU caller/назначение этих функций будет включён, обычная компиляция снова
потребует настоящие зависимости их классов. **CMatrixMapObject не перенесён.**

После этого полная MatrixRenderPipeline.cpp включена в native-cache на **всех**
платформах, вместо прежнего Windows-only включения. Нормальная host компоновка
общего bootstrap теперь проходит; в объекте и итоговом бинарнике есть настоящий
constructor/destructor рендера, отсутствуют две неиспользуемые функции и RTTI
ссылка на MapObject. В devkitA64 ELF constructor/destructor также реально
retained. Это закрытие текущей renderer TU, а не full-engine link.

NativeInitRuntime владеет renderer через исходную пару HNew(game heap)/HDelete.
g_Render публикуется только после успешного constructor. Finish снимает только
собственную публикацию, освобождает объект до device/texture/cache/heap teardown
и допускает повторные вызовы. Исходный полный CGame::Init не менялся.

**Реальная новая граница исполнения на host/QEMU:** NativeInitRuntime →
InitializeRenderPipeline → HNew(g_MatrixHeap) CRenderPipeline → исходный memset →
первый MaxSimultaneousTextures prerequisite → ReadyDevice →
`3G device negotiation not complete`. Constructor **вызван, но не завершён**.
После исключения matching placement delete освобождает память, g_Render и slot
остаются null; CPU owner сохраняется. Проверены два retry в каждом из двух
sessions, repeated Finish/Deinit и прежние реальные texture/config/cache стадии.
Доступные caps или успешная callback table на host не имитируются.

В существующем Windows bootstrap подготовлен положительный путь: реальные SDK
caps, исходные tiers числа water/terrain passes, непустые callback slots,
duplicate-owner refusal, вызов **оригинального TerBotClear** через таблицу →
реальные SDK SetRenderState/GetRenderState, Finish/reentry и удержание renderer
до настоящего DetachDevice/Deinit. При фактических caps<2 проверяется исходный
отказ карты. Эти проверки компилируются, но **NOT RUN** до закрытия Windows link.

| Gate | Доказано |
|---|---|
| Host ASan/UBSan + leak detection | PASS настоящего входа в constructor, первого device refusal, game-heap unwind/retry и двух общих sessions |
| Linux ARM64/QEMU | PASS того же реально исполненного CPU-prefix |
| devkitA64 | COMPILE/LINK PASS с retained renderer constructor/destructor, strong undefined=0; runtime NOT RUN |
| Windows x86/x64 | Новые constructor/callback/SDK-readback проверки компилируются; LINK_BLOCKED 33, без новых unresolved |
| Успешный renderer constructor/table, callbacks, GPU | NOT EXECUTED / HARDWARE_PENDING |
| Полный CGame::Init, map/form/loop/battle | NOT CALLED / NOT EXECUTED |

Проверялся один existing bootstrap candidate. Новый executable/prototype не
создан; полная compile inventory и старые standalone suites не повторялись.
Основные команды прежние: `cmake --build build/matrixgame/e2e519/host --target
matrixgame-native-bootstrap -j6`, запуск с ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
и UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 на прежних model-1.vo/plain.png/
pinned data.txt; тот же driver через qemu-aarch64 -L /usr/aarch64-linux-gnu.
SDK — matrixgame_init_sdk.py --output build/matrixgame/e2e519/sdk --bootstrap --resume.
Полные SDK commands и runtime texts сохранены в evidence.

239 prepared files и core outputs сверены по фактическим байтам между всеми
пятью конфигурациями. Generated изменение только одно — renderer CPP с двумя
добавлениями linkage; original managed-texture CPP и полный CGame CPP неизменны.
Effective manifest SHA-256:
`36571c58fd4c0637dd71ca2515e115c9da11ed75f831540301a1ecbcf51cad8e`.
SDK ELF SHA-256:
`f45f7944c0ce89ba5423165e34e646fe44598d4babd4cf63b4a3162d70163b87`,
build id `3ec51b3b3cad925c0b78c99d9c3dacb0935792c7`, 4699632 bytes.
build_dirty=true перед commit; embedded Git SHA отсутствует. Native inputs
сверены с implementation commit с нормализацией только CRLF/LF.

**Сохранность:** все **13 доступных** защищённых файлов совпали с baseline,
включая saves/config/logs, реальные VO/PNG, оба frozen аппаратных комплекта.
Pinned upstream чистый; основная NRO не пересобиралась, новый NRO не создавался.
`G:\pack.zip` сейчас недоступен: текущий SHA повторно не измерен. Последний
подтверждённый SHA `f2ea785ad8f1ca35288132a2fee211ac0e6e7bd2db00cff35f4a1fbc2cfed51f`
сохранён в предыдущем original-texture evidence. Полное сравнение 14/14 сейчас
**не объявляется PASS**; файл не переносился и не восстанавливался.

Следующие три приоритета:

1. Закрывать реальные зависимости полного CGame по порядку Init (включая
   SetMaxCameraDistance в MatrixCamera.cpp и последующий map/interface entry),
   затем исполнить подготовленные положительные SDK renderer/state проверки.
2. Согласовать настоящие map/interface/resource/cache factories, сохраняя
   различие исходного CTextureManaged и принятого CPU CacheTexture.
3. Согласовать Init/static-init/heap/cache/data ownership перед вызовом полного
   оригинального CGame::Init. Повторно инициализировать живой owner нельзя.

E2E-5.19 остаётся активным. Новый результат — полный renderer в общей связке
и фактический вход в его constructor до первого отсутствующего устройства;
успешной инициализации рендера и запуска игры ещё нет.

## 23.10.2026 — оригинальная камера как prerequisite карты

Implementation commit: `ae07595c364e78d70709c71dbd83763fb4cf5afd` в локальном
master, baseline `4ab7cffe602de4fc8b7281fd952a9271b40f581c`. Без push/fetch.
Evidence: `reports/matrixgame-e2e519-camera-entry-evidence.json`.

В исходном `MatrixGame.cpp:307–329` задаются configured dimensions, затем
ApplySettings/sampler, SetMaxCameraDistance и создаётся CMatrixMapLogic. В
`MatrixMap.hpp` камера является настоящим member карты; её constructor находился
в ещё не замкнутой MatrixCamera.cpp. В существующий общий bootstrap включены
пять выбранных исходных методов: SetMaxCameraDistance, CMatrixCamera ctor/dtor,
SAutoFlyData::Release/KillTrajectory как обязательные зависимости destructor.
Это **не полная Camera TU**. Полная prepared MatrixCamera.cpp сохранена без
изменений; selected TU исключена из full-engine inventory во избежание дублей.
Алгоритмы всех пяти методов сверены с pinned original: единственная замена в
constructor — ZeroMemory/D3DVIEWPORT9 на уже существующие ZeroTrivialBytes/Viewport.
Новые поведения autofly не добавлялись; его ненулевая ветвь **NOT EXECUTED**.

`NativeInitRuntime::InitializeMapCamera` требует текущего CPU/config owner,
проверяет положительные configured dimensions и конечный far plane >1 до
исходных делений. Использует реальные g_ScreenX/Y и SetMaxCameraDistance,
затем HNew(g_MatrixHeap) CMatrixCamera. При исключении восстанавливает прежние
глобальные значения; duplicate owner отклоняется. FinishMapCamera вызывает
исходный destructor и HDelete; Deinit освобождает камеру до config/heap.
После успеха configured globals сохраняются, как в оригинальном движке.
Физическое разрешение или device caps этим не измеряются и не имитируются.

Реально исполнено в двух sessions общего driver: реальные SetDefaults →
SetMaxCameraDistance(25), far=5000 → оригинальный CMatrixCamera ctor → исходные
LerpAng/LerpDist, обратные размеры и LH projection → исходный destructor с
m_AFD=null. Проверены аналитические near/far/FOV значения, нулевой размер,
NaN и неверный far без изменения globals, duplicate refusal, repeated Finish,
повторное создание и оставление камеры до общего Deinit. **Это независимая
CPU-предпосылка, не прохождение production Init через gamma/renderer blocker.**
g_MatrixMap не публикуется; map/form/loop/battle и полный CGame::Init NOT CALLED.

| Gate | Результат |
|---|---|
| Host ASan/UBSan + leak detection | PASS оригинальной камеры в общем bootstrap, cleanup/reentry |
| Linux ARM64/QEMU | PASS того же нового участка |
| devkitA64 | COMPILE/LINK PASS, strong undefined=0, реальные ctor/dtor retained; runtime NOT RUN |
| Windows x86 | Новый код компилируется; LINK_BLOCKED **32 вместо 33**; удалён только SetMaxCameraDistance, новых unresolved нет |
| Windows x64 | NOT REPEATED, последняя граница предыдущего этапа 33 |
| GPU / Switch / карта / полный Init | HARDWARE_PENDING / NOT EXECUTED |

Один кандидат, адресные существующие bootstrap targets; старые standalone suites
и общая compile inventory не повторялись. Логи:
`build/matrixgame/e2e519/camera-{host,arm64}-{build,runtime}.log`,
`camera-windows-build.log`, `camera-sdk-build.log`. Команды прежние: build target
matrixgame-native-bootstrap, затем прежние три реальные VO/PNG/data аргумента;
host ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 и
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1. SDK — прежний --bootstrap --resume.

240 effective files и core outputs сверены по фактическим байтам между host,
ARM64, Windows x86 и devkitA64. Единственный новый generated file —
MatrixCameraLifecycle.cpp; остальные prepared sources неизменны.
Manifest SHA-256 `6f932cefe3066c6068de1bc1b595c8376b36670f2fe6077d99af0f701ff241ab`.
SDK ELF SHA-256 `f9b92340b4de2a84aa33d550983eeaa92138e557d5d2ae701fda2459f191b9bd`,
4708856 bytes, Build ID `e71d5849a9aef4a95930f23572c1c26e3a638f85`.
Сборка выполнена с dirty рабочими исходниками до implementation commit;
входные файлы сверены с ним после нормализации только CRLF/LF. Embedded Git SHA нет.

13 доступных protected hashes совпали; G:\pack.zip по-прежнему CURRENTLY_UNAVAILABLE,
14/14 PASS не объявляется. Upstream pin чистый. Основная NRO/ELF и оба frozen
аппаратных ZIP не изменены и не пересобирались; новые NRO не создавались.
Два коротких аппаратных запуска описаны в `docs/e2e519-two-switch-runs.md`;
разбор основной игры — после реальных журналов, предположительных правок нет.

Следующие три блокера: (1) оригинальные CMatrixMap/CMatrixMapLogic ctor и полное
освобождение, включая DevConsole/Minimap/Cursor/Sound и factories; (2) настоящее
SDK-устройство для gamma/renderer и оставшиеся 32 Windows link dependencies;
(3) единое владение static-init/heap/cache/data перед полным CGame::Init.
E2E-5.19 остаётся активным; самостоятельный объект камеры не считается картой.
