# P0.3: числовая семантика широких script Int/Dword

Статус полного P0.3: **PARTIAL**. Исправляются числовые переходы реального
`TVarEC`; это не закрытие ownership/lifecycle всех игровых объектов и не
доказательство устранения аппаратного зависания после «Готово».

## Два воспроизведённых дефекта

Native storage `IntValue/DwordValue` уже расширен для живых адресов. Однако
это не должно расширять исходную signed-32 семантику `Int` и вход `SetInt`:

1. После настоящего `SF_ShipDestination(..., "Date")` адрес звезды сохранялся
   в vkInt полностью. `GetInt()` давал младшие signed-32 бита, но `GetFloat()`
   и `GetString()` читали всё широкое поле. Прямые `ConvertToKind` имели
   ту же проблему. `SetFloat/ConvertToKind(vkInt)` также могли сохранить
   64-битный результат Trunc вместо исходного int32, после чего обычное
   числовое сохранение отвергалось wide-pointer guard.
2. `SetInt(-1)` в уже существующем vkDword записывал `0xffffffffffffffff`
   вместо исходного DWORD `0xffffffff`. Аналогичный путь существует через
   vkRef: числовой `SetDword` делегирует в `SetInt`. Вход SetInt остаётся
   int32, поэтому здесь нет необходимости сохранять 64-битный адрес.

Первый дефект воспроизведён до изменения на реальном `TVarEC`/aScriptFun
под ASan/UBSan: `build/script-int-numeric-before.log`, assertion GetFloat
на высоком адресе. После первой части исправления расширенный signed-32
тест прошёл (`build/script-int-numeric-after.log`). Затем добавлен отдельный
DWORD/ref контроль, который воспроизвёл второй дефект:
`build/script-dword-before.log`, assertion GetDword после SetInt.

## Независимая проверка PC-контракта

Установленный оригинальный Rangers.exe только читался. SHA-256:
`83300344af802bc51e64389c58f047e5afdf195c133048098be3881fae29ed98`.
В int-ветке native GetFloat на `0x816ad3` используется signed DWORD FILD
из IntValue (+0x0c). Аналогичная загрузка в ConvertToKind float — `0x81606e`.
Native GetString на `0x816c43` читает DWORD из того же поля. SetInt в
ветке vkDword на `0x81727a` записывает DWORD из EAX в DwordValue (+0x10).

Это подтверждает размеры исходных числовых операций. Сами адреса исходного
32-битного EXE не используются в порте. Локальные opcode/field metadata:
`build/script-int-native-contract.json`; binary/disassembly в Git не включены.
Float→Int с Trunc проверяется по исходному int32-полю C++ reconstruction;
запуск такого преобразования внутри оригинального PC-процесса не заявляется.

## Исправление

`NumericIntFromStorage(int64_t)` явно получает младшие 32 бита и signed view
через bit_cast. Он применяется только к числовым Int getters/conversions:
GetInt, GetFloat, GetString, Int→Float/String и Trunc→Int. Последний переход
канонизирует signed-32 значение до проверки записи в сохранение. int64_t на
входе не допускает предварительного сужения результата Trunc на 32-битном host.

В `SetInt` запись в vkDword теперь сначала преобразует Value в uint32_t,
затем расширяет в native storage. Полный `SetDword(uintptr_t)` и его pointer
путь не сужаются. Сохранены vkInt tag Date getter, нативный адрес в storage,
GetDword, copy/reference/assignment и настоящий StarName consumer.

Размеры/поля TVarEC этим изменением не меняются. Новых disk tags, расширения
RSG64/v168 и изменений 32-битных ID нет. Высокий pointer-as-int по-прежнему
контролируемо отвергается при сохранении; после очистки временного буфера
проверяется успешная запись обычного числа. Это не обещание неизменности
частично заполненного буфера при исключении.

Effective-source guard проверяет семь NumericIntFromStorage-переходов и
zero-extension в конкретном SetInt. Добавлена ARM64/QEMU проверка helper в CI;
существующий host-regression уже запускает настоящий ShipDestination probe.

## Финальные адресные и сборочные проверки

- `build/script-numeric-final-after.log`: настоящий ShipDestination/TVarEC
  PASS после обоих исправлений. Проверены адреса >4 ГиБ, vkInt tag,
  copy/reference/assignment, настоящий StarName dereference после числовых
  чтений, GetFloat/GetString, ConvertToKind и Assume в Float/String.
- 8 целочисленных Float-входов, включая границы и значения за signed-32,
  проверены через SetFloat в Int и Float→ConvertToKind(Int): 16 случаев,
  canonical IntValue, Int/Dword/Float/String view и числовой save/load.
- SetInt в Dword и числовой SetDword через Ref проверены для
  0/0x7fffffff/0x80000000/0xffffffff. Высокий адрес через ту же Ref не сужается.
- ASan/UBSan инструментация применяется к тесту, effective aScriptFun.cpp
  и EC_Expression.cpp; остальные игровые объекты берутся из полного optimized
  host build. Это не sanitizer-инструментация всего движка.
- Callback/arcade/markup/purchase regression PASS:
  `build/script-int-width-regression-final.log`.
- Helper GCC ASan/UBSan и ARM64/QEMU PASS; Python tool regressions 25/25 PASS:
  `build/script-int-tools-test-final.log`.
- Полный optimized host link: 331 объект,
  SHA-256 `5378e634b11d41a34400f63ecaf0533c558ca6b65f66aece8be3a00b5ec4ac6f`.
- Combined Horizon compile/link PASS: 4 объекта пересобраны, 376 повторно
  использованы; ELF64 AArch64 PIE, 379 link objects, undefined_symbols=0,
  first_failure=null, hardware_save_writes=false. Проверочная WIP-сборка
  основана на d1d0131 с незакоммиченными изменениями этого блока.
  NRO `SpaceRangersHDAWarApartE2E4Combined-d1d0131-dirty-0930342386f9.nro`,
  16 460 080 байт, SHA-256
  `0930342386f959611b618da7a77850b529e41d8d4f269e60de811169e53a564f`.
  Matching ELF SHA-256
  `1070f282c9af10029ce210183086681c9d0317a4696fb7e899e35388e4a57ec8`,
  Build ID `69c03e50fbad54774fff1e59053895a1c7256bfd`.
  Это не новый согласованный аппаратный кандидат и не Switch runtime proof.
- Final effective audit: 380 translation units, 1388 файлов, 219 классифицированных
  findings, fixed_site_regression_failures=[]. Raw inventory отдельно:
  1411 modules, 2523 findings, 2401 unresolved, new_unclassified=0.
  Ни один из этих счётчиков не доказывает полный ownership PASS.

Файлы доказательств: `build/script-int-pointer-audit-final.json`,
`build/script-int-static-audit.json`, `build/script-int-host-build-final.log`,
`build/script-int-arm-build-final.log` и matching `e2e4-combined-build-report.json`.
Последний — текущий WIP report; report кандидата 3772031 остаётся внутри frozen ZIP.
SHA-256 frozen ZIP повторно подтверждён:
`4dd3eb2f5d5eb38d896e98469508fcd86c635faa8a77af60b9605c0f95be6d4e`.

## Игровой host-прогон окончательного кода

На host SHA `5378e634b11d41a34400f63ecaf0533c558ca6b65f66aece8be3a00b5ec4ac6f`
с оригинальными ресурсами пройдены New Game → stages 1/2/3/4/5/6/8 →
300/300 warmup → host mouse Continue → диалог → ангар и 60 кадров →
quicksave → reload → 60 кадров ангара → ход 302.
`build/script-int-final-newgame.json`: 65.98 с; stage
`4f23fe45438746bdb17bd5a745dcceef`, e2e.log содержит warmup 300/300 done.
Quicksave 757533 байт, SHA-256
`ee27ebca85916f159bb6d73eb1a636c50d5c0eb05abcb68ae8be3a9ba0adfbcd`;
ход 301, деньги 5710, корабль 2, звезда 3, планета 0, 16 скриптов.
Inventory PASS: 928 inspected, unknown=0.
`build/script-int-final-reload.json`: 67.74 с; те же state fields и 16 скриптов,
loaded_hangar_60_frames=true, post_load_end_turn=302, post_load_turn_advanced=true.
Crash/timeout/serialization rejection не зарегистрированы. Процессы остановлены
probe после условий; штатный выход игры и strict world/RNG comparison здесь
не проверялись. Тайминги не являются доказательством ускорения.

## Границы оставшегося аудита

Не все числовые переходы vkDword, Float/String, арифметика, story scripts,
отмена callbacks и владение объектами проверены этим набором. В частности,
нельзя глобально обрезать все Dword conversions: часть из них переносит
настоящие native pointers, и нужен отдельный producer→consumer аудит.
Физическая Switch и причина замершего экрана остаются OPEN/UNCONFIRMED.
