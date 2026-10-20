# Управление Switch — рабочая матрица E2E-5.15

Статус: **PARTIAL**, 20.10.2026. Это текущая реализация и список незакрытых
привязок, а не утверждение о готовом консольном интерфейсе. Аппаратной проверки нет.

## Реализованная граница

`RawInput → controller_actions::Router → Snapshot.actions → native UI / legacy adapter`.
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
SetArcadeControls удалены из generated source. MessageLoopStackGuard в обоих loops
удаляет только свой незакрытый stack entry при исключении; нормальный Pop не
повторяется. На настоящем стеке проверено десять nested exception cycles; отдельный
fault-injection из ресурсного OnOpen ещё OPEN. Инвентарный drag и HOME/suspend
требуют дальнейшей интеграции. Matrix contexts в настоящий бой не подключены.

## Кнопки в текущем адаптере

| Орган | Логическое действие | Реальная доставка сейчас |
|---|---|---|
| A / touch | Confirm | A в focus вызывает настоящий UI-handler; touch и cursor используют единый mouse down/up |
| B | Cancel | Escape down/up; прежний right click убран |
| Y | OpenContextAction | Right mouse down/up; исходные контекстные операции доступны |
| X | Inspect | В focus — HelpCallback выбранного объекта, если есть; в cursor — прежний Enter fallback |
| + в аркаде | OpenPause | VK_PAUSE → TfAB::BattleKeyDown → SimulationPaused toggle |
| + в главном меню | OpenPause | Безопасный no-op, меню уже открыто; выход не вызывается |
| + на остальных экранах | OpenPause | Временный Escape fallback; полноценное открытие меню для всех экранов OPEN |
| B в ArcadePause | BackToGame через Cancel | VK_PAUSE, возврат к симуляции; стрельба в паузе не доставляется |
| A в ArcadeResult | Confirm | Исходный Space, без одновременного mouse-down; touch остаётся указателем |
| − в аркаде | ToggleAutopilot | Исходный A → ToggleAutopilot |
| ZR / ZL в аркаде | PrimaryFire / SecondaryFire | Исходные Control / Space |
| L / R в аркаде | CyclePrimaryWeapon / CycleSecondaryWeapon | Исходные OEM [ / ]; оба циклически вперёд по своей группе, не «предыдущее/следующее» оружие |
| D-pad в аркаде | MoveForward/Backward, TurnLeft/Right | Исходные стрелки |
| Левый / правый стик в аркаде | Курс корабля / курсор | Существующий Steer и виртуальный курсор; независимый twin-stick огонь не заявлен |
| Стик / D-pad вне аркады | Курсор / Navigate | Hybrid: левый стик/D-pad — spatial focus; правый стик/touch — cursor. Cursor: левый стик — указатель, D-pad возвращает focus |
| ZR вне аркады | StartDrag / EndDrag | Удержание левой кнопки в cursor; сценарии инвентаря ещё OPEN |
| L / R вне аркады | PrevTab / NextTab | В focus — переход влево/вправо; специальные вкладки экранов ещё OPEN |
| L3 | ToggleNavigationMode | Focus ↔ cursor, отмена старого указательного удержания без завершающего клика |
| R3 | OpenControllerSettings | Штатное модальное окно GI_MessageBox с настройками контроллера |

`+` не создаёт WM_QUIT и не вызывает QuitClicked в главном меню. В аркаде B
сохраняет RequestExit с подтверждением, а + использует настоящий
обработчик паузы. Удалён отдельный автоматический выход по PLUS в runtime_loop_slice.
Quicksave остаётся существующим сочетанием ZL+ZR+Minus; одиночный Minus его не вызывает.
ArcadeGameplay/Pause/Result выбираются по настоящим SimulationPaused и Active
VictoryPanel/DefeatPanel. Контекст обновляется перед poll и сразу после pause toggle;
фоновой аркаде запрещено перехватывать ввод у открытого модального окна.

## Общая экранная клавиатура

Все настоящие TEditGI используют один hook после установки фокуса при нажатии
внутри поля. Специальные UpCallback overlays двух полей удалены. Сохраняются
MaxLength в UTF-16 units, исходные HasGlyph/AcceptCharCallback и уведомление об
изменении; отклонённая строка сохраняет прежний текст/каретку. Native backend
использует swkbdCreate/Show/Close с освобождением при отказе. TextEntry и
SwitchAppletActive — отдельные scopes, возврат подавляет старый A/touch до отпускания.
Один press_id не открывает несколько клавиатур на перекрывающихся полях.

Host backend внедряемый; без него обычный host keyboard driver сохраняется.
Проверены русский/английский, Ё, пробелы, длинные строки, Cancel, некорректный UTF-16,
исключение и восстановление контекста. На настоящем TEditGI проверен Cancel без
изменения текста/каретки. Принятие через реальную OSK, font coverage и удобство всех
полей остаются HARDWARE PENDING; полный ресурсный controller-only route OPEN.
Защитная граница буфера — 4096 UTF-16 units; больший MaxLength отвергается явно,
без усечения текста. Это не утверждение о лимите/поддержке самой системной OSK.

## Аудит фактических handlers: первая часть

Пути относительно `upstream/cpp/src`. Наличие handler означает обнаруженную точку
доставки, но не готовность новой semantic mapping. Два профиля и общий focus
реализованы; специальные механизмы экранов требуют проверки по строкам ниже.

| Экран | Исходное действие | Целевая команда | Handler | Текущий статус / проверка |
|---|---|---|---|---|
| Главное меню | Enter / Escape / нажатие кнопки | Confirm / Cancel | fMainForm.cpp: MainPanelKeyDown, QuitClicked | Native focus + прямые callbacks; пять ресурсных A/New/B циклов PASS; остальные пункты требуют route |
| Игровое меню | Escape, S/F2, L/F3, C, E, H | BackToGame, SaveLoad, Options, Help | fGameMenu.cpp: MainPanelKeyDown | Fallback; direct actions OPEN |
| Ангар | Space, F, A, B, S; right click | EndTurn, takeoff, repair, refuel, inspect, context | fHangar.cpp: MainKeyDown, MainRightButtonDown, TakeOffClicked | ButTakeOff через native focus/UpCallback → карта PASS; остальные операции требуют route |
| Планета | Space, Q, панель | EndTurn, quest, Confirm | fPlanet.cpp: MainPanelKeyDown | Pointer fallback; direct actions OPEN |
| Карта | Keys, right click, wheel | IssueCommand, OpenContextAction, Zoom | fStarMap.cpp: MapKeyDown, MapRightButtonDown, ProcessMouseWheel | Pointer + прежние swipe; новые actions OPEN |
| Оборудование/магазин | Keys, wheel | Inspect, Scroll, drag, tabs | fEquipmentShop.cpp: MainPanelKeyDown, ProcessMouseWheel | Pointer fallback; drag/tab/focus audit OPEN |
| Торговля | Right click, keys | OpenContextAction, trade | fGoodsShop2.cpp: GoodsRightMouseDown, MainPanelKeyDown | Y/right-click fallback; операции покупки/drag OPEN |
| Диалог | Click choice, wheel, keys | Confirm, Scroll, Cancel | fTalk.cpp / fRuinsTalk.cpp: ChoiceMouseDown, ChoiceMouseUp; ProcessMouseWheel | Native paired handlers для строк TalkPA; четыре настоящих выбора персонажа PASS; длинные списки/scroll OPEN |
| Save/load | Click, double click, key, wheel | Confirm, Inspect, Scroll, TextEntry | fSaveManager.cpp: SlotMouseDown, SlotDoubleClick, SlotKeyDown, ProcessMouseWheel | Native slot/edit/ButSave/ButLoad: Unicode save после взлёта и новый процесс load→map PASS; fault injection/длинные списки OPEN |
| Редактируемые поля | Click edit, key | OpenTextKeyboard, Confirm, Cancel | GI_Edit.cpp: TEditGI::ProcessLeftButtonDown; исходные Changed/AcceptChar callbacks | Общий hook с MaxLength; в том числе fSelectFace, fGameSettings2, SaveManager; native OSK hardware PENDING |
| Настройки новой игры | Click name, key, wheel | TextEntry, Navigate, Scroll | fGameSettings2.cpp: PlayerNameMouseDown, MainPanelKeyDown, ProcessMouseWheel | Native focus, Unicode имя/reopen/Cancel и Ok→генерация→диалог PASS; остальные поля требуют route |
| Аркада | Control/Space/arrows/[ / ]/A/Pause/Escape | Fire, Move, Weapon, Autopilot, Pause, Cancel | ab_MainForm.cpp: BattleKeyDown, BattleKeyUp, RequestExit | Прежние команды + новая безопасная PLUS-пауза; unit/adapter PASS, hardware PENDING |
| MatrixGame | Ввод настоящего движка | MatrixRTS / MatrixDirectControl | Robot.cpp, port/matrixgame/native | Исполнение не подключено; mapping contract не является боем |

Остаётся проаудировать остальные экраны, hover/double-click/drag lifetimes,
модальные окна и все редактируемые поля. Длинные списки с прокруткой, letterbox,
экранное сообщение для single Joy-Con и актуальные подсказки — OPEN.

## Два профиля и общий focus

Hybrid по умолчанию выбирает реальные доступные кнопки и TEditGI. Граф строится
по текущим hit bounds и проверяет Active, HitTestDisabled, occlusion и disabled
состояние графических кнопок. D-pad/левый стик выбирает соседний объект; при
границе предусмотрен переход к противоположному краю. Выбранный объект отмечен
жёлтой рамкой в настоящем RGB565 framebuffer. Вложенный loop сохраняет выбор
родителя. Главное меню первоначально выбирает New.

Confirm вызывает исходный callback / ExecuteOnPressCode либо общую OSK для
TEditGI. Fixed toggle сохраняет исходное Down-состояние; обычные кнопки с двумя
hold callbacks пока исключены из графа и доступны указателем. Общий spatial
focus не считается готовой специальной навигацией каждого списка/инвентаря.

Контейнеры ContentPanel/RootUiObject не становятся самостоятельными focus targets
из-за общего mouse-handler. Для строк TalkPA в настоящих TfTalk/TfRuinsTalk
действует адресная привязка: оригинальный visual-down, затем transactional-up.
Choice разрешается через owner-scoped e2e5_pointer_payload, как в effective
исходниках диалогов; не через указатель, усечённый до UserValue. Строки без
Callback/FallbackCallback не выбираются. Остальные dual-edge controls сохраняют
cursor fallback. После up row больше не используется: callback может её удалить.

Активация TEdit устанавливает исходный FocusedControl. В SaveManager при смене
слота сначала вызывается штатный callback родительского slot panel, как при
мыши, затем фокус поля и OSK. Уже выбранный слот не refresh-ится повторно:
это сохраняет черновик имени при reopen/Cancel, SelectedSlot и доступность Save.

Cursor использует левый стик для указателя. В обоих профилях L3 переключает
режим, D-pad возвращает focus, правый стик/touch выбирает указатель. Старое
удержание A/ZR при переключении отменяется без клика; новый физический press
доставляется после отпускания. В cursor ZR объединён с A/touch в один срок
жизни левой кнопки. Аркада сохраняет отдельное управление.

R3 открывает штатный GI_MessageBox. D-pad вверх/вниз выбирает строку, влево/вправо
меняет профиль, deadzone, скорость, инверсию X/Y, ускорение, перестановку A/B.
X сбрасывает редактируемые значения к defaults; Confirm применяет и сохраняет,
Cancel отменяет. Окно настроек всегда использует focus, чтобы X/reset и Confirm
оставались доступны без мыши в Cursor-профиле при нейтральных стиках. Действующие A/B отражены в
подсказке. Ускорение интегрируется
по времени; одинаковый интервал даёт одинаковое перемещение при 20/30/60 FPS.

Настройки находятся в user config/controller.ini (отдельно от игровых сейвов).
Чтение ограничено 1024 байтами, ошибочные версии/значения отклоняются целиком;
при повреждении используется controller.ini.bak. Запись проверяет временный
файл, сохраняет прежние байты в backup и проверяет результат перезаписи.
При отказе записи параметры действуют до выхода и показывается сообщение.
Проверены первоначальная запись, перезапись и отказ открытия .tmp; это не
доказательство устойчивости к отключению питания во время записи на SD.

Host test native-focus доставляет настоящий RawInput через message pump и
effective ProcessWindowMessage, проверяет прямые callbacks, исключение disabled/
hidden, framebuffer-рамку, отсутствие held/replay click, cursor fallback,
восстановление родительского выбора и отказ записи настроек. Отдельный
ресурсный runner: tools/e2e515_controller_route.py; область доказательства
указывается в его result.json. --campaign прошёл путь main menu → NewGame →
русское имя «Пилот Ёж» → повторная OSK/Cancel → генерация → четыре настоящих
диалоговых выбора → ангар → взлёт → orders mode карты. Host backend подменяет
только системную клавиатуру; обработчики, поля, генерация и кампания настоящие.
Системная OSK и удобство управления на Switch остаются HARDWARE PENDING.

--campaign --save дополнительно проходит PM_Logo → Save → slot/edit → OSK
«Проба Ёж» → reopen/Cancel → ButSave после взлёта. Успех требует физический
Unicode-файл и marker завершённой worker-записи/readback, а не наличие .sav
во время копирования. --load-save принимает только собственную build fixture,
копирует её в новый user root и новым процессом проходит Load → Slot0 → ButLoad
→ загрузка → orders mode карты. Сравнение всех полей world/RNG checkpoints и
реальный 346-й ход не закрываются этим маршрутом.

Host mailbox подтверждает команду только после её нормализации в PollHistory,
включая нейтральное отпускание. Иначе release во время тяжёлого OnOpen мог быть
заменён следующим press до чтения UI, и штатный anti-replay корректно блокировал
новый A. Атомарный focus.txt — opt-in host protocol; на Switch этот путь исключён.

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
