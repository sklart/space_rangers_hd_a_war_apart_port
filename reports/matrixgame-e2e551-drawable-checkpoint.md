# E2E-5.51 — управляемое ожидание foreground

После E550 дополнительно проверены `.create()` sites полной native цели:
device/queue/command buffers, presentation color/depth/swapchain, shader code,
uniform packets и texture store получают Unique-владельца. Временный handle
Impl::Allocate передаётся в возвращаемый UniqueMemBlock до выхода; повторный
AllocationSize для уже проверенного значения не добавляет failure path.
Дополнительной staging-подобной утечки в этих sites анализ не выявил.
Это source audit, а не доказательство отсутствия любых GPU leaks.
Старый отдельный DekoMeshRenderer probe не входит в текущую original-game цель
и этим этапом не изменяется.

## Исправленная граница

Lifecycle::WaitForDrawable обрабатывал applet messages и ждал foreground
порциями по 50 ms через native Backend::Wait, но не возвращал управление
диагностическому RunControl. Так что deadline/B/+ проверялись до входа в
функцию и могли игнорироваться внутри её собственного управляемого цикла.
В отличие от непрерываемого SDK вызова, этот цикл принадлежит порту.

Добавлен optional WaitCheckpoint, принадлежащий owning-thread Lifecycle.
В diagnostic main он вызывает настоящий Cancellation::Poll и
Journal::RequireHealthy. Cancellation и journal живут дольше Window/Device/
runtime; callback сохраняется при переходе early attachment → original Init
→ error attachment. Callback проверяется до первого ожидания и после каждого
возврата Wait/Update. HOS ExitRequested доставляется Update раньше checkpoint.
В обычном окне без callback исходное поведение сохранено.

Original CGame::Init/Draw не изменены. Это не прерывает blocking GPU/IPC
вызов, зависший Backend::Wait или процесс, приостановленный самой HOS;
после возврата/возобновления policy снова проверяется. Это также не делает
синхронную загрузку файла или декодирование произвольно прерываемыми.

## Проверки

| Gate | Результат |
| --- | --- |
| HOST ASan/UBSan | 7 пар baseline/candidate scenarios. Точное прежнее тело WaitForDrawable сохранено в build: baseline игнорирует deadline/cancel и возвращается после model resume, candidate бросает до следующего drawable work. |
| ARM64/QEMU | Те же actual Lifecycle/RunControl с deterministic clock/IPC; PASS. |
| Addressed lifecycle regression HOST/ARM64 | Original CForm, focus/resume/background, KEEPALIVE, exit, visibility restore failures, bounded pump; PASS. |
| Handoff HOST/ARM64 | Production InitAsNative/Deinit с IPC-моделью и exclusive owner; PASS. Дополнительно проверены ровно 4 wait-checkpoint вызова через early/original/error owners; отказ двойного Attach сохранён. Это не полный CGame::Init. |
| COMPILE/LINK | Actual SwitchWindow setter, diagnostic entry, callbacks, original-game SDK ELF; PASS, strong undefined = 0. |
| WINDOWS HAL | Не повторялся: original Init/Draw/Windows provider контракты не менялись. |
| SWITCH HARDWARE | Callback/HID/cancel/deadline на Switch NOT RUN / HARDWARE_PENDING. Новых запусков нет. |

Семь cases: deadline в background, user cancel, истёкший deadline при уже
foreground, приоритет HOS exit, no-hook resume, detach/re-attach/clear callback,
исключение самого wait. CPU endpoints не имитируют успешный GPU draw.
Прежние lifecycle/handoff evidence не перезаписаны; новые результаты изолированы.

```powershell
python tools/matrixgame_drawable_checkpoint_checks.py host
python tools/matrixgame_drawable_checkpoint_checks.py arm64
python tools/matrixgame_lifecycle_checks.py host --output build/matrixgame/e2e551-drawable-checkpoint/lifecycle
python tools/matrixgame_lifecycle_checks.py arm64 --output build/matrixgame/e2e551-drawable-checkpoint/lifecycle
python tools/matrixgame_handoff_checks.py host --output build/matrixgame/e2e551-drawable-checkpoint/handoff
python tools/matrixgame_handoff_checks.py arm64 --output build/matrixgame/e2e551-drawable-checkpoint/handoff
python tools/matrixgame_actual_draw_checks.py sdk --output build/matrixgame/e2e551-drawable-checkpoint --diagnostic-source build/matrixgame/e2e543-journal/source
```

Точные команды/hashes/SDK inputs — в profiles и
`matrixgame-e2e551-drawable-checkpoint-evidence.json`.
Основная NRO, оба переданных diagnostic пакета, upstream/frozen, ресурсы и
сейвы сохранены; третьей NRO, commit/push нет. Исторические Switch hangs и
manual-publish ENOENT этим controlled-wait исправлением не закрываются.
Настоящий Init/первая карта/кадр остаются hardware pending.
