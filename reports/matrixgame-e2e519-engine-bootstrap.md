# E2E-5.19 — интеграция оригинального DLL bootstrap

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

## Проверки окончательного кандидата

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
