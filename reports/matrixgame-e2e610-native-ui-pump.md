# E610: production native UI-pump и общий источник ввода

Добавлен `RangersNativeInput`: после Host/UiContext он владеет scoped UI target
и источником событий для оригинального `GR_WinMessage`. UI/script/campaign
пользователи уничтожаются раньше этого владельца, сам UiContext — позже.
`MakeSwitchRangersInputBackend` связывает его с настоящими SwitchWindow
Update/WaitForDrawable/CancelHeldInput/WarpInputCursor и размерами NWindow.
SDL/фиктивный HWND или указатели D3D9 для этих операций не создаются.

Production generator добавляет native-ветку перед прежним Win32 pump. Она
вызывает реальный window poll, ожидает drawable после потери фокуса,
диспетчеризует очередь в original GI callback, обслуживает audio fades и
message idle/resume. Original GR_WinMessage остаётся прежним при отсутствии
scoped source. Новый generator helper включён в source-cache fingerprint.

Импорты GetAsyncKeyState/GetCursorPos/SetCursorPos используют тот же источник.
ClientToScreen/ScreenToClient для принадлежащего ему окна работают в логической
системе borderless UI canvas; физический aspect-fit учитывает сам источник.
Warp проходит в реальный Router/Store, а не только меняет возвращаемый Point.
Проверена неизбежная квантизация до одного logical pixel при уменьшении canvas.

На границе FRun caller lease отменяет старое UI-состояние и приостанавливает
источник до передачи окна. После настоящего возврата окна/device/gamma/cursor
источник возобновляется. Вложенные UI owners и resize очищают старые события
и повторно prime физический ввод. Нельзя активировать старый control отправкой
mouse-up: momentary buttons отменяются без click callback. При cleanup
приостановленный источник не вызывает reset уже не принадлежащего ему окна.

Для keyboard/mouse/pad/touch добавлен held-until-release gate при focus/attachment
и смене UI-владельца. Router warp синхронизирован с physical cursor, а повторное
прикрепление восстанавливает позицию Store перед первым poll. В UI D-pad выдаёт
стрелки, L/R PageUp/PageDown, X Enter, Y Space, Minus Tab, ZL Shift, ZR Ctrl;
A/B остаются pointer buttons, Plus Escape, правый стик — указатель. Во время
боя остаётся MatrixGame mapping.

## Найденная обязательная файловая зависимость

Запуск настоящего production GR_Main обнаружил обращение к Windows Documents
через SHGetSpecialFolderLocation. В UiContext теперь привязан оригинальный
CachedGameUserDirectory к выбранному отдельному `user_root/config` с настоящим
EnsureLayout, UTF-8 → UTF-16 и восстановлением старого значения через RAII.
GetGameUserDirectory сохраняет оригинальную реализацию и возвращает настоящий
частный каталог через свой cache. Desktop Shell не подменяется заглушкой.
CFG/сохранения не записываются; финальные runtime-каталоги не содержат файлов.

## Финальные проверки

| Gate | Путь result.json от корня | Результат |
| --- | --- | --- |
| CPU primitives | build/matrixgame/e2e610-native-ui-pump/cpu/1/result.json | Host ASan/UBSan/LSan + ARM64/QEMU UBSan PASS: 65536 held-mask сочетаний, high keyboard bits, все canvas warp points и geometry reset |
| Windows | build/matrixgame/e2e610-native-ui-pump/windows/6/result.json | Partial MSVC ASan PASS: original GR_WinMessage/GI/imports, owner reset/cleanup |
| Host owner | build/matrixgame/e2e610-native-ui-pump/host-owner/4/result.json | ASan/UBSan/LSan PASS: production GR_Main и original UI/owner/cleanup |
| ARM64 owner | build/matrixgame/e2e610-native-ui-pump/arm64-owner/4/result.json | ARM64/QEMU PASS: тот же production UI owner; не заявляется полная sanitizer-инструментализация |
| SDK | build/matrixgame/e2e610-native-ui-pump/sdk/6/result.json | devkitA64 current window/HID/warp/factory/imports/caller compile + retained link PASS, zero strong undefined, NOT RUN |

В runtime owner-проверках граница ввода **сценарная**: реальные production owner,
original GI/pump/imports исполняются без физического HID и SwitchWindow.
Windows дополнительно использует настоящий EngineSink; host/ARM probe подаёт
события непосредственно зарегистрированному target. Это не аппаратная эмуляция.
Проверены nested-owner queue cancellation, запрос physical reset, suspend/resume,
wait/exit gates и cleanup без обращения к утраченному окну. Поведение libnx
на устройстве подтверждается только будущим пользовательским запуском.

SDK retained ELF: 57 220 032 байта, SHA-256
`9bca3db6a00e5d1e8ea1d63b2496cf8199629f72f99fa06758a40643ee1b542e`.
Отдельный original archive сохраняет 378 members; заменены только GR_Main.o
и три Win32 compatibility input/window objects. В копии native-cache изменены
четыре input/window objects; отдельная interface library включает актуальные
caller и Switch factory. Добавлены native-input и обновлённая ui-context libraries.

Для финальных gates сохранены as-built-sources/source-snapshot.json без изменения
result.json. Host/ARM GR_Main взят из проверенного production SDK generation;
текст совпадает с финальным SDK после CRLF/LF normalization, byte hashes отдельные.

Предыдущие попытки сохранены: Windows1 — пропущенный include теста; SDK1 — лишняя
транзитивная OSK/SDK include dependency, устранённая выделением существующей
UI-cancellation функции в portable header; host/ARM1 — MSVC-only fixture input,
заменённый production source; host/ARM2 — реально недостающий user directory.
Последующие проверки уточняли восстановление pointer/held state и безопасную
cleanup при отсутствии окна. Большие campaign/battle tests не повторялись.

[Машинный аудит E610](matrixgame-e2e610-native-ui-pump-evidence.json) подтвердил
sources/snapshots/objects/binaries/archives, отсутствие runtime-файлов,
защищённые game/frozen входы, чистые upstream pins, основную/root/E590 NRO и
E604 ZIP/NRO со всеми 198 прежними link inputs. Commit/push не выполнялись.

## Воспроизведение и открытые компоненты

```text
# Windows SDK dependency first:
python tools/matrixgame_native_ui_pump_sdk.py
python tools/matrixgame_native_ui_pump_windows.py
# WSL:
python3 tools/matrixgame_native_ui_pump_cpu.py
python3 tools/matrixgame_native_ui_owner_cpu.py host
python3 tools/matrixgame_native_ui_owner_cpu.py arm64
```

CPU owner runner использует сохранённый production GR_Main SDK3 и проверяет его
hash; audit сравнивает нормализованный текст с финальным SDK6. Runners создают
новые attempt directories. Audit gate paths относятся к финальным попыткам выше.
Будущий standard diagnostic builder выбирает новый E610 original dependency;
его standalone entry сама по себе не становится campaign/Gov entry.

Следующий необходимый этап: собрать Host deferred → NativeRangersInterface →
UiContext → NativeInput → Script/GlobalUi → Campaign/Gov в настоящей native
точке входа на одном Window/DekoDevice, подготовить caller desktop/display
context и native battle loop, затем отдельную стандартную NRO. Сохраняется
граница Gov::OnOpen без autosave handler. Ввод текста WM_CHAR/OSK ещё требует
отдельной интеграции. Естественная победа не подтверждена. Full native Gov/FRun,
физический HID/GPU/window runtime и аппаратная приёмка остаются NOT RUN.
