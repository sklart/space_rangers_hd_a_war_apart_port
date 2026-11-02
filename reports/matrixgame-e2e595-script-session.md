# E2E-5.95 — original Script host ownership and cleanup

Статус: **PARTIAL для полного порта; Script lifecycle PASS**. Galaxy/Player не
создавались, бой и возврат результата в галактику в этом сценарии не запускались.
Машиночитаемые команды, исходники, объекты, библиотеки и SHA256:
[evidence](matrixgame-e2e595-script-session-evidence.json).

`RangersScriptSession` владеет настоящим `InitializeScriptHostRuntime` и его
ресурсами: VM, функциями и переменными, двумя рабочими потоками, событиями,
film history, path-node pool и контейнерами builtin runtime. Нужен живой
`RangersHostSession`; Galaxy и все потребители должны завершиться раньше
Script owner. Чужие глобальные владельцы отвергаются до инициализации.

Проверка обнаружила реальную незавершённую очистку. Вызов
`InitializeScriptBuiltinsAndConstants` создаёт `ScriptRequestThread`, семь
стеков контекста, четыре очереди и `StagedArcadeShips`. Оригинальный
`FinalizeScriptEngine` освобождает только VM и function scope. Поэтому при
повторной инициализации старый request worker и контейнеры терялись. Windows
завершал процесс, но host и QEMU зависали на выходе; ранние таймауты не PASS.
GDB перед `exit` подтвердил два оставшихся потока в `EC_Thread::ProcessRequests`.
Трассы сохранены в `build/matrixgame/e2e595-script-session/asan/debug`.

Owner теперь сначала вызывает настоящий деструктор request worker с ожиданием
остановки, затем исходный `ClearPendingScriptRequests`, освобождает 12
контейнеров, после чего вызывает исходный `FinalizeScriptHostRuntime`.
Очереди, VM и события остаются живыми до остановки worker. После исходной
финализации восстанавливаются прежние path-list heads/counts и script callback/
trace: оригинальная очистка pool оставляла освобождённые heads. Upstream не
менялся, обязательные операции не заменялись заглушками.

В двух циклах на каждой платформе исполняются:

- регистрация 912 настоящих встроенных функций;
- исходные компиляция, связывание и исполнение `GRunFrom = abs(-7) + 35;`,
  независимое чтение результата 42;
- настоящий pool из 200000 узлов, получение 24 узлов `TSPath`, добавление двух
  waypoint и возврат узлов настоящим деструктором;
- реальная семантика auto-reset и manual-reset событий;
- standby обоих рабочих потоков, исходная остановка и закрытие двух handles
  потоков и всех 14 событий, освобождение контейнеров;
- отказ без host, при повторном owner, чужом событии и чужом context stack;
- восстановление callback/counts и полностью освобождённый host после scope.

| Проверка | Результат |
| --- | --- |
| Windows partial ASan | PASS, exit 0, два цикла |
| Host ASan/UBSan | PASS, exit 0, два цикла, leak detection включён |
| ARM64/QEMU | PASS, exit 0, два цикла |
| devkitA64 standard `matrixgame-rangers-script` | compile PASS |
| Retained constructor/destructor API AArch64 ELF | link PASS, 0 strong undefined |
| Switch runtime | NOT RUN |

Новые owner/probe TUs инструментированы на sanitizer-платформах; большой
ранее собранный корпус исходных зависимостей повторно не собирался и не
инструментировался. Это не полная sanitizer-проверка оригинального движка.
Рабочие задания расчёта галактики и обработки gameplay requests не запускались
без Galaxy. Проверка request worker охватывает настоящий lifecycle и standby.

SDK API имеет четыре сильных символа — constructor/destructor ABI variants.
Первая SDK-попытка собрала библиотеку, но остановилась на неверном ожидании
пяти символов, скопированном из Government probe с дополнительным `Screen()`.
Критерий исправлен; первая попытка и metadata сохранены в `sdk/history/1`.

| Артефакт внутри `build/matrixgame/e2e595-script-session` | Байты | SHA256 |
| --- | ---: | --- |
| sdk/rangers/libmatrixgame-rangers-script.a | 95700 | 68cc71b324c63a97b555453359a5b367cfaec8c629acba4a5a66b6f404b088a5 |
| sdk/matrixgame-script-link.elf | 60295312 | 3ccdd986681ca2ff1711c4e7aaae088ede2ee47833416196245d505f764b4a87 |

Воспроизведение при наличии проверенных E574/E576/E592/E594 inputs:

```text
python tools/matrixgame_script_windows.py
python tools/matrixgame_script_cpu_checks.py asan
python tools/matrixgame_script_cpu_checks.py arm64
python tools/matrixgame_script_sdk.py
python tools/matrixgame_script_audit.py --output reports/matrixgame-e2e595-script-session-evidence-new.json
```

Достигнутая цепочка: настоящие data/cache host → script host/path pool/VM/
builtin tables/two workers; отдельно Government constructor/config/layout
проверены E594, а настоящий FRun до поражения — E593. Эти доказательства пока
не образуют выполненный end-to-end переход через Galaxy.

Следующая интеграция требует настоящих `TfHangar` и `TCCInterface`/Buffer
для `TGalaxy_Create`, исходной генерации новой кампании с настоящими
Player/Star/Planet, затем исходного `TfGov::OnOpen` с результатом боя,
наградами, историей и переходом экрана. Module-size/integrity операции в
оригинальном конструкторе сохраняются. Не использовались fake world records,
загрузка/изменение сохранений или пропуск обязательных операций. Естественная
победа и native/deko FRun ещё не подтверждены.

Аудит проверил pins и чистоту upstream, protected game/frozen hashes,
primary/root NRO, оба E590 пакета и E592 SDK artifacts. Новая NRO не
создавалась: остаются прежние E590 диагностические пакеты с hardware gate.
`D:/logs2.7z` отсутствует, archive gate **NOT VERIFIED**. Игровые ресурсы и
сохранения не изменялись, commit/push и взаимодействие с Switch не выполнялись.
