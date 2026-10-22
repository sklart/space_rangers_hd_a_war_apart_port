# E2E-5.18 — оригинальный Cache и CPU-фаза нативного входа 3G

Продолжение: [оригинальная no-billboard анимация → skinned Draw](matrixgame-native-animation-20261022.md).
Host/ARM64-QEMU PASS, SDK LINK PASS. Полный Init/Run и GPU execution не подтверждены.

Новое продолжение: [оригинальные LoadSpecial/BeforeDraw/Draw и SSkin callbacks](matrixgame-native-callbacks-20261022.md).
Host/ARM64-QEMU runtime PASS, SDK LINK PASS; GPU и полный CGame::Init остаются pending.

Продолжение: [skin resources и UV lifecycle → VO surface draw](matrixgame-native-skin-20261022.md).
Новый CPU runtime/ARM64-QEMU PASS и SDK LINK PASS; GPU execution и полный CGame::Init
по-прежнему не подтверждены. Ниже сохранены результаты исходного этапа E2E-5.18.

22.10.2026. Локальный `master`, база `3ab96a02efa3341748c0ab34e91a659b8d81559d`.
Код: `5db150fdc3344653e7777fe493382ab2cce7b445`. Push не выполнялся.
MatrixGame pin прежний: `218115a72fa9c5d43269f4ca84a8596a0bcb82f1`, upstream чист.
[Машиночитаемые доказательства](matrixgame-e2e518-evidence.json).

## Новый результат и граница

Полная **release translation unit `MatrixLib/3G/Cache.cpp` теперь компилируется**
через portable resource adapters. В отличие от выделенного VectorObjectCpu.cpp,
здесь сохранены все release-методы исходного Cache.cpp, включая фабрику, поиск,
порядок использования, загрузку, уничтожение и глобальный lifecycle. Это не
компиляция полной VectorObject.cpp и не полный MatrixGame.

Новая исполняемая связная цепочка:

```text
NativeInitRuntime::Begin
  → собственный buffers::Context/Scope
  → исходный CacheInit → heap + CCache
  → L3GInitAsDLLCpuPhase: исходный CPU-префикс L3GInitAsDLL
      → CPU Deinit предшествующего пустого контекста
      → g_HInst = hinst, g_Wnd = hwnd, g_WndExtern = true
  → исходные CCache::Get/Find/Create/Add
      → настоящий CVectorObject, CCache::PreLoad → original LoadSpecial
      → типизированный texture adapter → существующий TextureRequest/decoder
  → CCacheData::Prepare → Load/CCache::Up
  → material reference → immutable payload lease
  → Deinit CPU-контекста → CacheDeinit/Clear/Delete/Destroy
  → освобождение buffer scope/context
  → повторный Begin/load/Deinit
```

CPU-фаза 3G **не устанавливает WndProc, не создаёт окно или GPU device**.
Она хранит только заимствованные opaque module/window identities. Исходный
префикс извлекается из pinned 3g.cpp; следующая часть исходной функции —
заимствование `IDirect3D9/IDirect3DDevice9`, затем `GetWindowLong`/WndProc hook
и `GetDeviceCaps` — не подменяется успешными функциями.

`CpuReady` означает только описанную фазу; `GpuReady=false`. Полные
`static_init`, `L3GInitAsDLL`, `CGame::Init`, `RunGameLoop` и `CFormMatrixGame`
**не вызывались**. Нового CGame, пустого render loop или результата боя нет.

## Владение и сохранённая семантика

| Узел / функция | Владелец | Перенос / зависимость | Достигнутая граница |
|---|---|---|---|
| `CacheInit`, `CCache::Get/Create` | Cache heap и глобальный CCache | исходный Cache.cpp, native texture factory и прежний VO | исполняется; двойной вход отвергается до замены владельца |
| `CCacheData::Prepare`, `Up`, `RefDecUnload` | исходный cache entry | исходные алгоритмы и счётчик ссылок | загрузка/порядок/Unload проверены |
| VO / общие BigVB/BigIB | CVectorObject + buffer context | прежний E2E-5.17 код без изменений | новый вызов через настоящую фабрику/PreLoad/Clear; старый standalone тест не повторялся |
| `Texture` / `TextureManaged` cache entries | CacheTexture, owned decoded image | существующие TextureRequest/TextureAsset, оригинальный ParseFlags | CPU payload/flags/ownership; не COM и не завершённый CTextureManaged |
| material reference | weak token владельца entry | явный typed interface | после Clear/Destroy просрочен; не может обратиться к переиспользованному указателю |
| payload во время upload | shared immutable lease | `UploadCachedMaterialTexture` → `SetTextureAsset` | CPU lease выполняется; настоящий deko upload скомпонован, не исполнен |
| CPU-префикс `L3GInitAsDLL` | NativeInitRuntime | заимствованные CPU identities | исполняется; чужой живой контекст не сбрасывается |
| полный 3G/device lifecycle | platform backend / borrowed device | Win32 hooks, caps/gamma, renderer state | BLOCKED; device loss/reset и физический GPU не проверены |

Ключи Cache сравниваются исходным Find, дубликат Get возвращает тот же entry.
Фабрика VO по-прежнему создаёт настоящий CVectorObject. Для текстур используется
явный CCacheData-адаптер, который не притворяется CTexture/CTextureManaged или
IDirect3DTexture9. Его нельзя передавать старым CSkinManager casts/`Tex()`.
Без bound native session texture factory явно выдаёт `UNSUPPORTED`.

Сохранена важная ветвь `USE_DX_MANAGED_TEXTURES`: **managed Unload оставляет
payload до уничтожения владельца**, default Unload освобождает его сразу.
Оба поведения проверены через RefDecUnload/Prepare и реальные byte counters.
Это CPU resource policy; managed/default поведение при настоящем device loss
ещё не реализовано этим этапом.

Cache heap публикуется транзакционно; при отказе второй allocation первый
heap освобождается. Get уничтожает ещё не опубликованный объект при ошибке
имени/list insertion. Уничтожение идёт в обратном порядке. Begin не вызывает
глобальный CVectorObject::StaticInit и не сбрасывает чужие общие буферы.
Прежняя защита повторной VO-загрузки E2E-5.17 не переделывалась.

Два path helpers Cache.cpp адаптированы явно: Windows implicit
`filesystem::path → wstring` не компилируется на Linux. Используется существующий
texture extension boundary; относительное имя, Unicode, Windows separators и
query suffix проверены адресно. Это не новая глобальная проверка всех Windows
drive-relative/UNC/malformed path вариантов.

## Compile delta: те же 106 units и флаги

Один полный baseline и один полный candidate audit выполнены существующим
`matrixgame_engine_compile_audit.py`, targets `matrixgame-native-engine` и
`matrixgame-native-matrixlib-audit`, GCC 15.2, C++20, RelWithDebInfo,
`-fsanitize=address,undefined -fno-omit-frame-pointer`, исходные target recipes.
Отдельный RoadNetwork не включён в этот набор и не перетестировался.

| Измерение | COMPILE_PASS | COMPILE_FAIL |
|---|---:|---:|
| Baseline, 106 units | 13 | 93 |
| Первый полный candidate, до исправления includes Cache | 13 | 93 |
| Итог: тот же candidate + адресный retry только Cache.cpp | **14** | **92** |

Для Cache устранены недостающий generated VectorObjectCpu.hpp в engine view
и лишняя зависимость публичного адаптера от private TextureFlags.hpp. Повторялся
только падающий Cache.cpp, а не полная матрица. Добавлен `--source` suffix filter
в audit tool для воспроизведения такого retry с исходным recipe/flags.
**Единственное изменение compile-статуса — Cache.cpp.** Остальные 105 source
SHA сверены с итоговыми effective inputs; их статусы не объявлены улучшенными.

Данные: `build/matrixgame/e2e518/baseline/`, `baseline-source/`, `candidate/`,
`cache-retry/`, `cache-final/`; объединённый результат — `candidate-effective.json`.
Raw candidate FAIL не скрыт и не переименован в PASS.
Full VectorObject.cpp и MatrixGame.cpp по-прежнему блокируются D3D headers.

## Что действительно проверено

Настоящая read-only копия `Matrix/Obj/tree/wood05.vo`, 5422 bytes, SHA-256
`04497dcc94ede26f183d847a4dd094d3ba05bc825d09fcfa73fa36bfae80981e`.
Текстура — **контролируемый PNG fixture**, не выданный за игровой материал:
7×5 исходных pixels, существующая CompleteBox/POT policy даёт 8×8 + mip chain.
Две загруженные texture entries удерживают **680 bytes**; после Deinit cache
bytes и число texture entries равны нулю. Отдельная in-flight payload lease
остаётся корректной до своего release и не оживляет уничтоженный cache entry.

| Gate | Результат |
|---|---|
| Host, весь связанный код ASan/UBSan/leak detection | **HOST PASS**, два Init/load/Deinit цикла, assertions включены |
| Missing texture, обрезанная VO, stale material handle, повторный Deinit | PASS; отказ не превращается в успешную загрузку |
| Два настоящих `calloc` failure в исходном CHeap при CacheInit | PASS: rollback, отсутствие опубликованных globals/утечек, последующий вход |
| Чужой Cache / живой CPU 3G context / повторный Begin | PASS: отказ без разрушения чужого владельца, cleanup частично созданной своей фазы |
| Managed retention / default release и повторный Prepare | PASS, сверены IsLoaded и byte counters |
| Linux ARM64/QEMU, тот же новый контракт | **ARM64-QEMU PASS**, включая оба calloc failure |
| devkitA64/libnx/deko3d | **SDK LINK PASS**, undefined strong = 0; настоящий cache → deko texture upload/device path в ELF |
| Windows x86 cache/device reference | **NOT RUN**; прежние layout/asset references не доказывают этот новый lifecycle |
| Switch / GPU execution / полный бой | **HARDWARE PENDING / BLOCKED** |

Флаги probe: `-UNDEBUG` после `-DNDEBUG`; GNU host/ARM64 применяют
`--wrap=calloc`, факт срабатывания проверяется отдельно. Host использует
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`, UBSan halt/stacktrace.
Старые CList, BigVB/BigIB, RoadNetwork, CMain/Form, image/texture suites и
500-tick аркадные сравнения не запускались повторно.

Сохранены FAIL-попытки: Windows implicit path conversion; ошибочное ожидание
исходного PNG-размера вместо установленной POT policy; missing effective/private
headers; отсутствие explicit TextureFlags include в probe. Исправлялся падающий
target. Декодер ради подгонки результата не менялся. Managed retention проверена
по исходному Texture.cpp:511 и включённому USE_DX_MANAGED_TEXTURES.

Для QEMU отсутствовали Linux ARM64 PNG/JPEG библиотеки. В build получены прежние
зафиксированные зависимости: libpng `a40189cf881e9f0db80511c382292a5604c3c3d1`,
libjpeg-turbo `ecf021bc0d6f435daacff7c35ccaeef0145df1b9`; старый ARM64 zlib
использован без пересборки. CRLF scripts libpng не принимались awk; нормализация
сделана только в производных exports, с журналом SHA `source-normalization.json`.
Source clones чистые; системные пакеты и SDK не менялись. Sandbox отказы WSL/Git
устранены разрешённым запуском сборок; это не дефекты движка.

### Артефакты и минимальное воспроизведение

- Host executable SHA-256:
  `33670079b1969464b1a0dd496321a33d64f6979407121fabc7008bc7b5de92c7`.
- Linux ARM64:
  `ec7860ff6d6a3f515cc6c6fc7de5c1bc471f6ba9a6fdb61b99a4fe1bd6bd66cd`.
- SDK ELF:
  `43072e3989460c30578bae1868efedd37f053b769f5ac140117f6da635dcb0a0`,
  Build ID `af37e330c650ace84f7183565e94959949d0d16b`.

Host и QEMU логи: `runtime-managed/runtime.log` и
`arm64-normalized/runtime-managed/runtime.log` внутри `build/matrixgame/e2e518/`.
SDK: `sdk-approved/result.json`, `symbols.log`, `build.log`.
Эффективные CacheNative.cpp/L3GCpuPhase.cpp/VectorObjectCpu.cpp/.hpp имеют
одинаковые SHA в host, QEMU и SDK source views.

```sh
cmake -S port/matrixgame/native-engine -B build/matrixgame/e2e518/host \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
cmake --build build/matrixgame/e2e518/host --target matrixgame-native-init-probe --parallel 4
python3 tests/probe_matrixgame_native_init.py \
  --binary build/matrixgame/e2e518/host/matrixgame-native-init-probe \
  --model build/e2e515/big-buffer-models/model-1.vo \
  --texture build/mg-merge-host/core/texture-request-fixtures/assets/plain.png \
  --output build/matrixgame/e2e518/fresh-host
```

Нужны свои read-only VO и PNG либо указанные локальные inputs. Для нового QEMU
output после подготовки pinned deps: `bash tools/matrixgame_init_arm64.sh
build/matrixgame/e2e518/fresh-arm64 --model <VO> --texture <PNG>`.
Для SDK на Windows: `python tools/matrixgame_init_sdk.py --output
build/matrixgame/e2e518/fresh-sdk`; требуются существующие mesh_shaders.hpp/DKSH.
`--resume` сохраняет прежние SDK результаты и пересобирает изменённые inputs.
Эти команды не создают NRO и не запускают старые suites.

## Следующий первый блокер и исходный порядок CGame

Исходный `CGame::Init` (`MatrixGame.cpp:172`) сначала делает seed/static_init,
потом heap/robots.pkg/config → CacheInit → L3GInitAsDLL → CRenderPipeline/settings
→ CMatrixMapLogic/RobotPreload/PrepareMap. Новый Runtime является предпосылкой
этой цепочки, а не альтернативным успешным CGame::Init. Его нельзя предварительно
оставить активным и затем вызвать старый Init: защита владельца отклонит повторный
CacheInit. Включение должно происходить внутри будущего корректно перенесённого
entry с сохранением оригинального порядка statics и cleanup.

**Первый compile blocker полного entry:** `MatrixGame/src/MatrixGame.cpp` через
`MatrixLib/3G/VectorObject.hpp:8` всё ещё требует `d3d9.h`. Его содержимое не
вырезалось. Полные shadow/game statics также не исполнялись этим тестом.

**Следующий обязательный runtime contract:** `MatrixLib/3G/3g.cpp:346`,
`L3GInitAsDLL` — корректный borrowed device/platform binding, затем Win32
WndProc attachment (`GetWindowLong`/SetWindowLongPtr), caps/gamma и полный state.
Первый такой API-вызов — GetWindowLong; GPU prerequisite — GetDeviceCaps.
Нативная замена этого полного контракта отсутствует. Существующий deko владеет
настоящим offscreen device/queue/images, но не предоставляет Win32/D3D lifecycle.

Далее остаются `D3DControl.hpp`/`CRenderPipeline` и `ApplyVideoParams`,
`MatrixSkinManager.cpp` с legacy CTextureManaged/skin pass state, полный
VectorObject draw/shadows, PrepareMap. `MatrixGame.cpp:778` RunGameLoop и
`MatrixFormGame.cpp` Enter/Draw/Takt разрешены только после настоящего Init.
GPU upload одного material asset не закрывает эти зависимости.

## Основная игра и сохранность

12 защищённых входов сохранили SHA-256 до/после: пользовательские saves/config/log,
untracked provenance, G:/pack.zip, source VO/PNG и основной E2E-5.16 ZIP.
Отдельно сверены неизменные SHA обоих замороженных ZIP и NRO/ELF внутри них.
MatrixGame и dependency pins чистые; игровые bytes не закоммичены.
Все runtime-записи находятся в отдельных ignored build directories.

Основная NRO и GPU smoke **не пересобирались и не менялись**. Основные renderer,
карта, аркада, save/recovery/Unicode, input и timer не редактировались.
Причины фонов и задержек карты по-прежнему не установлены; hardware PASS нет.
[Вечерний сценарий](../docs/e2e517-evening-switch-check.md) оставлен без изменений:
фактического недостатка инструкции не обнаружено. Новый SDK ELF E2E-5.18 в
старый matrixgame-gpu-smoke.nro не включён. OC/Atmosphère/Horizon не менялись.
