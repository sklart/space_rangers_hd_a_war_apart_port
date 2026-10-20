# Управление Switch — рабочая матрица E2E-5.15

Статус: **PARTIAL**, 20.10.2026. Это текущая реализация и список незакрытых
привязок, а не утверждение о готовом консольном интерфейсе. Аппаратной проверки нет.

## Реализованная граница

`RawInput → controller_actions::Router → Snapshot.actions → legacy adapter`.
Ядро не зависит от libnx, Win32 или движка. Оно выдаёт pressed/held/released/repeat.
Владелец передаёт Context и поколение; даже повторный вход в тот же Context должен
иметь новое поколение. Кнопки, удержанные при смене владельца/контроллера, блокируются
до отпускания, стики — до нейтрального положения. История HID прежнего владельца
отбрасывается. Одновременные touch/A дают одно Confirm; навигационный repeat имеет
задержку 300 мс и интервал 90 мс, без пачки повторов после остановки кадра.

Все требуемые Context объявлены. Оба настоящих `TMessageLoopGI::Run/RunContinuous`
подключают InputContextScope по identity зарегистрированного экрана; незарегистрированный
дочерний loop получает ModalDialog. Предыдущий контекст восстанавливается по RAII.
Очередь старых keyboard/mouse событий очищается; прежний владелец получает отмену
удержаний. Нормальные кнопки сбрасывают Down без mouse-up/click, fixed toggle
сохраняет выбор. В аркаде сбрасываются реальные movement/fire fields. Отмена
выполняется перед OnClose, до освобождения ресурсов. Старые overrides OnOpen/OnClose
SetArcadeControls удалены из generated source. Инвентарный drag, OSK, HOME/suspend
и динамические ArcadePause/ArcadeResult требуют дальнейшей интеграции. Matrix
contexts в настоящий бой не подключены. RAII-тест не доказывает восстановление
всего оригинального MessageLoopStack после исключения из самого Run — этот путь OPEN.

## Кнопки в текущем адаптере

| Орган | Логическое действие | Реальная доставка сейчас |
|---|---|---|
| A / touch | Confirm | Один левый mouse down/up; курсорный fallback |
| B | Cancel | Escape down/up; прежний right click убран |
| Y | OpenContextAction | Right mouse down/up; исходные контекстные операции доступны |
| X | Inspect | Сохраняется прежний Enter fallback; семантика Inspect ещё не привязана к экранам |
| + в аркаде | OpenPause | VK_PAUSE → TfAB::BattleKeyDown → SimulationPaused toggle |
| + вне аркады | OpenPause | Временный Escape fallback; полноценное открытие меню для всех экранов OPEN |
| − в аркаде | ToggleAutopilot | Исходный A → ToggleAutopilot |
| ZR / ZL в аркаде | PrimaryFire / SecondaryFire | Исходные Control / Space |
| L / R в аркаде | CyclePrimaryWeapon / CycleSecondaryWeapon | Исходные OEM [ / ]; оба циклически вперёд по своей группе, не «предыдущее/следующее» оружие |
| D-pad в аркаде | MoveForward/Backward, TurnLeft/Right | Исходные стрелки |
| Левый / правый стик в аркаде | Курс корабля / курсор | Существующий Steer и виртуальный курсор; независимый twin-stick огонь не заявлен |
| Стик / D-pad вне аркады | Курсор / Navigate | Сохраняется курсорный fallback; focus graph OPEN |
| ZR вне аркады | StartDrag / EndDrag | Только контракт ядра; доставка drag в UI OPEN |
| L / R вне аркады | PrevTab / NextTab | Только контракт ядра; доставка в UI OPEN |

`+` не создаёт WM_QUIT. В главном меню временный Escape доходит до QuitClicked,
который требует исходное подтверждение ShowMessageBoxGI; это ещё не целевое меню
паузы. В аркаде B сохраняет RequestExit с подтверждением, а + использует настоящий
обработчик паузы. Удалён отдельный автоматический выход по PLUS в runtime_loop_slice.
Quicksave остаётся существующим сочетанием ZL+ZR+Minus; одиночный Minus его не вызывает.

## Аудит фактических handlers: первая часть

Пути относительно `upstream/cpp/src`. Наличие handler означает обнаруженную точку
доставки, но не готовность новой semantic mapping. Без мыши здесь пока доступен
виртуальный указатель; два переключаемых профиля ещё не реализованы.

| Экран | Исходное действие | Целевая команда | Handler | Текущий статус / проверка |
|---|---|---|---|---|
| Главное меню | Enter / Escape / нажатие кнопки | Confirm / Cancel | fMainForm.cpp: MainPanelKeyDown, QuitClicked | Mouse/Enter/Escape fallback; unit message pump; focus OPEN |
| Игровое меню | Escape, S/F2, L/F3, C, E, H | BackToGame, SaveLoad, Options, Help | fGameMenu.cpp: MainPanelKeyDown | Fallback; direct actions OPEN |
| Ангар | Space, F, A, B, S; right click | EndTurn, takeoff, repair, refuel, inspect, context | fHangar.cpp: MainKeyDown, MainRightButtonDown | Pointer/right-click fallback; shortcuts OPEN |
| Планета | Space, Q, панель | EndTurn, quest, Confirm | fPlanet.cpp: MainPanelKeyDown | Pointer fallback; direct actions OPEN |
| Карта | Keys, right click, wheel | IssueCommand, OpenContextAction, Zoom | fStarMap.cpp: MapKeyDown, MapRightButtonDown, ProcessMouseWheel | Pointer + прежние swipe; новые actions OPEN |
| Оборудование/магазин | Keys, wheel | Inspect, Scroll, drag, tabs | fEquipmentShop.cpp: MainPanelKeyDown, ProcessMouseWheel | Pointer fallback; drag/tab/focus audit OPEN |
| Торговля | Right click, keys | OpenContextAction, trade | fGoodsShop2.cpp: GoodsRightMouseDown, MainPanelKeyDown | Y/right-click fallback; операции покупки/drag OPEN |
| Диалог | Click choice, wheel, keys | Confirm, Scroll, Cancel | fTalk.cpp: ChoiceMouseDown, ProcessMouseWheel, MainPanelKeyDown | Pointer fallback; focus OPEN |
| Save/load | Click, double click, key, wheel | Confirm, Inspect, Scroll, TextEntry | fSaveManager.cpp: SlotMouseDown, SlotDoubleClick, SlotKeyDown, ProcessMouseWheel | Pointer fallback; полный OSK/списки OPEN |
| Имя персонажа | Click edit, key | OpenTextKeyboard, Confirm, Cancel | fSelectFace.cpp: PlayerNameMouseDown, MainPanelKeyDown | Существующая OSK; все редактируемые поля ещё не проаудированы |
| Настройки новой игры | Click name, key, wheel | TextEntry, Navigate, Scroll | fGameSettings2.cpp: PlayerNameMouseDown, MainPanelKeyDown, ProcessMouseWheel | Pointer fallback; настройки контроллера OPEN |
| Аркада | Control/Space/arrows/[ / ]/A/Pause/Escape | Fire, Move, Weapon, Autopilot, Pause, Cancel | ab_MainForm.cpp: BattleKeyDown, BattleKeyUp, RequestExit | Прежние команды + новая безопасная PLUS-пауза; unit/adapter PASS, hardware PENDING |
| MatrixGame | Ввод настоящего движка | MatrixRTS / MatrixDirectControl | Robot.cpp, port/matrixgame/native | Исполнение не подключено; mapping contract не является боем |

Остаётся проаудировать остальные экраны, hover/double-click/drag lifetimes,
модальные окна и все редактируемые поля. Неактивные/скрытые элементы, видимый фокус,
профили Hybrid/Cursor, настройки deadzone/sensitivity/inversion/reset defaults,
letterbox, экранное сообщение для single Joy-Con и актуальные подсказки — OPEN.

## Контроллеры и доказательства

SDK-адаптер читает PadState и padIsConnected/padGetStyleSet; различает handheld,
JoyDual и FullKey. Неполный одиночный Joy-Con классифицируется Unsupported и не
передаёт игровые кнопки. UI-сообщение об этом ещё не реализовано. Host позволяет
внедрять connected/controller_style; смена стиля с удержанными A/стиком проверяется
на host, а удобство, dock/undock и HID-источники требуют Switch.
Отключение/смена источника в message pump отменяет старого владельца без mouse-up
по UI; переподключение с удержанным A не подтверждает окно. Этот путь отдельно
проверен через настоящий message queue с ASan/UBSan.

В журнале edges: time_us, source, style, context, action, edge, target, window.
`target=legacy-adapter` фиксирует вход в адаптер; для объявленных, но ещё не
привязанных действий это **не** доказательство выполнения UI-команды.

Focused suite: `python3 tools/e2e515_input_tests.py --sanitize`.
Message pump: `python3 tools/e2e_win32_host_test.py --only window-message-input --sanitize`.
Настоящие CPU-объекты TfAB/TObjectGI/TGraphButtonGI, очередь и effective scopes:
`python3 tools/e2e515_engine_tests.py`. Это не исполнение боя и не полный route
с контроллером по ресурсным экранам.
