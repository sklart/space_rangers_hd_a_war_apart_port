# MatrixGame E2E-5.40 — native command memory

**COMMAND MEMORY POLICY / SDK PASS; SWITCH RUNTIME PARTIAL.**
Native recorder получил настоящие дополнительные command backing pages,
удержание их до completion и проверяемый отказ записи при allocation/quota error.
Исходные Init, draw calls, vertices/indices, materials и shaders не менялись.

## Baseline и применимость

Сохранённый E2E-5.39 recorder использует один блок 2 MiB и создаёт CmdBuf без
cbAddMem. Он соответствует исходнику, скомпилированному в предыдущую полную
SDK ELF. В установленном SDK deko3d 0.5.0-1 отсутствие callback при исчерпании
command memory приводит к fatal. addMemory позволяет продолжить текущий список
на другой странице; clear возвращает текущий указатель на последнюю страницу.
[Исходный контракт SDK 0.5.0](https://github.com/devkitPro/deko3d/blob/v0.5.0/source/dk_cmdbuf.cpp).

Это source-confirmed риск, **не воспроизведённое аппаратное переполнение**.
Реальный command byte demand карты пока не измерен. Данные E2E-5.39 о 4038
draws не превращаются в доказательство переполнения 2 MiB: число и размер
команд различаются. SDK headers, package version и baseline recorder сохранены
в build/matrixgame/e2e540-command-memory/baseline с контрольными суммами.

## Реализация

NativeCommandMemory владеет spill pages; CommandDriver выделяет настоящие
dk::MemBlock с CpuUncached/GpuCached и передаёт их SDK через dkCmdBufAddMemory.
Callback noexcept; metadata capacity резервируется до входа в SDK. Предыдущие
страницы не копируются, не перезаписываются и не освобождаются при росте.

При давлении памяти Record запускает существующий batch submit/wait между
draw operations; NativeClear — между rectangles, в том числе без draw packets.
Scene, acquired image и порядок исходных команд сохраняются;
промежуточных Clear/Present нет. После completion CmdBuf сначала очищается и
переводится на постоянную базовую страницу, затем освобождаются spills. Это
важно: одного dkCmdBufClear недостаточно перед освобождением последней страницы.
Destructor уничтожает CmdBuf до освобождения его command backing.

Void callback SDK не позволяет вернуть HRESULT. Для поддержанных запросов
собственная allocation error перехватывается внутри callback; SDK получает
заранее выделенную реальную discard memory только для возврата текущего вызова.
Ошибка остаётся latched. Вне callback Check возвращает exception исходному
вызову; FinishForSubmission проверяет ошибку до и после finishList. Оба реальных
queue submission sites используют этот путь. Весь ошибочный список отклоняется,
discard memory на GPU не отправляется. Abort очищает его и возвращает базовую
страницу. Ошибки allocation и quota имеют отдельные диагностические сообщения.
По pinned SDK source обычный CmdBuf создаётся без auto-flush; запись/рост сами
не исполняют список в обход этих submission guards. Reference .h/.cpp сохранены
рядом с baseline, без изменения установленного SDK или upstream.

Постоянная base — 2 MiB; normal command quota — 16 MiB; отдельный discard
reserve — 16 MiB. Максимум command backing — 32 MiB, включая reserve. Эти
настоящие allocations отделены от texture arena и draw packet quota. Запрос
одной SDK операции сверх всей quota не поддерживается и терминален: callback
не возвращает SDK недостаточную память. Текущий private encoder использует
bounded state/draw calls; произвольная запись giant inline commands не заявлена.
Внутренние unrecoverable SDK/control-memory errors и GPU queue-loss recovery
этим механизмом не закрыты.

## Что действительно исполнялось

| Категория | Доказательство |
| --- | --- |
| HOST ASan/UBSan | Production NativeCommandMemory: рост на 4 страницы, сохранность bytes; null/throw allocation failures, quota latch, ошибка во время finish, запрет submit ошибочного списка; clear/rebind до free, teardown order. Совместный production DrainNativeDrawBatch/NativeFrameScope: pressure drain сохраняет сцену; clear-only acquired phase без packets допустим; failure удерживает pages/owners/counters. 6 failure scenarios PASS. |
| ARM64/QEMU | Те же production templates и assertions PASS. Writer, allocator и completion в тесте — явная CPU model, не deko encoder и не GPU. |
| COMPILE/LINK | Новая полная devkitA64 native-entry ELF: 183 own reachable link inputs, 0 strong undefined. Настоящие SDK callback, MemBlock, addMemory и submit paths слинкованы. 255 generated originals совпадают с E2E-5.39. |
| WINDOWS HAL | NOT RERUN. Изменения относятся к private Switch recorder; Windows provider/Init/Draw contracts не менялись. Последний полный original запуск — E2E-5.39. Большие font/pixel/state наборы не повторялись. |
| SWITCH HARDWARE | NOT RUN / HARDWARE_PENDING. Реальный command growth, allocation OOM, VRAM/FPS, Init/карта/Present на консоли не исполнялись. |

Host test models documented request/write/clear semantics и проверяет byte
contents/liveness; он не исполняет подменённый deko encoder и не доказывает
конкретный GPU command stream. Реальный encoder/callback ABI проверены SDK
compile/link и сравнением с установленными headers, не аппаратным запуском.

[Машиночитаемое evidence](matrixgame-e2e540-command-memory-evidence.json).
[Последовательность Init и cleanup](../docs/matrixgame-init-sequence.md).

## Оставшийся gate первой карты

Первый runtime blocker Switch не наблюдался: native запусков нет. Необходимы
реальные platform/device creation → original CGame::Init → RobotPreload →
stor.Load → PrepareMap → GPU allocation/upload → Draw → Present, затем input/бой.
В частности, влияние постоянного reserve на доступную память требует устройства.
Линковка и CPU failure model не являются PASS загрузки карты или кадра.

Protected inputs, основная/root NRO, frozen-комплекты, hardware inputs, robots.pkg,
CFG и frozen GI source сверены; ресурсы/сейвы не редактировались, upstream pin
чистый. Новых NRO, commit/push нет. P0, GI repeating-text causality,
manual-publish ENOENT, старые Switch hangs, raw HAL retry FAIL и queue loss OPEN.
