# MatrixGame: минимальные нативные providers для Switch

25.10.2026, E2E-5.20. Это контракт следующей интеграции, **не реализованный
полный backend Switch**. Windows HAL уже исполнил оригинальный Init и кадр
настоящей карты; это позволяет выделить потребности движка по реальной цепочке.

Обновление E2E-5.21: native entry и начало libnx/deko3d providers реализованы;
Init + original frame closure линкуются для SDK. Полный native renderer ещё
не реализован, hardware NOT RUN. [Первый отчёт](../reports/matrixgame-e2e521-native-entry.md).
Продолжение: прежние alpha/null отказы закрыты в проверенной матрице,
подключены FVF lighting/TCI и native draw routing, `ReadCaps` проверяет загруженные
shader programs. [Проверки и оставшиеся границы](../reports/matrixgame-e2e521-cascade.md).

Residency-продолжение: image-capable texture backing, allocator, original mip
upload/locks/bindings и image-version lifetime реализованы и SDK-связаны.
`AvailableTextureBytes` возвращает свободный непрерывный диапазон backing.
[Текущие результаты](../reports/matrixgame-e2e521-residency.md).

State-продолжение: CaptureState/restore с texture/VB/IB lifetime и без allocation
при restore реализованы. CPU host/ARM64/Windows и SDK original closure проверки
прошли; native Switch runtime NOT RUN. [Отчёт](../reports/matrixgame-e2e521-state.md).

## Граница запуска

Сейчас Windows идёт через `NativeInitRuntime::EnterLegacyDll`: проверяет
настоящие HWND/API/device identities, затем подключает providers. Switch должен
получить отдельный typed native entry, передающий настройки, файл карты, seed,
язык и callbacks хозяина без SRobotsSettings-полей с фиктивными D3D pointers.
Оригинальные подготовка ресурсов, PrepareMap, симуляция и renderer selection
сохраняются. Нельзя обходить Init демонстрационной сценой.
Теперь `InitializeNativeGame(NativeGameOptions)` выполняет этот вход без legacy
identities. Window lifetime attachment отделён от WndProc. `ReadCaps` больше
не содержит безусловного отказа: проверяет actual vertex/fragment programs.
Следующая известная граница — `SwitchWindow::Show(Normal)` в оригинальном
`CGame::Init` (`Switch visibility requires native applet lifecycle`).
Она установлена по исходникам после реализации CaptureState/restore;
новый native Init и texture GPU runtime на Switch не исполнялись.

## Дисплей и окно

Standalone и borrowed input разделены в E2E-5.35: только отдельный entry
устанавливает supported Npad IDs/styles; SwitchInput сохраняет policy хозяина.
Новая SDK ELF связана, реальный HID runtime не запускался.
[Текущий полный порядок Init/cleanup](matrixgame-init-sequence.md) и
[доказательства bootstrap](../reports/matrixgame-e2e535-standalone-input.md).

E2E-5.34 уточнил Reset ownership: renderer-only bindings теперь разрешены
до атомарного Commit; игровые owners, COW snapshots и surface leases блокируют
сброс до allocations. HOST/ARM64 candidate и SDK link PASS. WINDOWS HAL retry
после освобождения state block имеет два открытых FAIL; весь этап PARTIAL.
Native Init/карта/recovery на Switch не исполнялись.
[Проверки и открытая граница](../reports/matrixgame-e2e534-reset-owners.md).

- Provider сообщает фактическую display extent и доступную drawable extent;
  логические координаты UI, клиентская область и render extent различаются явно.
  Настройки камеры/проекции и viewport согласуются с размером render targets.
- Win32 ResizeOuter компенсирует рамку окна, а fullscreen меняет стиль/положение.
  На Switch вместо этих операций требуется выбор размеров swapchain/render
  targets под handheld/docked и изменение доступного дисплея. Win32 window style
  и HWND не являются частью нативного контракта.
- Жизненный цикл привязан к настоящему native window/libnx и applet-событиям.
  При уходе из игры восстанавливаются input/display/renderer хозяина; изменение
  video mode не может объявляться успешным без соответствующих targets/viewport.

## Устройство и кадр

E2E-5.37: source-confirmed CMinimap LINESTRIP получил реальный native aliased
line path, width=1 и без stipple/smoothing. Исходный LASTPIXEL=TRUE проверен
HAL/production GLSL на original geometry; FALSE/AA остаются Unsupported.
Full Windows/map-state HOST/QEMU и SDK PASS, Switch runtime HARDWARE_PENDING.
[Границы и доказательства](../reports/matrixgame-e2e537-lines.md).

- Настоящие deko3d device, queue, command buffers, swapchain, color/depth targets
  имеют явного владельца. Создание публикует готовность после успешного завершения
  всех обязательных шагов; частичный отказ освобождает созданные объекты.
- Сессия кадра: acquire drawable → подготовка исходной карты/объектов → запись
  команд → submit → present с правильной синхронизацией. Readback требует
  окончания GPU-записи и корректной видимости CPU-памяти.
- Recreation/потеря drawable должны сначала завершить или отменить владение
  предыдущим кадром и освободить зависящие от extent targets. Новые размеры
  публикуются после успеха. Нельзя механически копировать D3D9 Reset или
  возвращать фиктивный device-lost HRESULT из несуществующего устройства.
- Capability queries описывают поддержанные material/texture/render paths;
  не подставляют выдуманные D3DCAPS9. Строго сохраняются явные Unsupported для
  путей, для которых ещё нет реализации.

## GPU-память, ресурсы и bindings

- Allocation принимает фактические требования image/buffer layout SDK:
  alignment адреса, alignment размера, допустимый memory kind/flags и extent.
  Округление проверяет переполнение и совместимость требуемых выравниваний.
  Нельзя считать один фиксированный размер достаточным для всех images.
- Исправление прежнего GPU alignment guard остаётся **HARDWARE_PENDING**:
  старый smoke упал до draw, SDK compile/link исправления этого не отменяет.
- CPU staging не является GPU buffer. `Resource::Id/Version`, размер, stride,
  index width, offset и initialized ranges определяют upload и допустимый draw.
  Нужны настоящие native vertex/index bindings и поддержка используемых layouts
  и topology. Повторное использование адреса CPU-объекта не означает тот же ресурс.
- Текстуры сохраняют содержимое, mip levels, compressed formats, sampling и
  материал. Resource upload включает необходимые cache flush/barriers; память
  нельзя освобождать до окончания последнего использующего её submit.
- Scope материалов сохраняет original CSkinManager callbacks и их выбор passes.
  Side/gloss/mask/back, освещение, terrain/water и shadow/effect paths должны
  получить соответствующие shader/binding implementations. Ordinal single-pass
  renderer не покрывает этот контракт.
- Deinit снимает bindings, дожидается GPU, освобождает загрузки/targets/descriptor
  storage и убирает borrowed references до уничтожения владельцев CPU-данных.
  Два Init/frame/Deinit подряд — минимальная lifecycle-проверка.

## Приёмка перехода

E2E-5.33 подключил настоящий CFormMatrixGame и native pump исходного L3GRun.
HNew/FormChange/Enter предшествуют циклу; Leave/HDelete выполняются до Deinit.
Applet focus и HID направлены в исходные handlers. Acquire происходит после
Takt/recovery без дополнительных clear: очисткой и Present владеет original Draw.
HOST ASan/UBSan и QEMU проверяют scheduler со строго обозначенными CPU fixtures;
Windows HAL выполняет actual Init/form/input/Draw/release/recreate/Deinit.
Это не подтверждение исполнения GPU/HID на Switch.
[Результаты и оставшиеся границы](../reports/matrixgame-e2e533-form.md).

Сначала native entry и доступная CPU-цепочка с настоящими ресурсами, затем SDK
compile/link, затем новый аппаратный allocation/upload/draw/readback/recreate.
После этого — полный кадр исходной карты, управление и бой внутри основной игры.
HOST, ARM64/QEMU, SDK и SWITCH HARDWARE фиксируются раздельно.

Windows `PresentationAccess::Exclusive` — ограниченный контракт тестового
владельца нового SDK-устройства, не обещание менять чужое устройство игры:
чужие default-pool ресурсы/state blocks не должны оставаться живыми при reset.
Borrowed режим продолжает явно отказывать в Reset. Этот Win32 механизм не нужно
переносить на Switch как интерфейс управления deko3d.
