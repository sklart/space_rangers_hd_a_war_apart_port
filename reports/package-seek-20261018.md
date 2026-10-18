# Bounded reader: позиционирование и повторная проверка 18.10.2026

Статус: **FIXED (host) / HARDWARE PENDING** для лишних позиционирований.
Аппаратное зависание после «Готово»: первопричина **UNCONFIRMED**.
По уточнению пользователя завис весь экран и курсор; A и тап выполнить не удалось.
Отсутствие down/up не является доказательством промаха или неисправности hit test.

## Изменение

В port/switch/platform/package.{hpp,cpp} последовательное чтение использует известную
позицию FILE и размер уже прочитанного блока. Для обратного поиска добавлены 64
разреженных anchor: 512 байт на reader, без полного индекса или полной копии payload.
Короткое чтение/ошибка инвалидируют позицию; повтор тем же reader после восстановления
файла проверен. Ограничения packed 72112 / decoded 65536 и 16 readers сохранены.

## Проверки

tests/probe_package_streaming.py и test_package_streaming.cpp: ASan/UBSan, random seek
на 17 и 260 блоках, raw sequential/backward/EOF, восстановление после short read,
allocation denial, interleaved readers и 17 оригинальных ресурсов. Байты совпали с
независимым ReadPayload до/после. Контроль использует старые package.cpp И package.hpp.

| Сценарий | wrapped seek до | после | decode до/после |
|---|---:|---:|---:|
| Small random | 1524 | 116 | 418 / 418 |
| Large random | 3487 | 685 | 677 / 677 |
| Raw sequential | 7 | 1 | 0 / 0 |
| 2Open.gai, 13406836 байт | 409 | 1 | 205 / 205 |

Это вызовы stdio, не физические syscall. Wrapped fread не сравнивается: compiler
может inline/fold малые чтения. Максимальная C++ allocation на чтении 72112 байт
не является ограничением всей RSS или внутренних malloc zlib.

Настоящий TFileStreamEC: OOM при open/block, late failure, contending reader,
IdleEvent, join, fd cleanup и replacement PASS (build/file-stream-seek-asan.log).
Новый host SHA-256:
`712534555c571e94e1ebc7db9e2a103ab82538cdb32ce362cb54c773cf06695b`.
329 остальных объектов совпали с original-package control.

## Полная генерация

Три последовательных запуска с теми же seed=314159, именем и настройками.
Все payload: 2094630 байт,
SHA-256 `a6183264eb0d9c55f533a0fb4fae0fb94f1d666f0376f0ee18e0df47961faf61`.
Все 14 world/RNG checkpoints и native save transitions совпали с тремя control.

| Группа | Run 1, с | Run 2, с | Run 3, с | Median, с |
|---|---:|---:|---:|---:|
| Bounded + seek cache | 9.308419 | 12.247024 | 11.424134 | 11.424134 |
| Original whole-payload control | 9.648723 | 9.461698 | 9.465149 | 9.465149 |

Медиана всё ещё выше control на 20.70%. Устранение лишних stdio seek подтверждено;
устранение общего замедления НЕ подтверждено. Частоты/температура/внешняя нагрузка
не измерялись; один быстрый run не доказывает ускорение. Performance gate OPEN.
Подробные данные: build/package-seek-io-comparison.json,
build/package-seek-host-object-comparison.json, build/determinism-seek-vs-legacy.json.
Не выполнено нового аппаратного прогона; NRO не создавалась. Изменения пока без commit.


Обычный и combined ARM64 compile/link/audit после этого изменения: PASS.
Логи build/ordinary-package-seek-link.log и build/combined-package-seek-link.log.
Обычный ELF SHA-256: EECF24AC334107305DB0D793E36F64B85713A21E1ACFB0D9FBAB9DC1062FE866.
Это локальная проверка изменённого Package layout и линковки, не запуск на Switch.


Обычный host opt-out: build/host-package-seek-normal.json и отдельные checks.
71.54 с, crashed=false, timed_out=false, stopped_by_probe=true; 300 ходов,
два heartbeat, actual music worker bad_alloc, worker release, raw touch,
экраны 20/4 и 60+ кадров PASS. Без fixture seed и generation-world.bin.
Время этого функционального прогона не используется как performance benchmark.
