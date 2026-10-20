# E2E-5.15 — журнал офлайн-валидации

20.10.2026. **IN PROGRESS / PARTIAL. Цель не завершена.**

Последнее обновление: пакет 11, владельцы снимков и реальные переходы экранов. Разделы
ранних пакетов сохраняют их исторические границы; сводная таблица в конце
отражает текущий остаток требований.

Исходное ТЗ сохранено только в ignored `build/e2e515/task-specification.txt`,
SHA-256 `8837b62d493b6ae7b74bb22fd4e857cffe89553dfb15b0fd45c57856edbdfdbd`.
Требования не сужены до уже пройденных проверок; остаток перечислен ниже.

## База и сохранность

- HEAD до начала: `4323d1b3e8a1e880840574957e1f5eedf7d8c139`.
- Сохранённый origin/master и успешный live ls-remote:
  `353e386408ee2c06e42d976f5fe0706ddce0274a`. Два локальных коммита E2E-5.14
  отсутствуют на этой удалённой вершине. Push не выполнялся.
- До начала существовали untracked `saves/` и
  `reports/e2e5/e2e510-artifact-provenance.json`; они не входят в изменения.
- Pin cpp `57fa689c630193a66fdea6ca4c79a188814991cd`, okgf
  `c01aa7a168a6f1772541501074b7bba1b96550ef`, MatrixGame
  `218115a72fa9c5d43269f4ca84a8596a0bcb82f1`; upstream не редактируется.
- `build/e2e515/data-baseline.json`: восемь найденных оригинальных файлов
  (пользовательское дерево saves и G:/pack*.zip), размеры и потоковые SHA-256.
  Повторная проверка PASS. Скрипт `tools/e2e515_data_baseline.py` не пишет в эти пути
  и не перезаписывает baseline при расхождении. При временном отказе сети lookup
  не препятствует фиксации данных, но отмечается UNAVAILABLE, а не live PASS.

## Пакет 1: основание ввода

Добавлен независимый `controller_actions::Router`, подключён в настоящий
`input_platform::NormalizeAt`, а не только в fixture. Контексты/поколение,
pressed/held/released/repeat, запрет replay удержанных кнопок/стиков и HID history,
touch/A coalescing, источник контроллера и отключение. Существующие poll/history,
скорость курсора и классическая физика аркады сохранены.

Реальный Win32 adapter: B теперь Escape, Y — right click; PLUS больше не создаёт
WM_QUIT. Для аркады PLUS вызывает штатный VK_PAUSE, прочим экранам пока доступен
Escape fallback. Удалён автоматический PLUS-exit из runtime_loop_slice.
Целевой UI pause/focus/стек окон пока не завершён. Точная матрица и остаток:
[управление](../../docs/switch-controller-controls.md).

Основание ввода зафиксировано отдельным локальным коммитом
`9247efb745c8bdae05fd922b32d17ac3a281c091`. Следующий пакет привязывает владельца
к обоим реальным loops: `controller_screen_context.hpp`, InputContextScope,
очистка очереди и отмена у старого владельца, восстановление по RAII. Старые
OnOpen/OnClose overrides SetArcadeControls больше не перехватывают контекст.
Новые реальные методы игры не подменяются: callback отмены сбрасывает input fields
и отпускает исходные key callbacks; mouse-up в новое окно не отправляется.
Полное восстановление оригинального MessageLoopStack при исключении из Run
ещё не доказано; проверка Scope отдельно этого не закрывает.
Исходный код этого пакета зафиксирован локально как
`be53075d0eabe95c500fbfe65f6d748c55e9724d`. История сохраняет оба E2E-5.14 commits,
никакого squash/rebase/смены pin или push не было.

### Текущие доказательства

| Проверка | Результат и граница |
|---|---|
| Semantic router + семь старых input suites | 8/8 PASS, Linux ASan/UBSan; 24 context boundaries, touch, repeat, disconnect/style switch, history; build/e2e515/input-asan/result.json |
| Win32 wrappers | handles, filetime, window-message-input, resolver, original-wrappers PASS на основании ввода; новый message-input ASan/UBSan PASS: queued old A, три nested scopes, same-context owner, exception, disconnect/reconnect без click-up |
| Полный эффективный host engine | 296 исходных игровых units, 34 platform, 331 link objects; успешная сборка/линковка; не реальный маршрут с контроллером |
| Switch SDK input | input object отдельно PASS с -Wall/-Wextra/-Werror и SDK headers через -isystem; затем input_platform.cpp, win32_compat_messages.cpp и настоящий effective GI_MessageLoop.cpp COMPILE PASS со штатными Save ON flags; build/e2e515/sdk-input/result.json; не full link/runtime |
| ARM64 save-test preflight | PASS после обновления effective manifest: 254 imports, 45 dynamic calls, UNKNOWN=0; не full release build |
| Другие существующие профили | Стандартный release и combined Save OFF preflight PASS; manifests обновлены только по числу файлов/строкам callsites. Первые проверки ожидаемо выявили stale metadata, затем выполнена регенерация/повтор |
| Полный host: 3 focused tests | PASS: runtime 120+120 кадров и PLUS без выхода; 10 canvas cycles с nested exception; 10 отмен/восстановлений на настоящих TfAB/TObjectGI/TGraphButtonGI, fixed toggle сохранён. Проверено наличие двух scoped sites и двух CancelHeldInput перед OnClose в effective sources; build/e2e515/engine-tests/result.json. Реальные screen-transition маршруты ещё OPEN |
| Python source/build tools | 25/25 PASS в WSL; Windows запуск получил WinError 5 в Temp, поэтому его ошибки не объявляются игровыми отказами |
| Static pointer inventory | PASS guard: new_unclassified_count=0; старые 2401 unresolved/30 confirmed narrowing не закрываются этим результатом |
| git diff --check / original data | PASS на этапе основания ввода; повторить перед финальным коммитом |

Первая попытка focused engine linker получила конфликт имён временных objects:
`runtime-loop.o` использовался и реализацией, и test main. Инструмент исправлен
на `runtime-implementation.o`; это ошибка test setup, не краш движка.
Первый SDK compile с -I/-Werror останавливался на предупреждениях самих libnx
headers; -isystem сохранил строгую проверку собственного кода и дал compile PASS.

### Воспроизведение (Linux/WSL, из корня проекта)

```sh
python3 tools/e2e515_input_tests.py --sanitize
python3 tools/e2e_win32_host_test.py --only window-message-input --sanitize
python3 tools/e2e515_engine_tests.py
```

После preflight Save ON и наличия `compile-flags.txt` этого SDK-профиля:
`py -3 tools/e2e515_sdk_input_probe.py`. Инструмент берёт настоящие release flags,
проверяет effective manifest/оба lifecycle sites, сохраняет исходные/object/header
SHA-256; старый PASS инвалидируется перед запуском. Он не создаёт NRO и не
выдаёт objects за полный link. Первая попытка вне подходящей среды с отсутствующим
файлом flags должна считаться FAIL, а не готовой Switch-сборкой.

SDK compile текущего input:

```text
C:/devkitPro/devkitA64/bin/aarch64-none-elf-g++.exe -std=gnu++20 -O2 -Wall -Wextra -Werror -D__SWITCH__ -D__NEWLIB__ -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -isystem C:/devkitPro/libnx/include -I port/switch/platform -c port/switch/platform/input_platform.cpp -o build/e2e515/input_platform-switch.o
```

## Аудит требований: ещё не достигнуто

### Продолжение: keyboard/lifecycle/arcade contexts

Код этого пакета: `a3ff58c028b180256533d256713d6c44da573f8b` (локальный commit,
без push). Проверки выполнены на соответствующих рабочих source bytes до commit;
SDK source/header SHA повторно сопоставлены с сохранёнными fingerprints.

- Общий OSK hook теперь находится в effective `GI_Edit.cpp`, а не в двух отдельных
  callbacks. Все TEditGI получают свой MaxLength; сохранены HasGlyph,
  AcceptCharCallback и уведомления изменения. Unicode/Cancel/ошибки проверены
  внедряемым backend. Реальный системный applet на Switch **NOT TESTED**.
- Два scoped TextEntry/Applet владельца очищают очереди и удержания; проверены
  accepted/Cancel/throw и последующее новое A, без переноса старого нажатия.
- ArcadePause/ArcadeResult выбираются по реальным полям TfAB. B в паузе продолжает
  игру через исходный Pause handler, A на результате вызывает исходный Space
  без mouse-down; + в главном меню безопасно игнорируется. Новых правил боя нет.
- MessageLoopStackGuard включён в оба effective Run; десять исключений между
  настоящими Push/Pop сохраняют родителя, нормальный Pop не дублируется. Ресурсный
  fault-injection из OnOpen ещё не выполнялся.
- Фактически обнаружены и исправлены два дефекта композиции: Refresh конфликтовал
  с точным ExitCode-first gate E2E-5.11; GI_Edit отсутствовал в whitelist генератора.
  Проверки effective sources не позволили объявить эти Python-only изменения PASS.
  Новый regression test воспроизводит второй случай.
- Текущий набор: **9/9 input/text suites ASan/UBSan**, новый message-input ASan/UBSan,
  **26/26 Python tools**, три focused real-host tests. SDK **5/5 COMPILE PASS**:
  input_platform, win32_compat_messages, effective GI_MessageLoop, GI_Edit,
  ab_MainForm; strong full link/NRO этим не проверены.
- Настоящий ресурсный маршрут с настройками архива прошёл:
  `22 → 1 → 33 → 3 → 20 → 4 → 22 → 16`, затем **пять** `25 → 16`.
  `build/e2e515/main-route/result.json`: route_complete=true, timed_out=false,
  source_configs_unchanged=true, host ELF SHA-256
  `efe63921699134dd407f1f97798b07c045a57aa9455db17973fa6ad24272271f`.
  Это прежний keyboard/mouse driver, **не controller-only PASS**. После контрольной
  точки процесс остановлен probe (`returncode=-9`), обычный shutdown не проверен.
- Просмотрен контактный лист 11 реальных кадров: меню, генерация/Introduction,
  диалог, ангар, карта и игровое меню имеют соответствующие подложки; чужой фон
  генерации в этих samples не наблюдался. Это визуальная проверка ограниченной
  выборки, не полный layer analyzer/все экраны и не аппаратное доказательство.

Воспроизведение ресурсного маршрута (Linux/WSL, законные локальные assets и копии CFG):

```sh
python3 tools/e2e515_main_route.py --game-root /path/to/game --config-root /path/to/copied/config
```

Процедура копирует настройки в отдельный stage, сохраняет hashes, log/frame data и
требует все стадии/пять циклов без markers ошибки отмены/stack unwind. Новые кадры
получают отдельный каталог; stale images не принимаются за свежий прогон.

## Пакет 4: профили и прямая навигация UI

Реализованы controller_profiles (Hybrid/Cursor, проверяемый codec и revision),
controller_focus (spatial graph), controller_messages и typed controller_ui.
Игровой код и адресные тесты зафиксированы локально:
`ee44a6fbc8a22bc9e350a341a5c350143ffe0295`; push не выполнялся.
Настоящие Run/RunContinuous создают UI Scope; ProcessWindowMessage обрабатывает
новые команды внутри исходного try, FinishQueuedDraw рисует рамку в RGB565.
Оба lifecycle sites, Process и Draw проверяются по effective generated source,
а не только по Python override. Доставка Confirm вызывает исходные callbacks
графических кнопок, toggle или общую OSK настоящего TEditGI. Указатель сохраняется
для сложных hold/drag controls; generic graph не закрывает аудит всех экранов.

Hybrid использует focus, Cursor — указатель; D-pad/L3/правый стик переключают
режим без replay удержаний. ZR в cursor объединён с A/touch в одну левую кнопку.
В focus A/X/Y не порождают второй mouse/Enter путь. Отдельно найден и устранён
двойной Y/right-click; native-focus проверяет ровно один callback без WM_RBUTTONDOWN.
Исправлена смена предпочтительного режима после modal teardown: Poll мог увидеть
новый профиль раньше Refresh родителя. Отдельный regression проверяет этот порядок.

R3 открывает штатный GI_MessageBox для профиля, deadzone, скорости, инверсии,
ускорения, перестановки A/B; X — reset, Confirm — apply/persist, Cancel — отмена.
Само окно параметров использует focus и в Cursor-профиле для восстановления
без мыши. controller.ini находится в user config, вне игровых сейвов; ограниченное
чтение, fallback .bak, temporary/readback/backup/copy-and-verify. При отказе
показывается ошибка, действуют параметры в памяти. Отключение питания/SD не проверено.

Новые проверки входят в существующие host workflow через два runner scripts:

- input-asan: 10/10 PASS, включая codec/graph/два профиля, режимы, отсутствие
  duplicate Y, ускорение 20/30/60 FPS за одну и три секунды.
- engine-tests: добавлен четвёртый native-focus с настоящими GI объектами,
  RawInput/message pump/effective handler, рамкой framebuffer, disabled/hidden,
  held/cancellation, parent focus и отказом записи настроек.
- SDK probe дополнен hashes четырёх новых headers и effective UI hook guards.
  Пять original/platform units проходят compile; full link/NRO этого пакета нет.
- Python tools: 26/26 PASS. Standard/combined Save OFF/Save ON preflight PASS;
  manifests обновлены по четырём headers и смещению callsite, UNKNOWN=0.
- Настоящая Win32 message/input очередь: повторный ASan/UBSan PASS после
  последней правки policy/доставки Y.
- SHA исходных восьми пользовательских файлов/архивов повторно совпали;
  pinned submodules чистые. Push не выполняется.

tools/e2e515_controller_route.py запускает отдельную копию host ELF с настоящими
ресурсами и копиями CFG. Единственный driver — mailbox RawInput; legacy
PostHostKey/PostHostLeftClick в этом режиме не запускается. Диагностические graph
snapshots создаёт UI thread; driver не читает живые игровые объекты. Пять циклов
NewGame/Cancel уже прошли: в каждом ровно один native Confirm, удержание A 900 мс
не запускает генерацию. Расширенная проверка настроек и Cursor recovery
также PASS: изменение скорости 600→650, Cancel без изменения файла, применение
Cursor, L3→native NewGame→B, R3→X→Confirm возвращает все defaults без мыши.
Текущий результат: build/e2e515/controller-route/result.json, host ELF SHA-256
`25ce0b883ed961df23817cf093b54d4ce936f7de22ecae8f5b9ddb4189a147f6`.
Отдельно обрабатывается временный ENODATA DrvFS при чтении лога. Ожидается
фактический poll кнопки; короткий pulse во время первого тяжёлого draw больше
не принимается за доставленное действие. После checkpoints runner останавливает
процесс; normal shutdown не доказан. Полный controller-only маршрут до карты OPEN.

## Пакет 5: native controller route до карты

Код и адресные тесты зафиксированы локально:
`cc70493a18367d46b795059ec81223d6751118b4`. Push не выполнялся.

Добавлен --campaign в tools/e2e515_controller_route.py. Этот режим отдельно
проходит настоящий ProgramMain, главную кнопку New, поле PlayerName, Ok,
генерацию мира, Continue, диалог персонажа, ангар и ButTakeOff. Путь между
controls планируется по опубликованному UI graph, но каждый шаг D-pad и каждый
actual selected ID проверяется по новому снимку. Прямых PostHostKey/мыши и
установки выбранного элемента из driver нет. Короткие menu/settings regression
cycles остаются отдельным режимом; повторять их для каждого кампанийного прогона
не требуется.

Host OSK backend установлен на UI thread до ProgramMain. Он подменяет только
системный applet и выдаёт контрольное «Пилот Ёж»; настоящие HasGlyph,
AcceptCharCallback, Changed callbacks и TEditGI остаются в цепочке. Повторный
запрос обязан получить это имя как initial value, затем backend возвращает
Cancel. Этот сценарий прошёл на реальном поле, не только на codec fixture.
Native swkbd на Switch по-прежнему HARDWARE PENDING.

По исходным fTalk/fRuinsTalk ChoiceMouseDown/Up добавлена адресная активация
строк TalkPA: down меняет визуальное состояние, up выполняет исходный выбор.
TfTalkA разрешается тем же owner-scoped token registry, что и в effective
исходниках. Варианты без Callback/FallbackCallback исключены, остальные
dual-edge controls не расширяются произвольно. Containers ContentPanel/Root
больше не считаются нажимаемыми focus targets из-за общего mouse callback.

Полный raw-pad маршрут до orders mode карты PASS: Unicode/reopen/Cancel,
генерация, четыре настоящих диалоговых callbacks, ангар, взлёт, карта.
Артефакт: build/e2e515/controller-campaign/result.json, host ELF SHA-256
`d33ee71b3e33a1d2a9a4150500bf8e3433a0e7833316822fe86ab5303643679c`.
Просмотрены шесть кадров этого прогона (menu/NewGame/generation/dialog/hangar/map);
в этой выборке чужой фон генерации не наблюдался. Contact PNG и 12 PPM находятся
в каталоге run 67292003c3db43308ca997826801470f. Это ограниченная визуальная
выборка, не layer analyzer всех экранов и не hardware proof.

Для SaveManager добавлены исходное выделение slot panel и FocusedControl перед
OSK. Повторная активация того же слота не вызывает SelectSlot/RefreshSlot заново:
ресурсный тест обнаружил, что это стирает черновик нового имени. Focused native
test проверяет первичный выбор и сохранение «Draft Ёж» при повторной активации.
Расширены те же четыре engine tests; SDK compile пяти units PASS. Ресурсный
--campaign --save проверяет настоящее сохранение после взлёта и ожидает marker
завершённой worker-записи, а не просто появление файла. Итог этого отдельного
сценария хранится в build/e2e515/controller-save/result.json. Он прошёл:
после взлёта создан физический «Проба Ёж.sav», 722160 байт, SHA-256
`82548d5aba68b54da29c3cffd251f19d1ac7a2a186a911f9dcd07f0f7adfaa4b`.
В игровом логе есть `manual save installed and readback verified` и финальный
`E2E5 manual save committed path=`. Имена в ANSI-логе могут отображаться через
вопросительные знаки; физическое имя и Unicode-путь проверены файловой системой.
Записывающий host ELF:
`e186037db4416f44d78cb398eb6adb26b5886ec034314f1c07259a79d51ecdf0`.

Отдельный --load-save принимает только fixture из build, проверяет копию по SHA
и создаёт новый user root/процесс. Native Load → Slot0 → ButLoad прошёл настоящий
load screen 24 и вернулся в orders mode карты 16. Результат:
build/e2e515/controller-reload/result.json, run 791c600ed9fa4d3ab03d26f4384d527e.
Исходный fixture SHA не изменился. Сравнение всех world/RNG checkpoints в этом
прогоне не выполнено; реальный 346-й ход, межзвёздный перелёт и fault injection
из C2 остаются отдельными обязательными работами.

При проверках устранены две ошибки test protocol: неполная append-log запись
graph snapshot теперь заменена отдельным focus.txt с atomic publish; mailbox
подтверждает raw snapshot только после PollHistory. Раньше neutral во время
тяжёлого OnOpen мог смениться новым A до чтения UI, и anti-replay правильно
блокировал его. Этот handshake host-only, физический HID Switch не изменяет.
В native-focus проверено, что ack отсутствует до pump и появляется после poll.

Финальные проверки данного пакета:

- 4/4 actual engine tests PASS; добавлены paired dialog handlers, disabled choice,
  исключение контейнера, первичный выбор save-slot и сохранение черновика при reopen.
- 1/1 Win32 message/input queue ASan/UBSan PASS после host handshake.
- SDK: 5/5 units COMPILE PASS, effective guards и headers hashes сохранены;
  full NRO/link этого цикла не выпускался.
- 3/3 standard / combined Save OFF / Save ON preflight PASS, UNKNOWN=0.
- Повтор menu/settings raw route PASS: 5 cycles, apply/cancel/reset, Cursor recovery.
- Fresh-process Unicode load PASS. Общий финальный host ELF этих двух прогонов:
  `b06100de96006aa0186c5e67e38d5b0ca18868a66aab690259bf9c40071f8692`.
  После записи сейва в нём изменился только host input acknowledgement/protocol;
  версия/сериализация сейва не менялась.
- Python runner AST и git diff --check PASS. Исходные 8 файлов/архивов и pins
  проверены отдельно; пользовательские saves и старый provenance не включаются.

Все runners останавливают процесс после checkpoints; normal shutdown в этих
прогонах не доказан. Для аппаратной Switch, настоящей swkbd и SD эти результаты
не являются PASS.

Воспроизведение после сборки optimized host (пути к законным assets и копиям CFG):

```sh
python3 tools/e2e515_controller_route.py --campaign --save --game-root /path/to/game --config-root /path/to/copied/config
python3 tools/e2e515_controller_route.py --load-save /path/inside/build/Проба\ Ёж.sav --game-root /path/to/game --config-root /path/to/copied/config
```

| Требование ТЗ | Состояние |
|---|---|
| A1 полная handler matrix | PARTIAL: первая часть в controls doc, остальные экраны/drag/hover/OSK OPEN |
| A2 Action/Context | PARTIAL: ядро, реальные scopes/очередь, OSK и динамические arcade contexts есть; suspend и полный controller-only route OPEN |
| A3 controllers | PARTIAL: классификация/сброс есть; UI unsupported, dock/resume и hardware OPEN |
| B два настоящих профиля, focus, RTS, touch, accessibility | PARTIAL: Hybrid/Cursor, spatial focus, прямые UI callbacks, параметры и рамка реализованы; полная матрица экранов/RTS/scroll/gestures OPEN |
| C1 все реальные screen transitions, фон | PARTIAL: исправлена потеря подготовленного snapshot через OnClose/вложенный preview; real generation/character/hangar/map и пять ship/menu/modal/return cycles, 34 frame checks PASS. Planet/shop/treatment/ruins/arcade и hardware OPEN |
| C2 save fault injection / новый процесс / перелёт | PARTIAL: реальный 346-й ход → три хода → UI отказ/повтор → Unicode save → новый процесс/checkpoint compare PASS; manual transaction fault/process-stop/retry PASS (пакет 6). Новый межзвёздный перелёт, реальные no-space/permission и SD остаются непроверенными |
| C3 generation → hangar → map controller route | Legacy real route PASS; raw pad menu/NewGame/settings и Unicode→generation→4 choices→hangar→takeoff→map PASS; остальные экраны OPEN |
| D смерть/взрыв, overload 20/40/70/150/250, parity 500/1000, три perf пары | PARTIAL: шесть real-arena death/overload сценариев завершили host кадры, explosion/result callbacks PASS; pixel progression, input latency, replay/parity 500/1000, три perf пары и lifecycle OPEN |
| E MatrixGame before/after units, новый real block, entry/resource pipeline | OPEN; текущий d3d9.h blocker ещё не закрыт |
| F1 CI input | Core sanitizer suite, реальные owner/canvas tests и message-input ASan/UBSan включены в host workflow; удалённый CI не запускался/не проверялся |
| F2/F3 runtime suites | PARTIAL/OPEN согласно строкам выше |
| F4 full release NRO + matching ELF | OPEN; preflight трёх профилей и current SDK compile девяти units PASS; финальный кандидат этого цикла не выпускался |
| Отчёты perf/MG, SHA/provenance и hardware checklist | Perf PARTIAL с сырыми CSV/JSON; MG, финальная provenance и hardware checklist OPEN |

Все аппаратные пункты остаются HARDWARE PENDING: фон, зависание при взрыве,
сейвы на SD после перелёта, Joy-Con/Pro/touch/OSK и GPU MatrixGame.
Ни один локальный object или unit PASS их не закрывает.

## Следующие обязательные работы

1. Выполнить ресурсный fault-injection из Run/OnOpen и suspend/resume; собрать
   матрицу остальных handlers и проверить фактические OSK-поля на последующей Switch.
2. Довести два UI-профиля до полной матрицы drag/tab/scroll и подсказок;
   расширить raw-pad route на manual save/load, торговлю, инвентарь и остальные окна.
3. Повторить save failure/new-process цепочки и реальные screen transitions по ТЗ,
   сохранить исходные SHA.
4. Аркада: смерть/взрыв, все overload cases, parity 500/1000, три парных perf series.
5. MatrixGame: реальные unit/blocker counts и прирост native compile/resource pipeline.
6. Финальная общая регрессия, один release Save ON NRO + matching ELF/SHA/provenance,
   отчёты perf/MG и 10–12 шагов аппаратной проверки. На этом этапе новых NRO нет.

## Пакет 6: реальный сейв 346-го хода и восстановление ручной записи

Исходная копия `build/e2e514/recovered/Recovered-turn-346.sav` имеет SHA-256
`665b82366662e9a98fa85289a127621355e470035c8915b84a0379fb0aa955fb`.
Архивы и пользовательские оригиналы не изменялись; baseline восьми файлов PASS.
Настоящий loader восстановил экран планеты (5), ход 346, деньги 2044, корабль 3,
звезду 41, планету 192 и 15 скриптов. Это архивное состояние после перелёта;
новый межзвёздный перелёт этим маршрутом не доказан.

Raw-pad маршрут выполнил три настоящих хода и записал `После перелёта Ёж 515.sav`:
825032 байта, SHA-256
`1118270c93935ba8dea673fc4c7563033582d738b1abecf9e3a97179418171b7`.
Сведения: `build/e2e515/real346-resave/result.json` и
`build/e2e515/real349-reload/result.json`. Новый процесс загрузил этот файл;
`checkpoint_diff={}`. Помимо хода 349/денег/корабля/системы сравниваются
generation seed, RNG мира/игрока и хеш всех RNG, сериализованные скрипты,
глобальные переменные, инвентарь (9 элементов) и корабль.
Observer использует исходные сериализаторы, проверяет неизменность RNG и включён
только явной host-переменной `E2E515_SAVE_CHECKPOINTS=1`; на Switch это no-op.
Load checkpoint расположен после восстановления ссылок и PrimeIntegrityChecksum.

Обнаружен новый воспроизводимый P0: остановка процесса на `manual-publish`, после
удаления старого имени, оставляла основной слот отсутствующим. `.bak` сохранялся,
но Recover обслуживал только quicksave. До исправления:
`build/e2e515/save-faults/result-before.json`, FAIL, `primary_exists=0`,
`backup_exists=1`, `recovery=no transactions`.

В `e2e4_quicksave_transaction.cpp` добавлен отдельный журнал ручной публикации:
проверенные manifest/ready до изменения имени, fingerprint старой/новой генерации,
восстановление из проверенного `.bak` при незавершённой записи, committed marker
после readback. Прерванные данные сохраняются; повторное Recover идемпотентно.
Готовый `save.tmp` повторно используется при rollback без дополнительной полной
копии. Версия и содержимое игрового формата не меняются.

Окончательный ASan/UBSan прогон:
`build/e2e515/save-faults/runs/77acfcc897c94ebc912c1b52113463c2`, PASS:
18 отказов API, 18 настоящих дочерних процессов с `_Exit(99)`, 36 повторных записей,
латинские/кириллические занятые слоты, исходные SHA неизменны. Также прошёл прежний
transaction suite, включая частичную публикацию, битый temp и его сохранение.
Дополнительно три `_Exit` для нового незанятого Unicode-слота: до публикации,
после копирования до flush, перед committed. В каждом Recover убрал неполную
публикацию, сохранил temp, повторный Recover и новая запись прошли.
Это process-stop на границах операций; физическое выключение, FAT/SD и реальное
исчерпание носителя не проверялись. CI-команда исправлена: добавлен уже необходимый
`user_root.cpp` для линковки transaction test.

devkitA64 с настоящими Save ON flags скомпилировал семь модулей, включая aGalaxy
и transaction.cpp; `build/e2e515/sdk-input/result.json`: COMPILE PASS,
`linked=false`, `hardware_executed=false`. Финальная NRO ещё не выпускалась.

На окончательном host ELF SHA-256
`4c4083e0fa6024835f5fa138f866fd4430ac4f2f8102b58a01a3e441570e2e75`
повторён полный маршрут с `--fail-save-once`. В отдельном user root harness
создаёт собственный пустой каталог `save.tmp`, получает настоящий отказ worker,
проверяет отсутствие committed и неизменность всех слотов. Затем удаляет только
свой пустой каталог, контроллером закрывает GI_MessageBox, возвращается в тот же
SaveManager и повторяет запись. Русское имя сохраняется без повторного OSK.
`build/e2e515/real346-retry/result.json`: PASS, `save_failure_ui_retry=true`.
Получен `Повтор Ёж 515.sav`, 825044 байта, SHA-256
`7f818f3d31a0e6295c51f08e5897c56b00ef161a9118b19ac7442387ea2b63ab`.
`build/e2e515/real349-retry-reload/result.json`: отдельный процесс PASS,
`checkpoint_diff={}`, `new_process_reload_verified=true`. Оба процесса остановлены
runner после checkpoint; нормальный shutdown этим не доказан.
Четыре real-engine tests после rebuild PASS: runtime-loop, scene-canvas,
screen-context, native-focus (`build/e2e515/engine-tests/result.json`).

Воспроизведение на host (законные ресурсы и копия CFG отдельно):

```sh
python3 tools/e2e515_save_faults.py --old build/e2e514/recovered/Recovered-turn-346.sav --new '<полный путь нового сейва внутри build>'
python3 tools/e2e515_controller_route.py --load-save build/e2e514/recovered/Recovered-turn-346.sav --advance-turns 3 --resave-name 'Повтор Ёж 515' --fail-save-once --label real346-retry --game-root '/path/to/game' --config-root '/path/to/copied/config'
python3 tools/e2e515_controller_route.py --load-save '<manual_save_path из result.json>' --expect-checkpoint build/e2e515/real346-retry/result.json --label real349-retry-reload --game-root '/path/to/game' --config-root '/path/to/copied/config'
```

## Пакет 7: настоящая арена, смерть, взрыв и slow callbacks

Добавлены только diagnostic observer/header и host builder/runner; нового
исправления игровой механики или оптимизации в этом пакете нет.
`tests/e2e515_arcade_observer.hpp` вызывается из трёх disposable translation units,
`tools/e2e515_arcade_probe.py` линкует их с остальной полной host-игрой.
Смертельный удар на тике 50 проходит настоящий ApplyDamage; DefeatPanel появляется
из оригинального countdown, ExplosionComplete вызывается исходной анимацией.

Без задержки и с +20/+40/+70/+150/+250 мс пройдены все шесть host liveness случаев.
Максимальный интервал завершения software кадра среди них 402.59 мс, исходные CFG
не изменены. Измеряется успешный host PresentRgb565, не Switch SDL/GPU/scanout.
Первая неправильная попытка поставить probe в Switch-only SDL_RenderPresent
сохранена FAIL и исключена из таблиц. Корректный отдельный ELF:
`9b0089751fd7568561c4d1204b4eb74db8532a69260bc6ba7dcce1826f23fce6`.

Полные числа, raw CSV/JSON пути, команды и границы:
`reports/e2e5/e2e515-perf-comparison.md`, `reports/e2e5/e2e515-arcade-overload.json`.
Отсутствие длинного промежутка между host кадрами не доказывает изменения пикселей
самого спрайта или input-to-action. При +250 мс поражение появляется через 52.7 с,
окончание взрыва через 88.1 с; slow motion остаётся. Хеш выбранного состояния
различается между overload сценариями, поэтому world/RNG parity ещё NOT PROVEN.
Оптимизаций с таким расхождением не добавлялось. Нужны deterministic replay,
500/1000 ticks, geometry/image checkpoints, три paired series, pause/retry/main,
cleanup, watchdog и аппаратное подтверждение. Shipping ELF/NRO не менялись.

## Пакет 8: постоянные тормоза обычного боя

Пользователь уточнил: взрыв был крайним проявлением, основная проблема — тяжёлый
обычный бой. Последующая работа сосредоточена на полном CPU тика/кадра, а не только
на death transition. Повторно разобран аппаратный pack: 1547 TimerTakt,
взвешенное среднее окон 36.367 мс; Advance 24.405 мс, worldline 7.429 мс.
Это данные прежнего устройства из архива, не новый hardware тест.

Выполнен обычный host-бой на 1000 тиках без lethal hit/slow callback; добавлены
CPU samples горячих функций (1/64, полный call count), геометрические checkpoints
на 500/1000 и RGB565 hashes завершённых software кадров. Рассмотренный новый
sin/cos reuse оказался уже существующим e2e56 effective override и не принят
как новое улучшение.

Изолированный кандидат polygon-memo сохраняет только результаты исходной
проекции при точном совпадении matrix/point/FP-control. Математический контракт
на настоящем ProjectPointByMatrix: ASan/UBSan 98304 checks PASS, ARM64/QEMU 98304
checks PASS, четыре режима округления. Отдельный polygon unit скомпилирован
devkitA64: COMPILE PASS, LINK/HARDWARE не проверены.

Первый live before/after gate **INVALID / NOT ACCEPTED**: main-thread CPU 7.795 → 10.296 с
(+32.08%), DrawFrame 3123 → 5203; world/RNG и geometry на 500/1000 расходятся,
из 870 общих ticks полные frame hashes совпали в нуле. Повтор неизменённого
baseline также разошёлся по миру/кадрам и дал 9.731 с (+24.83%): причинный
вывод о регрессии кандидата из этих чисел недопустим. По требованию ТЗ
эксперимент не включён в shipping overrides. Header хранится в tests, а вариант
доступен только явным diagnostic флагом. Дополнительные paired series не
запускались после первого FAIL; ускорение обычной аркады не объявляется.

Факты и гипотезы разделены: ограничитель EndFramePresentation вызывается после
полной отрисовки; рост лишних DrawFrame подтверждён CSV. В SE_Space::TObjectSE::Advance
найден wall-clock RandomSound с общим System::RandSeed, но его вклад в конкретный
parity FAIL ещё не установлен. Следующий gate — repeatability и контролируемый
timer/input/render replay с поиском самого раннего расхождения, без обнуления RNG
или удаления эффектов/событий. Числа и команды в perf-comparison.md и
e2e515-arcade-normal-profile.json. Эта работа не закрывает D/F3, новые тесты
компонента выполнены локально; CI и итоговая NRO этого пакета не запускались.

## Пакет 9: сопоставимость замеров обычного боя

Неизменённый live baseline при повторе дал другое состояние мира и кадров:
фиксированных seed/числа тиков недостаточно для сравнения в текущем wall-clock
цикле. Прежнее наблюдение +32.08% CPU не доказывает регресс polygon-memo.

Добавлен opt-in replay только для diagnostic ELF: исходная GI очередь получает
шаг 1 мс, исходный DrawFrame вызывается каждые 20 мс. После LoadPanel переносится
эпоха UI timers и UI-thread MMSystem; RNG не переустанавливается между тиками.
Таймеры/AI/эффекты не удаляются. CPU измеряется настоящими thread CPU часами.
Два одинаковых baseline совпали по selected state, геометрии, 1001 RNG snapshots
и 1000 полным software кадрам. Первый кандидат также совпал, ускорения пока нет.
Звуковых RNG событий в fixture не было: причина live расхождения ещё открыта.

Новый компаратор проверяет raw CSV/stderr, неизменность исходного CFG и SHA ELF.
Диагностическая линковка сначала завершается во временный файл; прежний ELF
заменяется только после успеха, копия сохраняется под SHA. Неудачная первая
попытка подключить clock к portable fallback вместо effective MMSystem дала
link FAIL и не запускалась; исправленный вариант собирает реальные десять units.

Это не full-world serialization, не проверка физического ввода, нормального
закрытия или GPU/FPS Switch. Основная проблема постоянных тормозов не закрыта;
дальнейшие CPU результаты и run IDs приведены в perf-comparison.md.

Завершены три пары replay с чередованием порядка: во всех PARITY PASS по
указанным следам. Кандидат даёт +1.93%, +2.54%, −2.11% main-thread CPU;
суммарно +0.72%, отношение медиан +1.93%. Устойчивое ускорение не доказано,
кандидат остаётся NOT ACCEPTED. Исходные восемь файлов неизменны, source pins
сохранены; Python AST и git diff --check PASS. Shipping NRO/ELF не менялись.

## Пакет 10: исходная проекция без вызова на каждую вершину

Проверен отдельный diagnostic вариант projection-inline. Тело настоящего
effective ProjectPointByMatrix извлекается автоматически, без замены формул,
точности, порядка сложений и исключений. Пять polygon callsites используют
встроенную копию; остальные units и oracle остаются исходными.

В трёх последовательных paired series все recorded state/RNG/geometry/images
совпали. CPU: +3.64%, −9.54%, −0.77%; сумма −2.43%, отношение медиан −1.71%.
Разброс baseline 9.58%; устойчивое ускорение не доказано, в shipping кандидат
не включён. Числа/команды/SHA и пределы сравнения в perf-comparison.md;
санитизированные шесть строк totals/wall/p50/p95/p99 в projection-inline.csv.

ASan/UBSan и ARM64/QEMU: по 32768 inline byte/FP-flag checks в четырёх rounding
modes плюс 98304 memo checks PASS. DevkitA64 effective polygon COMPILE PASS,
без полного SDK link/выполнения. Host CI получил asset-free math contract;
CI не запускался. Новых NRO и утверждений о Switch FPS нет. Обычные аппаратные
тормоза, live repeatability, полный паритет сериализованного мира, input lag
и teardown остаются OPEN.

## Пакет 11: подготовленный фон принадлежит сцене

Подтверждён и исправлен отдельный host-дефект: SceneCanvas терял подготовленный
исходным CaptureScreenBackground результат. Snapshot теперь принадлежит scope
сцены, переживает OnClose/Clear и не расходуется вложенной миниатюрой перед меню.
Дочерние captures не заменяют snapshot родителя; самостоятельная сцена не
наследует его. Это сохранение исходной подложки/эффектов, не дополнительная очистка
всего кадра и не утверждение о первопричине аппаратного фона генерации.

Baseline shaded-capture test FAIL; после правки 10 lifecycle cycles и пять
engine tests PASS. ASan/UBSan/leak detection покрывают четыре renderer/GI units
и тест; остальные game objects не инструментированы. Реальный raw-pad campaign
route и пять ship/map/menu/exit-confirmation Cancel/map cycles PASS, 34 PPM/hashes
проверены; по двум боковым областям меню после каждого Cancel 0 changed pixels.
В пяти ship/menu entries `prepared=1`. Host SHA:
`d310e45ad19fd32ad5ca47fb9dfbc6ff34d342b4023bf9560b25606228a9fbdd`.

Preflight ordinary/combined/Save ON и devkitA64 compile девяти units PASS;
imports/implementations не изменены, manifests обновлены по новым callsites.
Исходные восемь файлов данных неизменны. Новых NRO/CI не было. Маршрут остановлен
runner после завершения, full-game shutdown не проверен. Подробные границы,
ранние FAIL тестового драйвера и команды в e2e515-scene-ownership.md; санитизированные
метаданные в e2e515-scene-validation.json. Полный C1 и HARDWARE остаются открытыми.

## Пакет 12: планетные экраны и товарные строки

Добавлена адресная native-focus активация настоящих строк TfGoodsShop2 через
исходный GoodsMouseUp. Собственные callbacks и скрипты сохраняют общий путь.
TfCount2 начинает с Ok; требующий move/up ползунок остаётся курсорным fallback.
Покупка, продажа и изменение количества пока OPEN.

На копии настоящего сейва 346 пройдены 25 переходов между оборудованием, товарами,
новостями, ангаром и планетой. Дополнительно открыто исходное окно покупки и выполнен
Cancel: одно удержание A 900 мс даёт один Confirm, recorded checkpoint diff пуст.
Проверены 30 кадров, владельцы и 27 самостоятельных entries. В sampled областях
фрагментов генерации нет; reference из другого процесса, полного C1 PASS нет.

Host ELF SHA-256: `44de08424d41ad1293d2ad1c57b7a42306755ff298b3155817d4e4fd9aaa3f58`.
Пять engine tests (включая частичный sanitized-scene), три preflight и SDK compile
девяти units PASS. Full SDK link/NRO/CI/hardware отсутствуют; процесс остановлен
runner, full teardown OPEN. Восемь исходных файлов данных неизменны. Подробности,
ранние ошибки драйвера и границы в e2e515-planet-goods.md/json. B/F1 остаются PARTIAL;
обычные тормоза аркады, межзвёздный save route и настоящий MatrixGame ещё открыты.

## Пакет 13: изменение количества и настоящая торговля

TfCount2 D-pad влево/вправо вызывает исходный MainPanelKeyDown. Press/repeat
используют прежние границы и RefreshValue, не запускают mouse-down repeat timer.
Вверх/вниз сохраняют focus-навигацию, Confirm/Cancel — исходные callbacks.

На отдельной копии сейва 346 проверены Cancel, обе границы количества, покупка
и продажа одной единицы продуктов. Деньги 2044 → 2012 → 2043; трюм 0 → 1 → 0;
рынок 23 → 22 → 23; свободное место 54 → 53 → 54; себестоимость 0 → 32 → 0.
Другие семь товаров не изменились. Семь кадров проверены по RGB565 fingerprint,
владельцу и глубине; окна покупки/продажи и возврат просмотрены визуально.
Это проверка реальных операций через raw pad и оригинальные обработчики, без
подмены модели. Полный экономический паритет/все товары этим не доказаны.

Run `d8173c76af0442a5b556f84776166d25`, host ELF
`3d472cc1b1999cd603ca74ce9f09940d83b948287b378ef40591e14b7487948c`.
Unicode manual save «Торговля Ёж 515.sav»: 798430 bytes, SHA-256
`9a93bba2b9ea185087f50cba14baf8a797564cd2b6a2a20c11e88310a68d1beb`.
Исходный слот сохранён. Пять engine tests и devkitA64 compile девяти units PASS.
SDK probe сначала ошибочно запущен в WSL с Windows compiler path (FAIL до компиляции);
повтор в Windows завершился COMPILE PASS. Нового NRO/full SDK link/CI/hardware нет.
Sanitized-scene по-прежнему покрывает четыре renderer/GI units, не торговлю целиком.
Новый процесс загрузил сейв и восстановил магазин 34: checkpoint diff пуст,
registered-owner кадр подтверждён; всего проверено восемь кадров обоих процессов.
Метаданные и два ранних FAIL драйвера в e2e515-trade-roundtrip.md/json.
Межзвёздный маршрут, полный teardown, общие тормоза
аркады и настоящий MatrixGame остаются OPEN.

## Пакет 14: реальная причина отказа сейва после перелёта

Воспроизведён настоящий маршрут на копии сейва 346: взлёт → галактика → touch звезды
в пределах исходной дальности → ButJump → обычные ходы/экран гиперпространства →
ручное сохранение после прибытия. До исправления получен реальный FAIL:
Script.PC_fem_rangers/init/quest_item содержит указатель на игровой предмет, который
инвентаризация не умела записывать. Исходный скрипт использует ShipItems/CreateEquipment;
скрипт и игровые ресурсы не менялись.

Добавлена проверка live item identity по известным контейнерам и исходному IdToItem,
typed kind 15/item ID в RSG64/v168 и transactional RebindPending. Неизвестные ссылки
не обнуляются. Старый экспериментальный reader с максимумом kind 14 отвергнет новый
item-tag; текущий reader загружает прежний сейв 346. Windows RSG/v167 не менялся.

Итог: система 41 → 5, расстояние 7, топливо 20 → 13, прибытие на 360-м ходу.
quest_item получил сохранённый item ID 182356; 873 переменные, 0 unknown.
Unicode save 679360 bytes, SHA `bc6b1059a743800a4ae279356fd8d58968fd6948deaafc34dac02582b8a1f6b0`.
Новый процесс восстановил screen 16/ход 360/систему 5, checkpoint diff пуст.
Пять кадров обоих процессов проверены по fingerprint/owner и просмотрены визуально.

Host ELF `63e8d10c4036687afe697620ee69f76dca76a7e89b33699e22660e6ab0e5bcc2`.
Пять обычных engine tests, partial sanitized-scene и partial sanitized-item-references
PASS. Последний инструментирует actual aGalaxy/save refs и item test, не весь ресурсный
маршрут. Три preflight и десять SDK compile units PASS; imports unchanged. Восемь
исходных файлов неизменны. Ошибка первой диагностики, промежуточный SIGSEGV nullable
обхода и ранние test FAIL перечислены в e2e515-interstellar-save.md/json.
SD/hardware, полный world parity/teardown, аркадные тормоза, MatrixGame и final NRO OPEN.

## Пакет 15: подготовка матрицы проекции — без доказанного ускорения

Проверен диагностический projection-prepared: точная конверсия коэффициентов матрицы
в Extended один раз на пакет, исходная арифметика проекции без результата-кэша.
Exceptional/denormal matrix coefficients используют исходный путь. ASan/UBSan и
ARM64/QEMU: 32784 direct byte/FP-flag comparisons в четырёх rounding modes плюс
98304 прежних memo checks PASS. DevkitA64 actual polygon COMPILE PASS.

Три последовательные пары 1000 active ticks в порядке B/C, C/B, B/C прошли raw
state/RNG/geometry/software-frame parity. Все baseline также повторяемы между
собой. CPU: +4.882%, +0.404%, −1.496%; сумма +1.163%, отношение медиан +3.953%,
baseline spread 11.019%. Устойчивого ускорения нет: NOT ACCEPTED, shipping-код
не менялся. Числа/CPU p50/p95/p99/total/hotspots/wall и hashes в
e2e515-perf-comparison.md и e2e515-projection-prepared.csv/json. Новых NRO/CI нет.
Эти результаты не закрывают обычные тормоза или аппаратный FPS. Большая цель active.

## Пакет 16: полный CPU-модуль навигации MatrixGame

Фактический compile audit 80 игровых единиц: до 1 PASS/79 FAIL, после 2 PASS/78 FAIL.
Новая единица MatrixRoadNetwork.cpp целиком перенесена через проверяемый overlay;
оригинальные алгоритмы сохранены. Host static library и executable LINK PASS.
Десять циклов synthetic graph и ресурсная цепочка настоящей Boarding.cmap
(149 зон, 35 дорог, 13 регионов, 33 path queries) HOST PASS. Нормализованная
Save/Load/Save стабильна; raw equality с входной картой отсутствует, не заявляется.
Initial test failures, причины, ограничения и все 80 строк в
`reports/matrixgame-e2e515-status.md`, machine-readable compile metadata в соседнем
`matrixgame-e2e515-compile.json`. Игровые ресурсы в Git не добавлены.
Полный Init/Run/бой, GPU, кампания, новый navigation ASan/ARM64/SDK и final NRO OPEN.

## Пакет 17: navigation sanitizer, x86 reference, ARM64/QEMU и SDK

Для полного перенесённого MatrixRoadNetwork закрыты дополнительные gates:
host ASan/UBSan/leak detection, оригинальный Windows x86 reference, Linux ARM64/QEMU
и devkitA64 ELF compile/link PASS. Все три фактических CPU исполнения прошли fixture
и Boarding.cmap; полные serialized bytes совпали между платформами. Независимый
wire-format разбор атрибутировал все девять изменений raw→normalized оригинальному
правилу маски движения места с пушкой; других изменений нет.

1/1 navigation CTest с ASan/UBSan PASS, регистрация проверена с `--no-tests=error`.
Asset-free contract подключён к host CI, удалённый CI не запускался. Полные hashes,
Build ID, scope, initial failures, воспроизведение в matrixgame-e2e515-status.md и
matrixgame-e2e515-navigation.json. Данные в private dumps только в ignored build.
Исходные восемь пользовательских файлов и pinned источники неизменны.
Это закрывает navigation gates, оставленные OPEN в пакете 16; полный engine,
Init/Run/бой/GPU/кампания, общие тормоза аркады и final NRO остаются OPEN.
