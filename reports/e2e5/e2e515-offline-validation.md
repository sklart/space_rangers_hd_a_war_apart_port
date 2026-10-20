# E2E-5.15 — журнал офлайн-валидации

20.10.2026. **IN PROGRESS / PARTIAL. Цель не завершена.**

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

| Требование ТЗ | Состояние |
|---|---|
| A1 полная handler matrix | PARTIAL: первая часть в controls doc, остальные экраны/drag/hover/OSK OPEN |
| A2 Action/Context | PARTIAL: ядро, реальные scoped loops, очистка очереди, owner cancellation есть; OSK, suspend, динамические arcade contexts и полный route OPEN |
| A3 controllers | PARTIAL: классификация/сброс есть; UI unsupported, dock/resume и hardware OPEN |
| B два настоящих профиля, focus, RTS, touch, accessibility | OPEN; существующий pointer/arcade fallback не считается реализацией двух профилей |
| C1 все реальные screen transitions, фон | OPEN; десять canvas unit cycles не заменяют переходы между настоящими экранами |
| C2 save fault injection / новый процесс / перелёт | OPEN в этом цикле; E2E-5.14 не объявляется новым тестом E2E-5.15 |
| C3 generation → hangar → map controller route | OPEN |
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

1. Закрыть реальный lifecycle при ошибке Run, OSK/suspend и динамические arcade
   pause/result contexts; собрать матрицу остальных handlers.
2. Реализовать два настоящих UI-профиля, focus/drag/tab/scroll, настройки и подсказки;
   пройти реальные маршруты с внедрённым контроллером, а не только host key driver.
3. Повторить save failure/new-process цепочки и реальные screen transitions по ТЗ,
   сохранить исходные SHA.
4. Аркада: смерть/взрыв, все overload cases, parity 500/1000, три парных perf series.
5. MatrixGame: реальные unit/blocker counts и прирост native compile/resource pipeline.
6. Финальная общая регрессия, один release Save ON NRO + matching ELF/SHA/provenance,
   отчёты perf/MG и 10–12 шагов аппаратной проверки. На этом этапе новых NRO нет.
