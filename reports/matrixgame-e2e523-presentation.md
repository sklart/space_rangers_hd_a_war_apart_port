# E2E-5.23 — native display/window/presentation/reset

Дата: 25.10.2026. **NATIVE PRESENTATION SOFTWARE PASS / FULL NATIVE INIT PARTIAL / SWITCH HARDWARE PENDING.**

Закрыт прежний provider-guard `CGame::ApplyVideoParams → Device::ReadCreationParameters`. Creation/display queries и настоящая замена deko presentation реализованы; оригинальные Init, ApplyVideoParams и Draw не заменены. Полный native Init и первая карта на физическом Switch ещё не исполнялись. Следующий конкретный UI text guard подтверждён исходным ресурсом и адресным отрицательным исполнением.

Машиночитаемые команды, source/artifact SHA, статусы и protected fingerprints: [evidence JSON](matrixgame-e2e523-presentation-evidence.json). Старые отчёты E2E-5.21/5.22 — исторические срезы; их build outputs не переписывались.

## Реализация

| Файлы | Реальное поведение |
| --- | --- |
| [platform_switch.hpp](../port/matrixgame/native-engine/platform_switch.hpp), [platform_switch.cpp](../port/matrixgame/native-engine/platform_switch.cpp) | Creation parameters существующего deko device: adapter 0, hardware type 1, отсутствие Windows CPU/mixed vertex-processing flags, FocusWindow — действительный NWindow. COM/Win32 identities не создаются. |
| Там же: ReadAdapterDisplayMode | Для default NWindow открывается VI Default display, запрашивается фактическое разрешение, читается VSync event и девять временных отметок VBlank. Частота вычисляется по восьми интервалам; неустойчивые, пропущенные или немонотонные измерения отвергаются. Повторный resolution query обнаруживает смену режима во время измерения. Успешные 60 Гц не подставляются константой. VI не выдаёт D3D scanout format: Format отражает фактический формат native layer buffers. Произвольный NWindow без явного display binding этим query не обслуживается. |
| Там же: SwitchWindow | Client/window rectangles читаются из настоящего borderless applet NWindow. ResizeOuter и MoveFullscreen вызывают реальный owned presentation Reset, затем ReadClientRect отражает committed размеры. Fullscreen означает видимый applet layer без crop, не эмуляцию desktop HWND. Декораций Win32 у этого окна нет. |
| [native_presentation.hpp](../port/matrixgame/native-engine/native_presentation.hpp) | Общие production Plan, capability checks, Reset transaction и combined restore lease. Эти же классы исполняются в CPU fault tests. |
| platform_switch.cpp: TargetSet/Driver | Настоящие DkImageLayout и backing MemBlocks: RGBA8/RGBX8/RGB565 и Z24S8/Z24X8/Z16. Swapchain создаётся из подготовленных images. Аллокации/состояние нового набора готовятся до удаления прежнего swapchain; старые backing blocks сохраняются до успешной регистрации нового. |
| [native_device_state.hpp](../port/matrixgame/native-engine/native_device_state.hpp), [.cpp](../port/matrixgame/native-engine/native_device_state.cpp) | Reset публикует заранее созданный authoritative render state и очищает staging bindings. Начальное создание устройства не очищает чужой уже привязанный CPU context. Restore lease сначала восстанавливает presentation, затем исходный render/buffer snapshot. |
| [native_texture.hpp](../port/matrixgame/native-engine/native_texture.hpp) | DefaultResourcesLive проверяет действительные weak/shared owners session: pool 0 блокирует Reset даже при удержании другим владельцем; managed pool 1 не блокирует. Managed CPU payload и GPU residency сохраняются при замене presentation, поскольку DkDevice/queue остаются прежними. |
| [native_frame_scope.hpp](../port/matrixgame/native-engine/native_frame_scope.hpp), platform_switch.cpp | Native Clear/BeginScene могут сами выполнить acquire. Это необходимо исходному transition внутри Init, ещё до внешнего first-frame probe. Clear запрещён после submit; необъявленный stencil не очищается в depth-only target. В диагностическом BeginNativeFrame явный initial clear сохранён. |
| [init_runtime.cpp](../port/matrixgame/native-engine/init_runtime.cpp) | При ошибке внутри Init runtime сначала quiesce/cancel незавершённый native frame, затем освобождает original map/font/resources. GPU ошибка не маскируется успехом; GPU packet owners удерживаются устройством. |
| [CMake](../port/matrixgame/native-engine/CMakeLists.txt) | Отдельный адресный production presentation test target. Полная original native entry ELF остаётся отдельной целью от CPU теста. |

CheckMultisample сообщает только реализованный single-sample path. Неподдержанный MSAA возвращает NotAvailable, и исходный ApplyVideoParams сохраняет свой обычный fallback без MSAA. Запись MULTISAMPLEANTIALIAS=false соответствует настоящим single-sample targets; true отвергается. Нового фиктивного resolve нет.

Для VSync включённого/default используются минимум два native buffers, для immediate — минимум три; эффективное число возвращается в Presentation. Это ограничение [NWindow/deko swapchain](https://raw.githubusercontent.com/devkitPro/deko3d/master/source/dk_swapchain.cpp): с двумя буферами NWindow принудительно сохраняет swap interval 1. Explicit refresh, отличающийся от наблюдаемого физического режима, отвергается; provider не притворяется, что меняет частоту дисплея. Исходный native entry использует RefreshRate=0, FSAA=0, AF=0, VSync=1 и разрешение текущего NWindow.

## Reset, rollback и владельцы

Перед изменением окна проверяются window identity, dimensions, format/depth, buffer count, interval, здоровье queue, отсутствие acquired/scene/submitted frame и живых pool-0 textures. Default resources не уничтожаются принудительно ради PASS; их должен освободить исходный владелец через оригинальный loss/release path.

Последовательность: подготовить новые allocations и defaults → завершить queue → очистить завершённые command packets → убрать прежнюю регистрацию buffers → создать новый swapchain → установить swap interval/crop → опубликовать новые targets/state. NWindow dimensions нельзя менять при зарегистрированных буферах; фактическую регистрацию и размеры задаёт SDK SwapchainMaker. Размер backing allocations берётся из SDK layout; offset zero используется в выделенных image blocks. На один набор targets действует отдельная верхняя граница 256 MiB, без подмены её показаниями AvailableTextureBytes.

При отказе allocation прежний набор не меняется. При отказе регистрации выполняется cleanup частичного кандидата и повторный attach прежних backing blocks без их переаллокации. Если rollback тоже отказал, состояние остаётся lost/not-reset, а не ready. Следующий Reset может повторить восстановление. Отказ owner Restore возвращает ошибку и допускает повтор; после destruction устройства lease возвращает DeviceLost через lifetime token, не обращаясь к уничтоженному Impl.

Деструктор ждёт очередь, снимает presentation, закрывает font/texture sessions и возвращает исходные dimensions/swap interval/crop заимствованного NWindow. Ошибки IPC cleanup выводятся явно. Constructor rollback отдельно обрабатывает отказ до создания command buffer; разыменования отсутствующего command buffer нет.

Это **presentation reset при живом DkDevice/queue**, необходимый ApplyVideoParams. Пересоздание самого GPU device после ошибки queue, общий offscreen/StoredStateBackend и все возможные default-pool consumers этим этапом не закрыты. Физические allocation/IPC/OOM/synchronization/recreation пока требуют аппаратной проверки.

## Исполненные проверки

| Уровень | Результат и предел |
| --- | --- |
| **HOST** | ASan/UBSan/leaks, assertions ON: production Plan/Transaction/RestoreLease/NativeStateStore; реальные default-resource owner queries; сохранность чужих staging bindings; отказ при busy/default resources, OOM до публикации, completion failure, partial unregister/register, failed rollback, retry, owner restore failure/retry, lease после закрытия, invalid layouts/settings, VBlank jitter/missed events, Clear после submit. Низкоуровневые VI/NWindow/GPU операции здесь — явно обозначенная fault-injected CPU model. |
| **ARM64/QEMU** | Тот же production portable coordinator/state/lease действительно исполнился, assertions ON. Это не исполнение Switch GPU/VI. |
| **WINDOWS CPU** | Те же portable contracts исполнились под MSVC x64. Отдельно от HAL. |
| **WINDOWS HAL** | Общий изменённый init_runtime.cpp включён вместо прежнего runtime object. Два настоящих original CGame::Init→PrepareMap→CFormMatrixGame::Draw→Deinit, по 1008 SDK draws, сохранены прежние PNG SHA. Повтор выполнен из-за изменения порядка общего cleanup. Windows-продукт отдельно не разрабатывается. |
| **CMake/CTest** | Clean configure/build нового target, затем финальная адресная пересборка изменённых units. Один относящийся к задаче CTest PASS; flags содержат ASan/UBSan и UNDEBUG. Старые большие наборы не запускались. |
| **COMPILE/LINK** | Полная devkitA64 native entry ELF с оригинальными Init/ApplyVideoParams/PrepareMap/Draw и реальными providers. Strong undefined symbols: 0. Хэши ELF и link inputs — в JSON. |
| **SWITCH HARDWARE** | **NOT RUN / HARDWARE PENDING.** Не доказаны VI services, измерение VBlank, реальные resize/reset/rollback, GPU allocations/upload/draw, native Init/карта/кадр. Прежний аппаратный smoke FAIL этим не закрыт. |

Два промежуточных отказа Windows regression recipe исправлены без изменения игровой логики: устранён дублирующий старый init.obj, затем передан обязательный MATRIXGAME_UNION_OUTPUT наблюдателю предыдущего эталона. Финальная регрессия PASS. Assertions и обязательные операции не ослаблялись.

## Следующая настоящая граница

Для default native entry без Rangers callbacks следующий подтверждённый ресурсный путь:

`CGame::Init → CInterface::Load(Top) → SStateImages::SetStateText(false) → Text::Render → text_backend::DeviceIdentity`.

Точное исключение: **text SDK device unavailable**. Исходная CFG `robots/iface.txt` содержит в Top кнопку `mm`, статические labels «Меню» размером 75×20 и `Font.2Ranger`. Адресный ASan/UBSan тест вызывает **оригинальный Text::Render** с этими параметрами и подтверждает отказ до создания bitmap/font/surface. Это source applicability + исполнившийся отрицательный ресурсный вызов, **не выполнение полного CInterface::Load или native Init**. Иной ресурс, allocation или IPC может отказать раньше на консоли.

Debug font 25 не заменяет этот контракт. Original Text::GetFont заранее создаёт Verdana 13/12/13/14 и Rangers 10, регистрирует исходный Rangers font; затем нужны font metrics/text layout, render surface, draw и CPU bitmap copy/readback. Следующий этап должен использовать оригинальные font resources/алгоритмы и Windows reference; пустая текстура, произвольный системный font или один фиктивный DeviceIdentity guard не закрывают его.

Далее остаются реально необходимые default/offscreen/shadow/minimap paths. CreatePoolDefaultResources содержит StaticPrepare2, minimap RenderBackground и original StoredStateBackend; первый кадр потребует также оставшихся stencil/projected shadow операций. Автоматический acquire внутри transition теперь подключён, но его успешное выполнение на Switch не объявляется доказанным.

## Воспроизведение и сохранность

Из корня, Windows Python 3.13:

~~~powershell
$py = 'C:\Users\Sklyarov\AppData\Local\Programs\Python\Python313\python.exe'
& $py tools/matrixgame_presentation_checks.py host
& $py tools/matrixgame_presentation_checks.py arm64
& $py tools/matrixgame_presentation_checks.py windows
& $py tools/matrixgame_presentation_checks.py sdk
& $py tools/matrixgame_presentation_windows.py
& $py tools/matrixgame_presentation_boundary.py
& $py tools/matrixgame_presentation_cmake.py
& $py tools/matrixgame_presentation_audit.py
~~~

Рецепты используют существующие закреплённые E2E toolchains/dependencies; команды и необходимые baseline outputs записаны в result.json. Generated SDK и CMake source trees сверены с независимым E2E-5.22 набором: те же 252 исходных файла и SHA. Upstream остаётся чист на pin `218115a72fa9c5d43269f4ca84a8596a0bcb82f1`; root HEAD не меняется. Scoped diff --check, source fingerprints, основной/старые NRO, frozen/package/CFG/GI fingerprints и 107 исторических аппаратных входов проверяются финальным аудитом; недоступные protected paths не считаются hash PASS.

Основная NRO, frozen-комплекты, исходные ресурсы и сейвы не изменялись. Новая NRO не создавалась; аппаратные тесты Switch не запускались; commit/push не выполнялись. P0 повторяющегося текста, причинность GI-cache, manual-publish ENOENT и зависания Switch остаются **OPEN**.

Текущая программная ELF: `build/matrixgame/e2e523-presentation/sdk/matrixgame-native-entry.elf`, SHA-256 `b77ea2f30abdbda77460ce16b2b0e1bc2796582c87b2e2fb791d82b0eb9740e7`; проверены 169 link inputs. Это не аппаратная NRO и не свидетельство загрузки карты на Switch.
