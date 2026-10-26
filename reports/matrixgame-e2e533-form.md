# E2E-5.33 — native original form и игровой цикл

В native game entry подключены настоящий CFormMatrixGame с полной vtable и
исходный L3GRun. Это программная интеграция; полный Init/карта/кадр на Switch
по-прежнему PARTIAL / HARDWARE_PENDING. NRO не создавалась, commit/push нет.

## Реальная цепочка и владение

InitializeNativeGame выполняет исходный CGame::Init, затем Session создаёт
CFormMatrixGame через HNew(NULL), выбирает FormChange и вызывает оригинальный
Enter. Полные Takt/Draw/MouseMove/MouseKey/Keyboard/SystemEvent сохранены;
материал/свет Enter и device calls направлены в настоящие typed providers.
Набор virtual-key чисел сверяется static_assert с исходным input.hpp и SDK.

RunOriginalNativeLoop сохраняет исходный порядок: platform pump, exit/active/
form guards, elapsed delta, 4-speed, clamp100ms, logger/reminders, form Takt,
form Draw, fps_counter и AvailableTextureBytes. Monotonic steady_clock заменяет
зависимый от реализации high_resolution_clock. Остальное тело сравнивается с
оригиналом после обратного раскрытия платформенных вызовов.

Switch pump вызывает настоящий SwitchWindow::Update: applet notifications,
HID router и EngineSink отправляют события теперь уже выбранной original form.
Inactive yield использует svcSleepThread(YieldType_WithoutCoreMigration) из
установленного libnx. APPACTIVE берётся из настоящего applet focus; Win32
timeBeginPeriod и принудительная активация не выдаются за native OS операции.

GPU acquire выполняется после Takt/recovery. Обёртка цикла не добавляет Clear:
все очистки принадлежат исходному Draw. Его Present выполняется один раз;
RAII rollback quiesce/cancel защищает выход по ошибке. Пока video resources не
готовы, исходный Draw сам пропускает работу, display image не захватывается.
При выходе Scope снимает loop backend, FormChange(NULL)/HDelete освобождают
форму до CGame::Deinit и detach Window/Device.

Standalone font diagnostic больше не является обязательной сценой launcher.
Оригинальные CMatrixDebugInfo/font creation/draw/lost/reset paths сохранены и
слинкованы через карту/DekoDevice. Minimal bootstrap остаётся отдельной целью.

Native ClientToScreen использует координаты настоящего borderless NWindow;
fullscreen ClipCursor задаёт реальный input extent и ограничивает pointer.
Эти Switch операции пока подтверждены SDK compile/link, не аппаратным вводом.

## Исполненные проверки

| Профиль | Доказательство |
| --- | --- |
| COMPILE/LINK | Полная devkitA64 ELF с Init, original form/vtable, L3GRun, map/side simulation, CalcVis/CalcMapGroupVisibility и deko providers; 0 strong undefined. Inputs считаются по достижимым object/archive nodes Ninja graph, без старых orphan probe objects и внешних SDK libraries. |
| HOST ASan/UBSan | Настоящий original loop с исходными portable Form/Reminder и явными CPU fixtures: delta, 4-speed, cap100, inactive/no-form, Reminder-before-Takt, Draw/FPS/residency, exit, исключения Pump/Takt/Draw, повторное bind. Отдельно production native scalar CDeviceState и COW/retention/OOM regression прошли, включая отказ light query и запрет restore неполного capture. Это не карта/GPU. |
| ARM64/QEMU | Те же scheduler и scalar/lease tests с assertions прошли. Не Switch/libnx/GPU runtime. |
| WINDOWS HAL | Две настоящие сессии Init/PrepareMap → original form → original loop с восемью Draw. EngineSink events: Enter закрывает начальный диалог, Pause переключает паузу, Escape открывает/закрывает меню, scroll двигает камеру, MouseMove меняет cursor. Контролируемый test clock25ms даёт 125ms неприостановленного игрового времени на цикл. Затем SystemEvent deactivation освобождает video resources, activation + original Takt/CheckLostDevice пересоздаёт их, Draw возвращается. Ещё один исходный Draw сохраняет readback. Deinit/restore и обе form chains чисты. |
| WINDOWS SDK oracle | Новые typed Get/SetLight, enable, Get/SetMaterial побайтно сверены с независимым D3D9 HAL GetLight/GetMaterial. Проверены все raw fields, Specular/Power, raw enable и неизменность настоящего HRESULT отсутствующего слота. Это не эмулятор GPU. |
| SWITCH HARDWARE | NOT RUN / HARDWARE_PENDING. Native Init, карта, GPU output, HID, fullscreen cursor, focus/recovery, FPS и бой на консоли не подтверждены. |

HAL: 10429 draw calls / 9690 indexed на сессию после Init; включает loop8,
recovery Draw и diagnostic readback Draw. Это короткий тест управления/жизни
ресурсов, не приёмка боя или производительности. Ввод подаётся test fixture
в EngineSink, не физическими клавиатурой/контроллером и не OS HID.

## Найденный и исправленный реальный blocker focus/recovery

Первое выполнение SystemEvent → ReleasePoolDefaultResources →
CDeviceState::StoreState остановилось на отсутствующем stored-light query.
Добавлены реальные Windows SDK Get/SetLight, GetLightEnable/LightEnable и
Get/SetMaterial, без выдуманных значений или фиктивных HRESULT.

Затем HAL воспроизвёл D3DERR_INVALIDCALL для ещё не назначенного light slot.
Оригинал заранее обнулял light Type и игнорировал такой GetLight результат;
прежний portable strict-check превращал штатное отсутствие в ошибку capture.
Теперь CaptureLightSlot явно представляет отсутствие как Type0/disabled,
только если оба реальных SDK запроса сообщают INVALIDCALL для допустимого
индекса. Прямые GetLight/GetLightEnable сохраняют исходные HRESULT и не
меняют output при отказе. Прочие ошибки прерывают capture; неполная группа
не может восстанавливаться. Restore сохраняет original Type0 rule: SetLight
пропускается, LightEnable выполняется. Fake SDK success не используется.

[Microsoft LightEnable](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-lightenable)
описывает создание default definition при Enable/Disable не назначенного
индекса; наличие/ошибки и raw payload здесь дополнительно проверены самим HAL.
Baseline failure log/exe/result сохранены в focus-light-baseline.

Из прежних 253 generated файлов изменён только MatrixLib/3G/DeviceState.cpp
на явный light-slot capture; добавлены OriginalNativeForm.cpp и
OriginalNativeLoop.cpp. Upstream не менялся. Конструктор, деструктор, Leave,
Takt и handler control flow сверяются с оригиналом адресным аудитом.

## Открытые границы

F9 screenshot остаётся явным Unsupported из CFormMatrixGame::Keyboard.
Оригинальная screenshot функция сохранена в original TU; native provider ещё
не подключён. Проверяется именно отказ, не пустой файл/успешный screenshot.
Этот необязательный путь не объявляется блокером первой карты.

Первый следующий runtime отказ Switch не наблюдался: hardware execution нет.
Full map release/reset/recreate и отказ самой GPU queue требуют отдельной
native проверки; Windows controlled focus cycle не доказывает OS device loss
или Switch recovery. Нужны настоящие map load/первый кадр на консоли, затем
управление боем, результат и интеграция с основной игрой. Пока доступен кодовый
аудит оставшихся реально вызываемых native render/resource путей.

Команды: tools/matrixgame_form_checks.py; tools/matrixgame_loop_cpu_checks.py
host/arm64; tools/matrixgame_form_state_checks.py host/arm64;
tools/matrixgame_form_windows.py; tools/matrixgame_form_audit.py.
Результаты: build/matrixgame/e2e533-form и matrixgame-e2e533-form-evidence.json.
Игровые входы только читаются, generated cache/output/readback находятся в build.

NRO, frozen, ресурсы, аппаратные входы и сейвы сохраняются. P0, причинная роль
GI-кэша в повторяющемся тексте, manual-publish ENOENT и старые Switch hangs OPEN.
