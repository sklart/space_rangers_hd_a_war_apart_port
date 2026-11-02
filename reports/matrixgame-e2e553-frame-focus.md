# E2E-5.53 — focus pump на границе frame

`Impl::StartRecording` выполняет `SwitchWindow::WaitForDrawable` перед
`frame.Acquire()` и `state.Frame(true)`. Прежний `AcquirePrimary` повторно
вызывал WaitForDrawable после открытия frame. Второй pump мог синхронно
войти в оригинальное освобождение video resources.

Прослеженная по исходникам цепочка при готовых video resources и существующей
minimap texture без system backup:

`AcquirePrimary → WaitForDrawable → Lifecycle::Update → FocusChanged(false)
→ CFormMatrixGame::SystemEvent → CMatrixMap::ReleasePoolDefaultResources
→ CMinimap::StoreTexture → unmanaged_texture::CopySurface
→ native_surfaces::Copy/Read/ReadFor → SurfaceCallbacks::read → FlushRecording`.

FlushRecording при открытой scene требует её закрытия и выбрасывает исключение.
При acquired frame без scene он может завершить recording и вызвать frame.Abort,
поскольку image_slot ещё отрицателен. Затем внешний AcquirePrimary продолжает
получение изображения, хотя его frame уже закрыт. Это нарушение порядка native
обёртки; весь оригинальный callback на настоящей карте в этом тесте не исполнялся.

Первоначальное предположение о вызове CaptureState из CDeviceState::StoreState
не подтвердилось. StoreState читает исходные группы регистров через отдельный
StoredStateBackend. Проверка с искусственным CaptureState callback не является
доказательством дефекта оригинальной игры и исключена из итоговых результатов.

## Исправление

В AcquirePrimary удалён повторный pump и добавлен `frame.RequireBatch()`.
StartRecording сохраняет настоящий WaitForDrawable до начала frame. События,
пришедшие после этой границы, остаются в очереди до следующего безопасного
Update/WaitForDrawable. Оригинальные SystemEvent, minimap backup, освобождение
и восстановление video resources не заменены и не отключены. HOS exit также
обрабатывается на следующей границе, без входа в teardown посреди acquire.

Проверки readiness, размеров, slot range и повторного acquire сохранены.
Начало frame, очистка, сцены, offscreen batches и presentation не подменены.
Это устранение повторного опроса в native backend; общие контракты Init/Draw
и original generated sources не изменены.

## Проверки и пределы

| Gate | Результат |
| --- | --- |
| HOST ASan/UBSan | PASS, 22 случая. Точные тела прежнего/нового AcquirePrimary и текущего FlushRecording; настоящие Lifecycle, NativeFrameScope, NativeStateStore и DrainNativeDrawBatch. |
| ARM64/QEMU | PASS, те же 22 случая. |
| Baseline | При focus readback воспроизведены закрытие acquired frame до возврата acquire и отказ RequirePresent для открытой scene. Это CPU boundary reproduction, не аппаратное воспроизведение полной minimap backup. |
| Candidate | Событие остаётся queued внутри frame; readback callback исполняется до открытия следующего frame. Проверены background/resume до допуска, отложенный exit, readiness/extent/slot errors, repeated acquire, scene, отказ вне frame/после submit. |
| COMPILE/LINK | Полный diagnostic devkitA64 ELF, strong undefined отсутствуют. Хеши и команды в SDK profile и итоговом audit. |
| WINDOWS HAL | NOT RERUN: изменён только Switch Impl::AcquirePrimary; общие providers и оригинальные методы сохранены. |
| SWITCH HARDWARE | NOT RUN / HARDWARE_PENDING. Ни focus/resume на карте, ни первая карта/Present этим этапом не доказаны. |

Тестовый Observer вызывает точный FlushRecording как readback boundary;
он не исполняет CFormMatrixGame/полный CMinimap. Window IPC, acquireImage и
submit/wait — явно обозначенные CPU endpoints, не эмуляция HOS/GPU или успешного
Init. Изменение не объявляется исправлением исторических зависаний Switch.

```powershell
python tools/matrixgame_frame_focus_checks.py host
python tools/matrixgame_frame_focus_checks.py arm64
python tools/matrixgame_actual_draw_checks.py sdk --output build/matrixgame/e2e553-frame-focus --diagnostic-source build/matrixgame/e2e543-journal/source
```

Команды, binaries, извлечённые тела и hashes:
`build/matrixgame/e2e553-frame-focus`.
Итоговый аудит: `matrixgame-e2e553-frame-focus-evidence.json`.

Последняя аппаратная граница остаётся run2: BC1 upload выполнен, затем штатный
пустой applet poll ошибочно признан фатальным; draw/Present/original Init не
достигнуты. Исправления E547–E553 ещё не выполнялись на Switch. Новой NRO,
commit/push нет. Основная NRO, оба diagnostic пакета, original generated inputs,
upstream/frozen, ресурсы, hardware inputs и сейвы проверяются по хешам.
