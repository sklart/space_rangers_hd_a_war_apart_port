# P0.2: изменение размера в emergency reserve, 18.10.2026

Статус: **FIXED (host) / HARDWARE PENDING** для отдельного дефекта realloc.
Это не доказательство причины аппаратного зависания после «Готово»; P0.1 UNCONFIRMED.
Изменения в изолированном codex/stabilization-20261018, пока без commit.

## Воспроизведённая причина

Arena::Reallocate для reserve-owned блока всегда выделял другой блок, копировал
данные и освобождал старый. При недостатке свободного места это давало отказ даже
при уменьшении занятого буфера, для которого второй буфер не нужен.
Контролируемый native heap layout: 64 MiB buffer + 16 MiB neighbour + 47 MiB guard
в 128 MiB reserve; ordinary malloc/realloc >=65536 принудительно возвращает null.
До исправления попытка 64 → 32 MiB возвращает null при largest_free=1048448.
Это подтверждённый дефект контролируемого allocator path, не реконструкция всего
аппаратного состояния из pack.zip.

## Изменение

port/switch/platform/e2e510_heap_reserve.hpp:
- Reserve-owned realloc уменьшается на месте, возвращая свободный tail в arena.
- При росте используется соседний свободный блок, если его хватает.
- Tail немедленно сливается с соседним свободным блоком; split/free не выделяет metadata.
- Если рост на месте невозможен, сохраняется moving fallback и прежнее владение
  при отказе. Native heap mutex и проверка owner сохранены.
- Zero-fill растущей части остаётся обязанностью существующего heap_realloc(flags=8).
  Размер резерва, порог 64 KiB и обычный allocator не менялись.

## Проверки

Расширен существующий tests/test_e2e510_heap_reserve.cpp; запуск через
`python3 tests/probe_e2e510_runtime.py` уже входит в mandatory CI job.
До исправления тот же pressure case завершается assert(shrunk), в логе result=null.
После: shrink result=allocated, largest_free=33554400; затем free neighbour и
рост до 80 MiB без второго большого буфера. Проверены все 32 MiB сохранённых байтов
и нулевые байты расширения до 80 MiB.

Отдельная последовательность через настоящий native heap API: 1024 смешанные
alloc/realloc/free с размерами от 1 байта до примерно 4 MiB и заведомо чрезмерными запросами 256 MiB.
На host ASan/UBSan и ARM64/QEMU одинаково: 210 перемещений, 359 изменений на месте,
118 отказов. После каждого успеха проверены старые байты/zero-fill, после отказа —
старый owner/размер/содержимое. heap_destroy освобождает всё, LargestFree возвращается
к исходному значению. Сохранились прежние hardware-size allocation, 100 split/coalesce
cycles, overflow/ownership и generation-progress checks.

Доказательства: build/reserve-resize-before-confirmed.log,
build/reserve-resize-stress.log, build/reserve-resize-proof.json.
ASan backing — один malloc: границы внутренних arena allocations дополнительно
контролируются byte-проверками и полным слиянием, не одним только ASan.

## Ограничения

Тест принудительно отказывает большим ordinary allocations; это не воспроизведение
физической фрагментации всего libnx/newlib heap. Native reserve остаётся общей ареной
под native heap mutex; C++ vector/zlib allocations не перенаправлялись в неё.
Крупный request не может перескочить через живой соседний блок: при отсутствии
подходящего места отказ корректно сохраняет старую память. Никакой A2 hardware PASS
и устранения всех OOM/утечек этим не заявлено. P0.1 и общий A2 остаются открыты.


Дополнительно host ASan/UBSan и ARM64/QEMU: четыре одновременных native heap
owners, по 128 циклов alloc 65536 → realloc 131079 (zero fill) → realloc 32769
→ free, затем heap_destroy. Все byte checks, отсутствие tracked allocations
и полное слияние arena PASS. Лог build/reserve-resize-concurrent.log.
Это проверка mutex-protected native API; TSan и все игровые jobs этим не закрыты.


Обычный ARM64 compile/link/audit после изменения reserve PASS,
packaging SKIPPED (--link-only). Лог build/ordinary-reserve-resize-link.log,
полный отчёт build/ordinary-reserve-resize-link-report.json.


Combined ARM64 compile/link/audit после reserve resize тоже PASS.
Лог build/combined-reserve-resize-link.log, отчёт
build/combined-reserve-resize-link-report.json. undefined_symbols=0,
first_failure=null, packaging SKIPPED (--link-only). NRO не создавалась.


Полный optimized host после reserve resize: build/host-reserve-resize-build.log,
316 пересобранных зависимых единиц, link 331 объектов, SHA-256
`8890e70bf7db7983e98e8dfc4583325e08c165ffcef9a1b48f9b22026d07e5e1`.
Обычный opt-out New Game: build/host-reserve-resize-normal.json и checks;
85.03 с, crashed=false, timed_out=false, generation_complete=true,
два heartbeat, actual music worker bad_alloc, worker release, raw touch,
экраны 20/4 и 60+ кадров PASS. Без fixture seed и generation-world.bin.
Это функциональная проверка, не performance benchmark.
