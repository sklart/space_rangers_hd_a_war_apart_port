# A4: native ShipDestination Date contract, 18.10.2026

Статус: **PARTIAL**. Подтверждён исходный PC-контракт; ARM64 runtime treatment
этого необычного pointer-as-int результата остаётся OPEN.

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
меняла gameplay/script semantics. Она не принята как ARM64 fix. Проверка не
доказывает, что результат безопасно сужать из native 64-bit pointer, и не доказывает
допустимость нового типа script value. REVIEW_NATIVE_SEMANTICS в аудите пока
сохранён: нужно установить потребителей/тип результата и затем проверить native
script вызов с высоким адресом и сохранение/восстановление. Это не A4 ownership PASS.


Дополнительно проверены native setters: 0x817214 при пустом result cell задаёт
kind=1 (vkInt), а 0x817390 — kind=2 (vkDword), затем записывает соответствующие
32-bit поля +0x0c / +0x10. Поэтому простая замена SetInt на SetDword также меняет
тип результата Date. Строка сравнения по 0x644d90 проверена как UTF-16 Date.
