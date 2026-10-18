# P0.2: ёмкость native raw-cache буферов, 18.10.2026

Статус: **FIXED (host) / HARDWARE PENDING** для учёта owned raw buffer capacity.
Аппаратное зависание после «Готово» по-прежнему UNCONFIRMED.

## Причина и изменение

Подтверждено на настоящем TCBufEC: LoadFromConfigBuffer копировал source payload
в собственный TBufEC, но оставлял ResidentBytes=0. Native cache добавляет именно
Data.ResidentBytes к своему ResidentBytes и использует эту сумму в TrimToBudget.
Так raw quest/script buffers не прибавлялись к бюджету. В исходных игровых логах
71 PlanetQuest cache load завершились с bytes=0.

В disposable overlay tools/e2e_source_overrides.py после успешного AddBytes теперь
ResidentBytes = Buffer->Capacity. Pinned upstream не менялся. Отражён новый override
в build report generator. Не изменены форматы, текст квестов, RNG или расчёты ходов.
Считается ёмкость payload allocation; это не полная RSS и не учёт allocator metadata.

## Before/after и lifecycle

Расширен tests/test_e2e56_native_cache.cpp, runner компилирует точные effective
EC_Cache.cpp И EC_CacheBuf.cpp из завершённого host source_map с ASan/UBSan.
Остальные 328 объектов optimized; это не full-game sanitizer coverage.
Настоящий raw buffer до исправления: data=131079, capacity=131335, resident=0,
assert accounting FAIL (build/native-cache-buffer-before.log).
После: data=131079, capacity=131335, resident=131335. Проверены все данные,
защита retained entry от TrimToBudget(0), eviction после Release, ResidentBytes=0
и отвязка control. Оба прежних pre/post-buffer bad_alloc с конкурентным читателем,
повтором и настоящими event handles тоже PASS (build/native-cache-buffer-after.log).
Проверка не читает закрытые игровые ассеты. Старый proof сохранён отдельно,
новая попытка заранее инвалидирует прежний PASS до своего успешного завершения.

## Настоящий игровой маршрут

Host изменил только один игровой объект EC_CacheBuf.o, link 331 объектов.
SHA-256 `5ce8a9af61f89c3dbb1f4f9315907745653a7f22180e5205cf7cbdeb0407eb9f`.
build/host-cache-buffer-normal.json: 79.5 с, 300 ходов, реальный music worker fault,
worker release, два heartbeat готового UI, raw touch, экраны 20/4, 60+ кадров;
crashed=false, timed_out=false. Без fixture seed/snapshot.
Все 71 загруженных PlanetQuest entries теперь имеют ненулевой bytes, сумма
owned capacities 18250536 байт. Это сумма загрузок, не отдельный замер текущей RSS
или гарантия одновременной резидентности всех entries после возможной eviction.
Полные checks: build/host-cache-buffer-normal-checks.json.

Обычный ARM64 compile/link/audit нового override PASS,
build/ordinary-cache-buffer-link.log и matching report. Combined ARM64 compile/link/audit тоже PASS: build/combined-cache-buffer-link.log
и matching report, undefined=0, stubs=[], first_failure=null.
Нового NRO/commit/push пока нет. Это исправляет бюджет raw cache, но не доказывает
причину исходного физического OOM/замершего экрана и не закрывает все A2/A3 пути.


Через настоящий e2e5_memory_recovery::Reclaim проверены оба случая: пока raw entry
retained — false и entry сохраняется; после Release — true, данные освобождены,
ResidentBytes=0. Лог build/native-cache-buffer-recovery.log. Это правильное распознавание
освобождения для повторной попытки, не гарантия успеха любого последующего malloc.
Python gates: 25 tool regressions + 8 staging + 8 generation comparer PASS.


## Повторяемость после изменения бюджета

Три полных New Game fixture на текущем SHA 5ce8a9af…07eb9f прошли strict comparator
против тройки bounded/seek-cache до raw accounting (SHA 8890e70b…07e5e1).
Формат, seed=314159, имя, input hashes и источники ресурсов совпадают.
Все payload 2094630 байт SHA-256
`a6183264eb0d9c55f533a0fb4fae0fb94f1d666f0376f0ee18e0df47961faf61`,
все 14 world/RNG checkpoints и native save transitions совпали.
Данные build/determinism-cache-buffer-vs-before.json, лог build/cache-buffer-generation.log.
Времена 11.952565 / 11.101220 / 11.077923 с, median 11.101220 с.
Это хронологические группы; они подтверждают игровую эквивалентность, не ускорение.
Предыдущее +8.10% A/B наблюдение относилось к прежней паре SHA и не переносится
автоматически на новый ELF. Performance/батарея/физическая Switch ещё OPEN.


## Компактный итог блока

| Поле | Результат |
|---|---|
| Исходное замечание | P0.2 учёт raw cache: реальные логи на 8890e70b, 71 quest load bytes=0; аппаратный pack.zip остаётся исходным P0.1, но причинная связь не заявлена. |
| Причина | CONFIRMED (код/native regression): TCBufEC не заполнял ResidentBytes после owned buffer copy. Hardware freeze UNCONFIRMED. |
| Изменения / Git | tools/e2e_source_overrides.py, tools/e2e_arm64_build.py; native regression расширен в двух tests. Base 0f91c1ab48328f187e10f58682d318b2487176bd, ветка codex/stabilization-20261018, новый commit/push ещё не выполнен. |
| Проверки | Host normal + native ASan/UBSan/recovery PASS; ordinary/combined ARM64 link/audit PASS; local Python 41 PASS. Remote CI и физическая Switch не проверены этим блоком. |
| Эффект | 71 реальные загрузки учитывают сумму capacities 18250536 байт вместо нуля; retained protection и reclaim после release PASS. Это не RSS/contiguous-free measurement. |
| Регрессии | 3 complete worlds на 5ce8a9af…07eb9f: full payload, 14 RNG/world checkpoints, native save transitions совпали до/после. Усиление budget eviction не изменило проверенный мир. |
| Риск | Полный cache/jobs/writer stress, concurrent reclaim snapshot и hardware memory pressure ещё открыты. Причина неподвижного аппаратного UI не доказана. |
| Следующий шаг | Адресная native TSan проверка concurrent Reclaim/ResidentBytes под CacheLock. |
