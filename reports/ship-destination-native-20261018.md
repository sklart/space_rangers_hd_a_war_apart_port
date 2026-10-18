# A4: native ShipDestination Date contract, 18.10.2026

Последующее исправление signed-32 conversions и DWORD/ref writes:
[script-numeric-width-20261018.md](script-numeric-width-20261018.md).

Статус: **PARTIAL**. Подтверждён исходный PC-контракт, исправлена потеря адреса
в native producer/assignment, пройдены host ASan/UBSan и полный ARM64 compile/link.
Физическая Switch и полный runtime ownership всех сценариев остаются OPEN.

Проверен установленный оригинальный Rangers.exe, SHA-256
`83300344af802bc51e64389c58f047e5afdf195c133048098be3881fae29ed98`.
EXE только читался; бинарник и его disassembly не включены в Git/CI.
Строка ShipDestination (UTF-16 VA 0x6673d0) используется при регистрации функции
0x644bc4 через assignment на 0x661d99. Error.Script ShipDestination внутри этой
функции независимо подтверждает соответствие имени.

В ветке Date инструкция на 0x644cbc читает DWORD из [ruins+0x558], затем результат
передаётся в setter 0x817214. Setter запроса Date на 0x644ce0 пишет [ruins+0x55c].
Обычный запрос destination тоже читает +0x558, но использует setter 0x817390.
Offsets совпадают с FlyToStar и FlyDate в pinned reconstruction и её комментарием.
Небольшие opcode проверки и provenance: build/ship-destination-native-contract.json.

Следствие: замена Date getter на FlyDate исправляла бы странность оригинала и
меняла gameplay/script semantics. Она не принята как ARM64 fix. Бинарная проверка
сама по себе не доказывала безопасность сужения native pointer или допустимость
нового типа script value; поэтому исходный аудит оставлял REVIEW_NATIVE_SEMANTICS.
Ниже описаны последующая доработка и её границы. Это не A4 ownership PASS.


Дополнительно проверены native setters: 0x817214 при пустом result cell задаёт
kind=1 (vkInt), а 0x817390 — kind=2 (vkDword), затем записывает соответствующие
32-bit поля +0x0c / +0x10. Поэтому простая замена SetInt на SetDword также меняет
тип результата Date. Строка сравнения по 0x644d90 проверена как UTF-16 Date.

## Native producer → storage → assignment/reference → consumer

`e2e5_script_pointer_int::Set` сохраняет исходную семантику SetInt для типа
результата и обычных числовых конверсий, затем оставляет полный адрес в уже
существующем широком IntValue/DwordValue. Пустая ячейка остаётся vkInt;
референс продолжает ссылаться на прежнюю ячейку. Getter всё ещё читает FlyToStar,
setter всё ещё пишет FlyDate. Новых полей или изменений layout TVarEC нет.

Второе сужение находилось в `TVarEC::Assume`: assignment в vkInt вызывал GetInt.
Для источников vkInt/vkDword теперь используется существующий native-width
GetDword/SetDword с сохранением типа integer destination. Обычные signed int32
и все проверенные DWORD-границы сохраняют прежние числовые значения; остальные
типы продолжают обычную числовую конверсию через GetInt.

Реальный asset-free runtime probe повторно компилирует effective aScriptFun.cpp
и EC_Expression.cpp с ASan/UBSan, не подменяя эти функции. Он проверяет:

- живые TRuins/TStar с адресами выше 4 ГиБ, исходный vkInt и 32-bit GetInt view;
- Date setter, обычный destination setter, nullptr, AssignFrom и Assume;
- ссылочную ячейку и настоящий SF_StarName, разыменовывающий адрес после getter,
  копирования и assignment;
- сохранение existing vkDword, float и string результатов, signed границы и
  DWORD 0/0x7fffffff/0x80000000/0xffffffff;
- числовой save/load, отказ записи высокого pointer-as-int и последующий
  успешный числовой save/load после очистки временного буфера.

До исправления probe завершался на проверке native адреса после Date getter;
после исправления прошёл без ошибок ASan/UBSan. Native-width callback/event
регрессия также прошла; Python tool regressions — 25/25.

```text
python3 tools/e2e5_host_game_build.py --profile optimized --jobs 2
python3 tests/probe_e2e59_film_history.py --ship-destination
python3 tests/probe_e2e59_film_history.py --width
```

## Полный host smoke и ARM64

Оптимизированный реальный host с установленными ресурсами прошёл New Game,
`generation warmup turn=300/300 done`, stage 8, mouse callback «Далее»,
`screen returned id=3 requested=20`, RuinsTalk и ангар с 60 кадрами.
Quicksave записан на ходу 301: 754274 байта, SHA-256
`5fc084fa553f88e37c26b6c1c9777600c6a9390f540481d5183cf3a0a90bcac4`.
Save inventory: 928 ячеек, UNKNOWN=0. После загрузки совпали ход 301, деньги,
ship/star/planet IDs и 16 скриптов; штатный ввод завершил следующий ход 302.
Это функциональная host-проверка, не измерение производительности Switch.

Локальные исходные отчёты: `build/ship-destination-before.log`,
`build/ship-destination-after.log`, `build/ship-destination-newgame.json`,
`build/ship-destination-reload.json`, `build/ship-destination-width-regression.log`.
Host binary SHA-256:
`e85e2a04d929ff404a0c9c6c4d694706b32efb0b2a20ecc35e68d7c5b8645513`.

Полностью скомпилированы 298 исходных и 82 platform единицы реальным devkitA64
16.1.0; ELF64 AArch64 PIE слинкован с undefined symbols=0. Это локальная
проверочная dirty-сборка на базе d63aa34, не clean release-кандидат.
Аппаратная запись сохранений по-прежнему выключена. Физического запуска
этого изменения нет; кандидат 3772031 этим отчётом не заменяется.

Effective inventory: 380 units / 1388 файлов, 219 классифицированных findings,
fixed_site_regression_failures=[]; прежнего REVIEW_NATIVE_SEMANTICS в этом
сужающем producer больше нет. Добавлены guards producer и int assignment.
Сырой lexical inventory всё ещё содержит 2401 unresolved записи — это другой
уровень анализа, не доказательство их безопасности и не полный lifecycle audit.
Win32 manifests изменились только по числу файлов 1410→1411; новых импортов нет.

## Открытые границы

RSG64/v168 и 32-bit disk ID не менялись. Высокий pointer-as-int не записывается
как обычный integer: существующий guard выдаёт контролируемую ошибку.
Поддержка произвольного постоянного сохранения Date-result как ссылки здесь
не добавлена; новая disk-kind/schema не вводилась. Успешный обычный game save/load
не является доказательством сохранения этого необычного pointer-as-int значения.
Числовые операции по-прежнему имеют исходную 32-bit int семантику; общий аудит
всех конверсий, ownership и поздних callbacks остаётся отдельной незакрытой работой.
Причина аппаратного зависания после «Готово» остаётся UNCONFIRMED.
