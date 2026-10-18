# A3: настоящий EC_Cache, конкурирующий читатель и отказ, 18.10.2026

Статус: **PARTIAL / HARDWARE PENDING**. Два native fault cases PASS.

Запуск: `python3 tests/probe_e2e56_native_cache.py` после завершённого optimized host build.
Компилируется точный effective EC_Cache.cpp из source_map host report, а не копии тел
методов с заменёнными типами. Остальные объекты берутся из того же готового host build.
Проверяются binary SHA и timestamp, чтобы не смешать старый бинарник с новым reserve.

Настоящие TCacheEC/TCacheControlEC/TCacheDataEC, EC_Buf, pas class metadata, CacheLock
и platform event handles. Управляемый производитель наследует native cache data:
первый loader ждёт, затем бросает bad_alloc до либо после создания 65536-byte EC_Buf.
Это контролируемое исключение, не воспроизведение исчерпания physical heap.

В обоих случаях concurrent contender не получает незавершённый ресурс; после отказа
повторно загружает его. Проверены сброс binding/retain failed control, общие retained
данные двух controls, отсутствие повторной загрузки при acquire готового entry,
все байты payload, закрытие событий failed И successful load через настоящий
WinHandleTable, Release/TrimToBudget(0), ResidentBytes=0, пустые LRU endpoints,
нулевые live data/buffer counters и уничтожение cache/controls.

ASan/UBSan инструментируют EC_Cache.cpp и test producer. Прочие переиспользованные
игровые объекты optimized; это не full-game sanitizer run. Есть 20-second timeout.
Закрытые игровые ассеты не требуются, управление producer/payload создано в тесте.
Это сильнее проверки native method bodies с shim types, но не полный stress очередей
preload jobs/writers/cancel и не физическая Switch проверка. Полный A3 остаётся OPEN.

Host SHA-256: `8890e70bf7db7983e98e8dfc4583325e08c165ffcef9a1b48f9b22026d07e5e1`.
Effective cache source SHA-256: `7a636d0ccea22393802f538a9a782b8cdbe00ac9f3e487842a0f81d757f20d0c`.
Лог build/native-cache-reserve-resize.log; provenance build/e2e56/native-cache/proof.json.
Публичный CI этот дополнительный native integration в данном сеансе ещё не выполнял.


Расширение после первоначального SHA: добавлен настоящий TCBufEC accounting
и retained trim/release eviction regression. Before resident=0 при capacity=131335
FAIL; fixed PASS. Текущий host 5ce8a9af…07eb9f, ASan/UBSan теперь EC_Cache.cpp,
EC_CacheBuf.cpp и test, остальные 328 объектов optimized. Новый provenance в
build/e2e56/native-cache/proof.json; первоначальный в build/native-cache-initial-proof.json.
Подробности: cache-buffer-accounting-20261018.md.
