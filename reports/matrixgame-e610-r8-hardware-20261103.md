# Аппаратный r8 — 03.11.2026

Архив `G:/logs44.7z`, SHA256 `b8a4a1181581ac28d11cd9a59893c6927d93e396036f9829cf172e5b1ecbce8b`.
Оба self NRO SHA и build ID совпали с r8 package. Journals: 868 / 1199 records,
непрерывные seq, journal_close. Init/Init GPU handoff завершены, queue healthy,
SDK warnings=0, fatal/SDK errors отсутствуют. Оба вышли code 10 по deadline.
Ошибки acquire failure screen после deadline вторичны и не объясняют скорость.

| Параметр | Quick | Interactive |
| --- | ---: | ---: |
| Init, с | 61.930 | 62.435 |
| Minimap RenderBackground, с | 9.000 | 9.186 |
| Draw/Present в original loop | 34 | 75 |
| Период этих кадров, с | 55.152 | 121.366 |
| Средний Draw, с | 1.614 | 1.608 |
| Частота Present на переходе, кадра/с | 0.616 | 0.618 |
| Зачтённые map frames | 0 | 0 |

R7 Interactive: средний Draw 2.529 с, 48 кадров; r8: 1.608 с, 75 кадров.
Это наблюдаемое улучшение примерно 1.57x, не изолированный controlled A/B.
Все кадры ещё помечены transition; original-first.bgra отсутствует, smoke
не пройден. Результат боя/Galaxy/Gov, робот и физический HID не подтверждены.
Новых пользовательских наблюдений звука нет; прежнее «музыка вроде играла»
относилось к r7 и не переносится автоматически на r8.

## Измеренный профиль

Для Interactive после post-Init snapshot (включая последующие status/error
cleanup) дельты: draw_cpu=72.513 с, vertex_decode=36.288 с, block_create=14.121 с,
command_encode=6.512 с, batch_wait=5.811 с, residency-in-Record=0.735 с,
journal IO=4.924 с. Таймеры вложены; суммы не складывать. Counters .max_us
кумулятивные максимумы, их нельзя вычитать между snapshot. Residency timer
охватывает Record image retrieval, не весь decode/texture upload в других entry.

Pool после Init: hits=129020, misses=40816 (около 76% reuse), 64 max live
packets; free pool в конце 2351104 байт. CPU модель r8 со 128 allocations была
workload с фиксированными размерами и не обещала 128 allocations на аппаратном
mix разных размеров. Последний всё ещё делает десятки тысяч новых blocks.

## Найденная проблема сборки

r8 CMakeCache: CMAKE_BUILD_TYPE пуст. В настоящих native compile commands
(build.ninja) нет -O1/-O2/-O3; компилятор использовал не оптимизированный режим.
Это особенно существенно для std::array/span/map и многократного обхода
render registers/проверки индексов/декодирования вершин. R8 оптимизации reuse
и IO сработали, но native CPU код остался без compiler optimization.

R9 исправляет профиль сборки: -O2 -g -UNDEBUG -ffp-contract=off, exceptions и
assertions сохранены, debug deko3dd остаётся. Effective compile_commands.json
проверяется отдельным gate, профиль включается в fingerprint/package metadata.
Внешний предсобранный Rangers archive не меняется и не объявляется O2.
[Evidence](matrixgame-e610-r8-hardware-20261103-evidence.json).
