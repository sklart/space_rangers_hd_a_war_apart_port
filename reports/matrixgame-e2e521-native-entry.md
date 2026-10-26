# E2E-5.21: native entry и начало Switch providers

Актуальное продолжение: [alpha/null, ReadCaps, FVF/TCI и native draw routing](matrixgame-e2e521-cascade.md).
Ниже сохранены результаты первого шага с прежним отказом ReadCaps.

Продолжение: [texture-stage shaders, Windows differential и незакрытый material
contract](matrixgame-e2e521-materials.md). Ниже сохранён снимок предыдущего шага;
новая часть не закрыла ReadCaps и не выполнила Init на Switch.

25.10.2026. **PARTIAL**. Создан native entry к оригинальному CGame::Init и
настоящий libnx/deko3d lifecycle provider. Полный Init/кадр на Switch не выполнены.
Основная NRO, frozen-комплекты, ресурсы и сейвы не менялись; push нет.

## Что изменилось

`NativeInitRuntime::InitializeNativeGame(NativeGameOptions)` получает типизированные
Window/Device, drawable extent, seed, карту, язык/тексты и необязательный draw sink.
В API нет HWND/HINSTANCE/D3D pointers. Настройки оригинального Init создаются из
исходных defaults, размеры берутся у Window; legacy pointer fields равны nullptr.

Оригинальный Init сохранён. Единственная новая адаптация его алгоритма — замена
вызова L3GInitAsDLL на EnterPlatform с теми же аргументами. Legacy branch вызывает
прежний L3GInitAsDLL; native branch проверяет отсутствие D3D identities и подключает
Native platform. Весь остальной Init/Deinit и полный исходный form Draw сохранены.
Native attachment — владение жизненным циклом Window, без Win32 WndProc/messages.
Отказ возвращает CPU/platform состояние владельцу и затем исполняет original Deinit.
Размер NativeInitRuntime не менялся; новый виртуальный Window метод добавлен в конец.

`SwitchWindow` проверяет настоящий NWindow и читает размеры через
nwindowGetDimensions. `DekoDevice` создаёт настоящий device, graphics queue,
два color targets, Z24S8 depth и swapchain. Все SDK objects имеют RAII-владельцев;
при обычном освобождении очередь дожидается idle, swapchain освобождается до images.
Окно с уже настроенными display buffers отвергается — чужой swapchain не перенимается.
NWindow остаётся borrowed и не закрывается provider.

Images выделяются в отдельных Image memblocks с offset=0; размер backing allocation
округляется с проверкой переполнения. Требования SDK к image GPU layout не смешиваются
с выравниванием CPU backing storage. Это использует прежний кандидат исправления,
который **аппаратно не подтверждён**.

## Реальные проверки

| Уровень | Результат |
| --- | --- |
| HOST COMPILE/LINK | Native entry, owner/platform и полный исходный CGame с прежними библиотеками — PASS |
| HOST RUNTIME | ASan/UBSan/leak checks: два original Init prefix/отказ/Deinit — PASS |
| ARM64/QEMU | Те же два реальных Init prefix и cleanup — PASS |
| devkitA64 COMPILE | Native runtime/platform/owner/game, Switch provider, original frame и полный visibility TU — PASS |
| devkitA64 LINK | Отдельный native-entry ELF с реальными deko3d/libnx и original Init/frame closure — PASS, strong undefined=0 |
| WINDOWS HAL RUNTIME | Регрессия после native routing: два Init/frame/readback/Deinit, те же 1008 tracked draws и идентичный E2E-5.20 кадр — PASS |
| CMake | Раздельные native-entry, Windows game и bootstrap цели; configure/generate проверен, полной большой пересборки не было |
| SWITCH HARDWARE | NOT RUN; actual allocation/swapchain/present/draw на устройстве не подтверждены |

Host/ARM64 тест использует **отрицательный fixture Device**, который отказывает
в ReadCaps, не возвращая фиктивных caps. Внутри него проверено, что настоящий
оригинальный Init уже создал heap и загрузил g_MatrixData. После каждого отказа
пусты heap/map/data/cache, native attachment, skin leases и original owner.
Ни ReadLegacyHandles, ни Win32 Attach/Show/messages не вызывались. Это не runtime
проверка deko3d и не успешный полный native Init.

Первый запуск новых host/ARM64 runners не дошёл до процесса из-за неверного cwd;
после исправления использованы прежние реальные resource directories, без повторной
компиляции. Для полного SDK frame closure были дополнительно нужны оригинальный
CalcMapGroupVisibility и исходное начальное FPS-состояние. Алгоритм visibility сохранён;
FPS начинается с нуля, измеряющий native loop пока не реализован.

Воспроизведение на текущем локальном наборе библиотек:

```text
python tools/matrixgame_engine_prepare.py --output build/matrixgame/e2e521/source
python tools/matrixgame_e2e521_checks.py host
python tools/matrixgame_e2e521_checks.py arm64
python tools/matrixgame_e2e521_checks.py sdk
python tools/matrixgame_e2e520_windows.py --stage e2e521
python tools/matrixgame_e2e521_audit.py
```

Host/ARM64 выполняются в WSL. Это адресная сборка с сохранёнными E2E-5.19/5.20
archives/objects, не автономная чистая release-сборка. SDK ELF не запускался,
NRO не создавалась. [Команды, хэши и результаты](matrixgame-e2e521-native-entry-evidence.json).

## Точная следующая граница

Если реальные libnx/deko device/queue/image/swapchain созданы успешно, первый
**известный неподдержанный программный вызов**:

`CGame::Init → original_game::EnterPlatform → NativeInitRuntime::EnterNativePlatform
→ platform3g::InitAsNative → DekoDevice::ReadCaps`.

Сообщение:
`deko original material capability contract unavailable: terrain/water/side/gloss/mask/shadows`.

ReadCaps отказывает намеренно: standalone ordinal shader не реализует полный
material renderer, и приписать ему D3D MaxTextureStages/MaxSimultaneousTextures
означало бы фиктивную готовность. Этот отказ в настоящем Switch runtime пока
**не наблюдался**: более ранний аппаратный отказ allocation/SDK также возможен.

Следующий кодовый этап — нативное представление material passes и реально
поддержанных shader/binding capabilities. Затем нужны CaptureState/lifecycle,
texture residency budget и original texture backend, gamma/color pipeline,
font/cursor/input, native frame acquire/submit/present/readback. Текущий provider
не выдаёт успех вместо этих операций. Первый кадр Switch и первая карта в native
Init остаются открытыми; Windows reference сохранён.

P0 основной игры, GI screen causality, manual-publish ENOENT и Switch hangs
этими изменениями не закрываются. Frozen cache source/call sites и защищённые
входы сверяются отдельно. Большие прежние CPU/Path наборы не повторялись.
