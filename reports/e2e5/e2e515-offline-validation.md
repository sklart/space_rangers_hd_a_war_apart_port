# E2E-5.15 — журнал офлайн-валидации

20.10.2026. **IN PROGRESS / PARTIAL. Цель не завершена.**

Последнее обновление: пакет 5, native controller campaign/save/load. Разделы
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
| C1 все реальные screen transitions, фон | OPEN; десять canvas unit cycles не заменяют переходы между настоящими экранами |
| C2 save fault injection / новый процесс / перелёт | PARTIAL: fresh campaign Unicode manual save после взлёта и raw-pad load новым процессом PASS; реальный 346-й ход/межзвёздный перелёт/checkpoint compare/fault injection OPEN |
| C3 generation → hangar → map controller route | Legacy real route PASS; raw pad menu/NewGame/settings и Unicode→generation→4 choices→hangar→takeoff→map PASS; остальные экраны OPEN |
| D смерть/взрыв, overload 20/40/70/150/250, parity 500/1000, три perf пары | OPEN; оптимизаций/FPS-победы этого цикла пока нет |
| E MatrixGame before/after units, новый real block, entry/resource pipeline | OPEN; текущий d3d9.h blocker ещё не закрыт |
| F1 CI input | Core sanitizer suite, реальные owner/canvas tests и message-input ASan/UBSan включены в host workflow; удалённый CI не запускался/не проверялся |
| F2/F3 runtime suites | PARTIAL/OPEN согласно строкам выше |
| F4 full release NRO + matching ELF | OPEN; финальный кандидат этого цикла не выпускался |
| Отчёты perf/MG, SHA/provenance и hardware checklist | OPEN; сформировать по настоящим завершённым проверкам |

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
