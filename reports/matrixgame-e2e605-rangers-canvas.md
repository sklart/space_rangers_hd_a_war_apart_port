# E2E-6.05 — галактический RGB565 canvas через настоящий DekoDevice

Добавлен реальный native presenter галактического CPU canvas как зависимость
общего native/deko перехода в MatrixGame. Полная цепочка native Gov/FRun/return
этим этапом не объявляется выполненной; GPU runtime на Switch NOT RUN.

`renderer_platform::PresentRgb565` теперь допускает scoped presenter. При его
наличии выполняется именно этот backend; его ошибка не заменяется успехом SDL
или host-only presentation counter. Без регистрации прежний путь сохранён.
Регистрация заимствуется синхронно, допускает одного владельца и привязана к
UI-потоку; чужой поток отвергается. Регистрацию нельзя создавать/уничтожать
параллельно с presentation; остальная исходная software renderer также имеет
единственный UI-thread lifetime.

`NativeRangersCanvas`:

- создаёт настоящий managed texture R5G6B5 через TextureSession DekoDevice;
- копирует только полезные строки RGB565, сохраняя слова точно, без чтения
  source padding и без записи destination padding;
- рассчитывает aspect-fit/letterbox, очищает фон и выполняет native textured
  quad, scene end и фактический PresentNativeFrame;
- задаёт собственные raster/sampler/texture-stage/viewport состояния, чтобы
  настройки прежнего игрового draw не меняли вывод canvas;
- сохраняет и восстанавливает настоящий device state lease; при исключении
  вызывает AbortNativeFrame/Restore, ошибки cleanup записывает в журнал;
- освобождает регистрацию до texture owner; заимствованный DekoDevice должен
  пережить canvas и каждый синхронный Present.

На Switch `NativeRangersInterface(NativeGameOptions)` теперь владеет canvas
и требует настоящий DekoDevice. Владелец canvas переживает Run и callback owners.
Legacy reference constructor не меняет вывод Windows. Эта ветвь скомпилирована
и сохранена в retained SDK ELF, но её constructor/Present не исполнялись на GPU.
Текущая standalone NRO E604 вообще не создаёт NativeRangersInterface.

## Проверки и доказательства

[Машинный аудит](matrixgame-e2e605-rangers-canvas-evidence.json) PASS:

- `cpu/1`: host ASan/UBSan/LSan, ARM64/QEMU UBSan — exit0. Проверены все 65536
  RGB565 words, padding, отсутствие изменений source, полные/усечённые строки,
  изменённый pitch после построения плана, отказ до записи, single-pixel и
  aspect fit 1024×768 → 1280×720 (960×720 с x160).
- `windows/1`: изменённый renderer dispatch скомпилирован MSVC; CPU pixel test
  ASan exit0. GPU/физический Gov display не проверялись. Предупреждения о
  debug information/PDB сохранены в логах; тест не выдаётся за проверку GPU.
- `sdk/4`: реальный deko presenter, native interface owner и original renderer
  dispatch скомпилированы devkitA64, retained campaign/interface/presenter ELF
  успешно слинкован, AArch64, strong undefined symbols отсутствуют.
- В новом отдельном original archive изменён только renderer_platform.o из
  378 members относительно E603. В отдельной копии native interface archive
  изменён только native_rangers_interface.cpp.obj, support member сохранён.
  E604 link inputs, все NRO и TEST ZIP сохранили SHA. Protected game/frozen
  inputs проверены; upstream и игровые ресурсы не редактировались.

SDK attempt1 сохранил успешные compiles, но не дошёл до линковки из-за проверки
relative basename `libmatrixgame-native-cache.a` в runner. Проверка исправлена;
attempt2 выполнил link presenter, attempt3 — owner integration, финальный
attempt4 включает dependency manifest и все текущие hashes. Результаты не
перезаписывались и старые промежуточные gates не выдаются за финальный этап.

## Воспроизведение

```text
wsl --exec python3 /mnt/d/repos/space_rangers_hd_a_war_apart_port/tools/matrixgame_rangers_canvas_cpu.py
python tools/matrixgame_rangers_canvas_checks.py
```

В native-engine CMake добавлены source native_rangers_canvas.cpp и CPU target
`matrixgame-rangers-canvas-pixels`. Изолированный SDK runner собирает только
новые/изменённые units и повторно использует проверенные SDK link inputs.
Он не запускает GPU и не создаёт NRO. `matrixgame_current_diagnostic_sdk.py`
для будущих сборок выбирает последний успешный E605 dependency manifest с
полным перечнем объектов; прежние NRO E604 и их архив не обновлялись.
Metadata E604 описывает исторические as-built sources: после текущих изменений
файлы исходников отличаются, сами проверенные binaries/ZIP остаются прежними.

## Оставшаяся native цепочка

1. Адаптировать реальные caller reset/gamma/window/cursor операции Robot::FRun,
   которые ещё используют GR_DXReset и Windows/D3D9 globals. Нельзя передавать
   поддельные COM identities или пропускать обязательное восстановление.
2. Собрать настоящий native caller context: software canvas/UI/campaign/Gov,
   общий Window/DekoDevice, callbacks, private output/user roots и цикл возврата.
   При letterbox нужно согласовать UI pointer coordinates с destination rect.
3. Пройти integrated native Init/Draw/result/snapshot/Gov return и построить
   новую отдельную диагностическую NRO именно с этой точкой входа.
4. Естественная победа остаётся NOT PROVEN. Natural defeat и настоящий возврат
   с exception overlays доказаны на Windows в E604. Hardware/image/audio/HID
   acceptance выполняет пользователь вручную.

Заглушки GPU-операций не добавлялись. Сохранения/ресурсы/frozen/upstream и
основная/прежние NRO не изменялись; commit/push и доступа к Switch не было.
