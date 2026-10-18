# A3 / P0.2: конкурентный Reclaim, 18.10.2026

Статус: **FIXED (host) / HARDWARE PENDING** для подтверждённой гонки ResidentBytes.
Это не доказанная причина физического зависания после «Готово».

## Подтверждённая причина

Reclaim считывал GlobalCache->ResidentBytes до и после TrimToBudget без общей
блокировки. Producer меняет этот счётчик под CacheLock при публикации данных.
Кроме C++ data race, отдельные несогласованные snapshots могут неверно описывать
эффект очистки при промежуточной конкурентной публикации. Воспроизведён именно
race, а не каждый возможный ошибочный return value.

Настоящие EC_Cache.cpp / EC_CacheBuf.cpp / EC_Thread.cpp и test producer
инструментированы ThreadSanitizer; остальные 327 объектов взяты из готового optimized
host build. EC_Thread включён в замены, чтобы Reclaim не разрешался из старого
неинструментированного inline/weak экземпляра. Остальной engine не является
полностью инструментированным и не объявлен race-free.

До исправления: TSan exit=66, write AcquireDataFromDirectKey (generated EC_Cache.cpp:306)
под mutex против read Reclaim без mutex. Адрес символизирован по сохранённому before
ELF в before e2e5_memory_recovery.hpp:25. Старый header/ELF сохранены в ignored
build/reclaim-control; hashes и source provenance в build/reclaim-lock-proof.json.
Лог build/native-reclaim-tsan-before.log.

## Исправление

port/switch/platform/e2e5_memory_recovery.hpp теперь сохраняет borrowed cache pointer
и удерживает существующий CacheLock на обоих snapshots и TrimToBudget. Runtime
CriticalSection рекурсивный, поэтому внутренний Trim не создаёт self-deadlock.
ScopeExit освобождает внешний уровень блокировки; diagnostic I/O выполняется после
выхода из critical section. Cache lifetime обязан оставаться валидным у вызывающего
кода; concurrent destruction этим изменением не исправляется и не проверяется.

## Проверки

Тот же stress после исправления: 200 настоящих cache load/publish/release/trim
и 2441 параллельный Reclaim, TSan PASS; buffers/data/counter cleanup PASS.
Producer payload синтетический, cache, class metadata, locks и event handles настоящие.
Изначальные cases до/после выделения буфера, concurrent retry, raw capacity accounting,
retained protection и actual recovery reclaim ASan/UBSan PASS. Native FileStream
OOM/late read/contending reader/IdleEvent/join/fd cleanup/replacement тоже PASS.
Логи build/native-reclaim-{tsan,asan}-after.log,
build/file-stream-reclaim-lock-asan.log. Сам тест не читает закрытые ассеты.

Host пересобрал только EC_Thread.o, SHA-256
`11a5e0c96078c95b3d54ccae01d2eeeded18162ae4e032134afa152e173fb66e`.
Normal route build/host-reclaim-lock-normal.json и checks: 57.42 с, 300 ходов,
реальный music worker bad_alloc, worker release, два heartbeat, raw touch, экраны
20/4 и 60+ кадров. crashed=false, timed_out=false, seed/snapshot opt-in отсутствовал.
Это функциональное время, не speedup measurement.
Ordinary/combined ARM64 compile/link/audit PASS, undefined=0, stubs=[], first_failure=null;
логи build/{ordinary,combined}-reclaim-lock-link.log и matching reports. NRO не создавалась.

## CI

В существующий обязательный host-regression добавлены complete optimized Linux host
build без ассетов, native cache ASan/UBSan, concurrent reclaim TSan и native file-stream
ASan/UBSan. Добавлена libsdl2-dev. Пять jobs и required release dependencies сохранены;
публикация NRO/ELF остаётся только после успешного push master. YAML и локальные
Python gates проверяются отдельно. Реального GitHub run/download proof пока нет.

## Остаток

Full cache/preload/writer/cancel stress и объектный lifetime всего engine остаются
PARTIAL. Hardware freeze UNCONFIRMED; данные старого pack.zip не устанавливают,
что эта гонка выполнялась в момент замершего интерфейса. Новый scoped commit,
clean combined NRO/ELF, remote CI и физическая проверка ещё нужны по общему ТЗ.


## Три полных мира текущего SHA

После исправления 11a5e0c9…3fb66e три New Game fixture: 9.535782 / 9.487811 /
9.379595 с. Все payload 2094630 байт SHA-256
`a6183264eb0d9c55f533a0fb4fae0fb94f1d666f0376f0ee18e0df47961faf61`,
все 14 world/RNG checkpoints, input hashes и native save transitions строго
совпали с тройкой до Reclaim fix (5ce8a9af…07eb9f). Comparator PASS,
build/determinism-reclaim-lock-vs-before.json. Это последовательные эпохи,
не controlled speedup comparison; improvement времени не приписывается lock fix.

| Поле | Итог блока |
|---|---|
| Исходное замечание | A3 concurrent recovery; физический pack.zip содержит OOM/замерший UI, но causal correlation с этой гонкой не установлена. |
| Причина | CONFIRMED native TSan read/write data race на ResidentBytes: producer под CacheLock, Reclaim без lock. |
| Изменение / Git | e2e5_memory_recovery.hpp: один lock для snapshot/trim, ScopeExit, logging снаружи. Native runner/stress и CI host-regression расширены. Base 0f91c1ab48328f187e10f58682d318b2487176bd, codex/stabilization-20261018, новый commit/push ещё не выполнен. |
| Проверки | Native TSan before exit66 / after 200 loads +2441 reclaim PASS; native ASan/UBSan, real FileStream fault/cleanup, host normal, ordinary/combined ARM64 PASS. YAML + Python 25+8+8 PASS. Remote CI/HW ещё не проверены. |
| Эффект | Синхронизированы наблюдения счётчика и очистка; concurrent publication не вклинивается между snapshots. Не заявлено измеримого Switch ускорения. |
| Регрессии | Retained protection/released reclaim, 3 полных world/RNG/native-save сравнения текущего SHA PASS. |
| Остаточный риск | Full-engine instrumentation, cache destruction/lifetime, все preload/writer/cancel paths и аппаратный freeze остаются открыты. |
| Следующий шаг | Свести готовые локальные исправления и доказательства для одного clean кандидата; отдельно проверить remaining lifecycle gates перед его выдачей. |
