# E2E-5.21: native CaptureState/restore

25.10.2026. **PARTIAL; Switch runtime NOT RUN.** `DekoDevice::CaptureState`
теперь создаёт настоящий snapshot состояния native renderer, а restore
восстанавливает сохранённые значения и resource bindings. Прежний исходный
блокер `NativeInitRuntime::AttachDeviceImpl` снят и SDK-связан. Следующая
известная исходная граница — `SwitchWindow::Show(Normal)` из `CGame::Init`.
[Команды, результаты и SHA-256](matrixgame-e2e521-state-evidence.json).

## Что сохраняется и как применяется

В `NativeRenderState` собраны все изменяемые значения, которые текущий
`DekoDevice::Record` использует для настоящих GPU-команд: raw texture stages/
factor/null bindings, восемь samplers, raster/cull/shade, raw register map,
FVF, world/view/projection и восемь texture matrices, viewport, material,
восемь lights/enables и TCI/lighting controls. Все setters DekoDevice изменяют
этот же state store; Record читает его для shader uniforms и SDK bindings.

Snapshot удерживает неизменяемую версию CPU state через shared ownership.
При последующем изменении device создаётся отдельная копия. Restore возвращает
сохранённую версию без heap allocation и без поэлементного частичного Apply.
Observable getters сразу видят сохранённые значения; следующий native draw
записывает их в реальные deko commands. Уже записанные draw packets сохраняют
свою память и texture image versions. С моделью явной записи команд согласованы
[описанные API deko3d](https://github.com/devkitPro/deko3d/blob/master/Primer.md);
аппаратная проверка этой записи на Switch всё ещё не выполнена.

Программы и owned swapchain targets фиксированы для жизни этого DekoDevice.
Snapshot разрешён вне активного acquire/frame; restore во время кадра возвращает
INVALIDCALL и допускает повтор после завершения кадра. Capture/restore стороннего
renderer, активной swapchain transaction или ещё не реализованных offscreen
targets этим контрактом не заявляется.

## Владение и ошибки

Texture bindings в snapshot держат сам native texture resource независимо от
original owner. Новая `buffers::BindingSnapshot` удерживает vertex/index
resources, offset и stride; восстановление возвращает их в исходный Context.
Resource bytes, lock state и версии содержимого не откатываются — сохраняется
именно binding, как при исходном state capture. Capture не читает staging bytes
и допускает сохранение binding на временно locked resource.

Context lifetime проверяется через его allocation owner; уничтожение Context
не оставляет snapshot с указателем, через который можно выполнить restore.
Чужой или отсутствующий активный Context даёт отказ до изменения device state.
Уничтоженный device/state store даёт DEVICELOST, включая lease, переживший owner.

Счётчик OutstandingStateLeases растёт только после успешного создания lease
и уменьшается при его уничтожении. Само уничтожение lease не делает скрытый
restore. Повторный явный Restore снова применяет ту же сохранённую версию;
независимые snapshots можно применять в разном порядке. Отказ allocation
при Capture или copy-on-write не меняет прежние state/bindings и не теряет refs.
Restore не выделяет память, включая освобождение заменяемого state.

`NativeInitRuntime::AttachDeviceImpl`, исходные CGame::Init/Draw и prepared
original algorithms не переписывались. Их существующие вызовы CaptureState,
Restore и cleanup теперь достигают новой реализации DekoDevice.

## Что действительно исполнилось

| Проверка | Результат |
| --- | --- |
| Host ASan/UBSan/leaks | PASS: тот же state store, который использует DekoDevice; 9 основных restores, 2 точки Capture OOM, COW OOM, restore при запрещённых allocations |
| ARM64/QEMU | Те же state/lifetime/error проверки PASS |
| Windows x86 CPU | Те же проверки PASS, настоящий MSVC C++20 build |
| Resource scenarios | Пустые/непустые bindings, удержание texture/VB/IB после release owner, nested/out-of-order snapshots, locked buffer, сохранение изменённых resource bytes, чужой/уничтоженный Context, lease после смерти device store |
| devkitA64 | Новые state/buffer/provider/probe objects + сохранённые original Init/frame archives compile/link PASS; strong undefined=0 |
| Windows original Init/frame regression | Два настоящих Init/frame/Deinit с новым buffer_resources object, по 1008 draws, 1006 indexed, 111812 primitives; snapshots совпали с прежними побитово |
| Switch | Вызов нового CaptureState/restore, создание arena/device и загрузка карты — NOT RUN |

Windows regression выполнена из-за изменения общего staging ownership.
Карта 276×285, группы 28×29, кадр 800×600 с 154038 RGB-цветами. SHA-256 обоих
снимков: `f6f0d25c1e7b2a194f6523f8c96b9a74f7b7385525e5bb5cf7696de7f09a52bb`.
Reset failure/release/retry, readback состояния и восстановление gamma/window
прошли. Использованы прежние original libraries с адресной заменой buffer object;
чистая пересборка всего engine и полный бой не заявляются.
1476-case material suites и прежние большие CPU наборы повторно не запускались.

CPU state tests выполняют настоящие NativeStateStore/BindingSnapshot, проверяют
сохранённые shader uniforms и bindings; они не эмулируют GPU и не выдают успех
native Init. Новая CMake test target подключена; адресные runtime builds и SDK
link проверены скриптами ниже, полный root CMake rebuild не выполнялся.

## Где остановилась цепочка до первой карты

После успешных реальных device/queue/swapchain/shaders/arena allocation:

```text
CGame::Init
 → original_game::EnterPlatform
 → NativeInitRuntime::EnterNativePlatform
 → AttachDeviceImpl: InitAsNative → DekoDevice::CaptureState
 → original CRenderPipeline construction
 → ReadyWindow().Show(Visibility::Normal)
 → SwitchWindow::Show
     Unsupported("Switch visibility requires native applet lifecycle")
```

Это первая известная граница по исходникам, не аппаратно наблюдавшаяся.
При нынешних восьми texture slots исходный CRenderPipeline выбирает штатные
ветви материалов; его подготовка не обойдена. Show/Update требуют настоящей
native applet/window lifecycle интеграции. Они не заменены успешным no-op.
Дальше остаются original stored-state backend, video/font/cursor/input,
незакрытые texture conversions/surfaces/offscreen/shadows и физическая проверка
allocation/alignment/upload/restore. Original Init/PrepareMap и первая карта
на Switch ещё не приняты.

## Сохранность и открытые дефекты

Аудит подтвердил SHA-256 15 защищённых файлов, 107 файлов hardware-комплекта,
robots.pkg/CFG, frozen GI cache source, frozen E2E-5.10 NRO/ELF, корневой
cumulative NRO и основной production NRO по предыдущему fingerprint.
Исторический `G:\pack.zip` остаётся UNAVAILABLE. Новая NRO не создавалась;
основная NRO, frozen-комплекты, ресурсы и сейвы не изменены, commit/push нет.

P0 основной игры, `manual-publish ENOENT` и зависания Switch остаются OPEN.
Новые texture/state ownership tests не устанавливают применимость дефекта
общего GI-кэша к ресурсу проблемного экрана и не доказывают причину повторяющегося
текста. Новых аппаратных проверок нет.

Воспроизведение: `tools/matrixgame_state_checks.py host|arm64|windows|sdk`,
`tools/matrixgame_state_windows_frame.py`, `tools/matrixgame_state_audit.py`.
Все новые артефакты — в `build/matrixgame/e2e521/state`.
