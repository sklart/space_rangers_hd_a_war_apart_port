# E609: native UI-получатель и очередь событий оригинального GI

Реализованы `native_ui_target.hpp` и `RangersInputQueue`. EngineSink передаёт
клавиши, мышь и согласование кнопок явно установленному UI-получателю; без
активного получателя сохраняется прежний MatrixGame-путь. Регистрация scoped,
на одном потоке, с отказом при повторном владельце. Другой поток её не видит.

Очередь переводит физические координаты в логическую область через тот же
`rangers_canvas::Layout`, что и настоящий RGB565 presenter. Для canvas
1024×768 на output 1280×720 это изображение 960×720 при x=160. Центр физического
экрана соответствует (512,384). Чёрные поля дают (-1,-1), нажатие в них не
создаёт click-down. Отпускание после начатого внутри изображения drag остаётся
в очереди даже за пределами изображения. Double-click, состояния Shift/Ctrl,
wheel и middle button имеют исходный формат WM. Wheel delta передаётся как
заданный signed-word delta; перевод единиц конкретного HID-источника здесь
не утверждается. Текстовый ввод WM_CHAR этой очередью не синтезируется.

Движения объединяются только между соседними mouse-move; переходы кнопок и
клавиш сохраняются. Предел 2048 событий вызывает явную ошибку вместо потери.
При смене владельца старые события/held state очищаются. Перед этим владелец
обязан отменить состояние оригинальных UI controls; этот внешний lifecycle
контракт ещё предстоит подключить к native caller.

## Доказательства

| Gate | Результат |
| --- | --- |
| Host ASan/UBSan/LSan | PASS: все 691200 пикселей изображения, поля, drag release, double/modifiers/wheel, merge источников, scoped/TLS owner и overflow |
| ARM64/QEMU UBSan | PASS: тот же CPU-контракт |
| Windows partial MSVC ASan | PASS: настоящий Host/UiContext/Script/GlobalUi; новый EngineSink и оригинальный MainMenu ProcessWindowMessage приняли переведённые mouse-move; MatrixGame key state сохранён; отключение UI target вернуло события обычному Input; cleanup PASS |
| devkitA64 | PASS: текущий EngineSink и очередь скомпилированы, retained UI/caller/campaign ELF linked, strong undefined отсутствуют; runtime NOT RUN |

Windows проверяет реальный `GI_MessageLoop::LastMousePosition` после вызова
оригинального обработчика. Нажатие игровых кнопок или запуск перехода этим
probe не выполняются. Drag release/double/wheel проверены на уровне очереди,
а не как аппаратное взаимодействие с оригинальным control.

Результаты находятся в `build/matrixgame/e2e609-rangers-input/{cpu,windows,sdk}/1/result.json`.
Сохранены as-built-sources и source-snapshot.json для всех трёх gates.
В SDK изменён только `native_input_engine.cpp.obj` в отдельной копии native-cache
архива. Оригинальный cache и прежние ELF/NRO не перезаписывались.

[Машинный аудит](matrixgame-e2e609-rangers-input-evidence.json) подтвердил
исходники/снимки/objects/binaries, actual Windows log, пустой приватный runtime,
защищённые игровые/frozen входы, чистые upstream pins, основную/root/E590 NRO
и E604 package с его 198 link inputs. Commit/push и доступа к Switch не было.

```text
# WSL, из корня проекта:
python3 tools/matrixgame_rangers_input_cpu.py
# Windows, из корня проекта:
python tools/matrixgame_rangers_input_windows.py
python tools/matrixgame_rangers_input_sdk.py
python tools/matrixgame_rangers_input_audit.py --output reports/matrixgame-e2e609-rangers-input-evidence-new.json
```

## Оставшаяся интеграция

Очередь пока не устанавливается постоянным native caller: нужен настоящий
владелец polling/dispatch, который связывает SwitchWindow.Update, актуальный
GI loop и состояния ввода. Нужно подключить GetAsyncKeyState/GetCursorPos/
cursor warp к тому же источнику, обеспечить отмену UI controls и пустую очередь
на границе FRun/window handoff, согласовать controller mapping и ввод текста.
Текущий Win32 compatibility pump имеет отдельные state/нормализатор; нельзя
выдать тест ручного dispatch за его завершённую замену.

Полная native UI/script/campaign/Gov entry, отдельная интегрированная NRO и
GPU/HID/window runtime остаются открытыми. E608 доказывает совместимость
production UI context с полным Windows-боем и возвратом после natural defeat,
но не runtime нового native UI input. Большой бой заново не запускался.
