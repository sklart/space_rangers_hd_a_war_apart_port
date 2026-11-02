# E2E-5.50 — staging ownership и transfer queue admission

В реальном DekoTextureStore обнаружена утечка временных staging blocks:
`auto staging = dk::MemBlockMaker{...}.create()` получает невладеющий
`dk::MemBlock`. Все три transfer-пути (Upload/ReadImage/WriteImage) теряли
владельца после выхода, включая обычное завершение mip upload.

## Изменения production

- Staging в трёх путях теперь `dk::UniqueMemBlock`, с освобождением после
  завершённого transfer или до submit при отказе подготовки.
- Upload отвергает уже отказавшую очередь до image/staging allocations;
  AllocateTarget также проверяет её перед layout/residency allocation.
- Общий SubmitAndWait проверяет очередь после finishList и до dispatch,
  а также после submit/flush/wait. Ошибка не превращается в успешную загрузку.
- При ошибке после submit staging переходит в безаллокирующий quarantine
  `failed_staging_`. Он сохраняется до закрытия borrowed queue её владельцем.
- Impl teardown закрывает отказавшую очередь до освобождения command/data
  owners; в нормальном teardown queue закрывается перед texture_store.
  Оригинальные Init/Draw/material/mip алгоритмы не заменены.

Этот порядок необходим, поскольку [исходный deko3d v0.5.0 Queue::waitIdle](https://github.com/devkitPro/deko3d/blob/v0.5.0/source/dk_queue.cpp)
может просто вернуться для error-state queue, а отправка/flush в известную
отказавшую очередь является SDK ошибкой. Возврат wait при error не считается
доказательством GPU completion. Quarantine не является queue-loss recovery:
восстановление устройства/сессии и изображения после такого отказа остаётся OPEN.

## Адресные проверки

Исполняются настоящие DekoTextureStore.cpp, native_texture.cpp, SDK C++ wrappers,
ValidateUpload, residency и Resident publication. SDK C endpoints в тесте —
явная CPU-протокольная модель allocation/copy/queue; opaque image/layout fixtures
не выполняют GPU copy, не создают настоящий framebuffer и не доказывают пиксели.

В baseline используется сохранённый исходный .cpp до исправления; обе сборки
используют тот же установленный SDK и совместимый текущий header. Нулевой новый
quarantine field сам по себе не устраняет прежние ошибки baseline.

| Gate | Фактическое доказательство |
| --- | --- |
| HOST baseline | 15 сценариев, 8 попыток dispatch в известную lost queue; 15 staging blocks остались после разрушения store. В отдельном запуске LeakSanitizer подтвердил 61 800 байт / 30 CPU allocations с exit=23, ожидаемым только для baseline reproduction. Это объём CPU endpoint fixture, не измеренный Switch VRAM. |
| HOST candidate ASan/UBSan | 15 сценариев PASS: 0 known-lost dispatch/flush/wait, 0 оставшихся blocks после channel close/store teardown. Прежний Resident image и dirty state сохранены при failed upload, residency rollback работает. |
| ARM64/QEMU | Те же baseline/candidate protocol/ownership показатели и quarantine-порядок. Без реального HOS/GPU. |
| Post-submit faults | Отдельно submit/flush/wait ошибки: pending staging запрещено освобождать до model channel close. Actual Impl close-before-owner order проверяется исходниками и SDK link, а не выдаётся за hardware execution. |
| COMPILE/LINK | Диагностический devkitA64 ELF с настоящим production transfer backend и deko3dd; strong symbol/исходные TUs проверяются итоговым аудитом. |
| WINDOWS HAL | Не повторён: общий original Init/Draw/Windows D3D9 контракт не меняется, исправлен только native deko backend. |
| SWITCH HARDWARE | Изменения NOT RUN / HARDWARE_PENDING. Нет доказательства карты, кадра, реального VRAM расхода или queue-loss recovery. |

Перечень fault cases: уже lost queue, late loss при staging allocation и
finishList для upload/read/write, отказ allocation, late failure submit/flush/wait,
успешная mip-публикация и сохранение предыдущего image при failed Resident.
Тест не обходит health-проверки production. Ручной endpoint reset разделяет
независимые CPU fault cases и не изображает восстановление настоящей queue.

В обычном baseline запуске тест сначала фиксирует оставшиеся blocks, затем
явно освобождает только fixture bookkeeping resources. Отдельный baseline-leak
запуск убирает bookkeeping roots и намеренно оставляет allocations для LSan;
его ненулевой exit не считается успешным candidate test.

## Воспроизведение и границы

```powershell
python tools/matrixgame_transfer_admission_checks.py host
python tools/matrixgame_transfer_admission_checks.py arm64
python tools/matrixgame_actual_draw_checks.py sdk --output build/matrixgame/e2e550-transfer-admission --diagnostic-source build/matrixgame/e2e543-journal/source
```

Логи/hashes: `build/matrixgame/e2e550-transfer-admission`.
Аудит: `matrixgame-e2e550-transfer-admission-evidence.json`.
Сохранены оба переданных diagnostic NRO, основная NRO, upstream, frozen,
ресурсы и сейвы. Третьей NRO и аппаратных запусков нет, commit/push нет.

Последняя фактическая аппаратная граница остаётся run2 appletGetMessage=59905
до первого draw/Init. E547–E550 исправления существуют пока в исходниках/ELF
и адресных CPU доказательствах. Не следует считать их подтверждением успешного
smoke или первой загрузки карты на Switch.
