# MatrixGame E2E-5.39 — original frame demand и native draw batching

**BATCH POLICY / SDK PASS; SWITCH RUNTIME PARTIAL.**
Снят исходный hard cap всего native кадра в 2048 draw packets. Оригинальные
вызовы не пропускаются: крупный кадр исполняется последовательными batches
в одной сцене, с одним acquired display image и одним итоговым Present.

## Применимость и baseline

Два настоящих Windows HAL сеанса снова исполнили original Init → RobotPreload →
stor.Load → PrepareMap, original form loop и сценарий выделения/движения/
стрельбы/focus recovery. Измерение ограничено 46 form Draw в каждом сеансе:
8 loop frames и 38 selection/control frames; прочие Init/offscreen/recovery
draws не объявляются измеренными кадрами.

Новый observer считает successful original draws внутри этих границ, сравнивая
текущий RenderTarget с настоящим GetBackBuffer. COM refs освобождаются сразу;
устройство, геометрия, callbacks и результат SDK не меняются. Expensive state
capture, 17940 font cases и большие pixel suites не повторялись.

Получены 92 кадра / 208808 primary-target draws. В 40 кадрах нужно больше
2048 primary draws; максимум — 4038. В каждом сеансе max=4038, over-limit=20.
Original HAL завершился успешно. Замороженный исходник native Record содержит
`if(packets.size()>=2048)throw`: этот guard не допускает измеренный поток.
Это **source-policy rejection по реальному HAL demand**, не исполненный
native GPU crash и не аппаратное воспроизведение на Switch.

Baseline EXE, runtime, CSV, исходный recorder и их SHA находятся в
build/matrixgame/e2e539-batches/baseline-windows. Диагностический exit=10
означает source-policy rejection; original_HAL=0 сохранён явно.

## Реализация

NativeDrawBatch ограничивает outstanding uploads каждого batch: 2048 packets
и 256 MiB. При достижении лимита настоящий recorder выполняет barrier →
queue submit → flush → waitIdle → проверку queue health; лишь затем освобождает
command list, packet buffers/descriptors/materials и retained image/target owners.
Следующий Record заново связывает targets и полный draw state.

Внутренний Drain не вызывает EndScene/BeginScene, Acquire, Clear или Present.
NativeFrameScope остаётся Scene (либо Acquired для cursor/compositor), а
NativeStateStore сохраняет признак занятого кадра. Вершины, индексы, текстуры
и порядок original draws не меняются. Отдельный draw сверх byte quota всё ещё
отвергается до GPU packet allocation/submission; предел не маскирует реальные OOM.

Если submit/completion неуспешен, owners и counters не освобождаются этим
путём. Это не доказательство восстановления потерянной GPU queue: такой gate
остаётся OPEN. Final Present, offscreen flush, Abort и Reset используют прежние
границы completion; общий cleanup освобождённых batches согласован с ними.

## Проверки

| Категория | Что доказано |
| --- | --- |
| WINDOWS HAL | Два полных original сеанса и real per-frame primary demand: 92 frames, max 4038. Original game runtime PASS; old native source-policy gate REJECT. |
| HOST ASan/UBSan | Production NativeDrawBatch/NativeFrameScope на captured counts: все 208808 admissions в исходном порядке; 40 split frames, 132 batches, максимум 2048 packets. Byte boundary/oversize, invalid phases, acquired compositor phase, owner release после completion, удержание owners при отказе submit/wait. Queue/owners — явная CPU model. |
| ARM64/QEMU | Тот же production policy и captured counts/assertions PASS. Не libnx/deko GPU. |
| COMPILE/LINK | Полная новая devkitA64 native-entry ELF, 183 own reachable link inputs, 0 strong undefined. 255 generated original files совпадают с E2E-5.38. Настоящие queue submit/wait и native recorder слинкованы. |
| SWITCH HARDWARE | NOT RUN / HARDWARE_PENDING. Ни baseline native guard, ни новый GPU batching, карта и Present на консоли не исполнялись. |

Count replay не содержит original geometry и её upload sizes; он адресно
проверяет packet-count barrier. Byte quota проверена отдельными boundaries,
а настоящий GPU allocation/command capacity/VRAM/FPS требуют Switch runtime.
Между batches используется waitIdle; аппаратная производительность не измерена.

После Switch-specific изменения повторять неизменные Windows shader/font
сравнения не требовалось: original D3D9 provider и draw semantics не менялись.
Новый Windows запуск нужен был для ранее отсутствовавшего per-frame demand.

[Машиночитаемое evidence](matrixgame-e2e539-batches-evidence.json).
[Полная последовательность Init/cleanup](../docs/matrixgame-init-sequence.md).

## Следующий обязательный gate

На Switch пока подтверждён только SDK compile/link. Первый аппаратный blocker
не наблюдался. Нужны реальные native platform/device creation → original Init →
RobotPreload → stor.Load → PrepareMap → allocation/upload → Draw → Present,
затем аппаратное управление и бой. Формальная линковка этот gate не закрывает.

Основная/root NRO, frozen-комплекты, robots.pkg/CFG и hardware inputs сверены
по protected fingerprints; ресурсы/сейвы не редактировались. Upstream pin чистый,
новых NRO, commit/push нет. P0, GI repeating-text causality, manual-publish ENOENT,
исторические Switch hangs, E2E-5.34 raw HAL retry FAIL и queue-loss recovery OPEN.
