# MatrixGame E610 r18 — validation-local initialized coverage

После r17 следующий CPU расход в indexed draw — Resource::Initialized для
каждого индекса, включая повторные. В полностью заполненном VB эти lookup
возвращают true после одинаковых проверок. Все original bounds/initialized
операции должны оставаться логически обязательными, partial VB и unused holes
нельзя отклонять по одному лишь declared vertex range.

## Изменение

Внутри ValidateIndexed создан локальный VertexCoverage, недоступный внешним
callers. Он хранит binding, неизменяемый размер, quotient maximum_index и
аутентичный результат Initialized(0,Size). Constructor не бросает/не выделяет
память. Он используется только в callback-free read-only index loop; между
вызовами ничего не кешируется. Resource bytes vector не resize; Scope не
может пережить запись/Discard. Bindings retain storage как прежде.

Каждый index сначала проверяется по declared minimum/count в прежнем порядке.
Затем остаются null/locked/stride/negative guards, bound quotient и полный
байтовый диапазон vertex stride. В exact-end случае сохраняется прежняя ошибка
uninitialized, а не byte-range. Для fully initialized VB истинность coverage
доказана одним запросом, но bounds проверяются для каждого индекса. Для partial
VB продолжается старый Initialized(offset,stride); unused holes не блокируют
рисование. Public Vertex helper и остальные Resource/Map/Unmap APIs unchanged.
Новых persistent state/ABI fields/heap/GPU allocations нет; records, decoder,
indices/order/base, shaders/uniforms, quotas/ownership/cleanup не меняются.

## CPU proof

Frozen reference — staging_draw.hpp и полные CPP r17 source
0d18392f001a8566f8f676bb4220c0a4e25e3d53. Test wrapper выбирает frozen/candidate
header явно; actual NativeDrawIndexed и decoder одинаковы в обеих версиях.
11828 cases / 7168 expected rejections, 1948 coverage scenarios, digest
17308508795845681009 совпал на host/ARM64/Windows. Включены все r17 9864 cases,
864 FVF/padding layouts, full 192 bytes, holes/indices/base, 216 failures без
Record. Дополнительно full→Discard→partial, referenced vs unused holes,
empty/partial/full VB, strides 1/32/33/128/129/SIZE_MAX, offsets 0/1/127,
zero/end/past-end/65535 indices, base -1/0/1 и declared vs missing VB error order.

CPU-only counter добавлен в копию Resource::Initialized; остальная body точно
production, оригинальный CPP не изменялся. На U16 и U32 full case (6 indices)
проверено 7→2 real calls; partial fallback 7→8. Счётчик не включён в timing и
NRO. Production доказательство coverage всегда берётся из real Initialized,
никаких fabricated readiness/always-true flags. Code assertions не ослаблялись.

Host ASan/UBSan/LSan O2, ARM64/QEMU UBSan O2, Windows O2 — PASS. ARM64 local
type_info workaround сохраняет UBSan/vptr. Native state ownership/COW/OOM/
restore: restores=9 capture_OOM=2 COW_OOM=1 restore_allocations=0; guards retained.
Большие бои и полные suites не повторяются. Record в CPU fixture — data sink,
не GPU execution; в production Record unchanged.

## Timing

Первый вариант view без cached size/quotient не дал устойчивого выигрыша,
сохранён benchmark/1. Вариант с size/maximum-index даёт предварительно
28813 → 26093 мкс CPU gather (−9.4%). Primary final timing приводится ниже.
Uninstrumented production Resource CPP, host O2 ABBAAB, no sanitizers/LTO/
fast-math, ffp-contract=off, assertions on. 256 synthetic indexed draws,
524288 decoded vertices, 1572864 indices; checksum 1052755254566562048.
Direct decode — unchanged control. Это не FPS/Init/GPU Switch. Частота full vs
partial VB на устройстве пока не измерена; partial получает один дополнительный
coverage query на validation, без изменения acceptance behavior.
Runner: tools/matrixgame_coverage_checks.py; checks root e610-coverage-r18-checks.

## Границы

HARDWARE PENDING: Init→transition→4 real map frames, physical GPU/HID/audio/FPS.
Standalone entry не вызывает единую Campaign/Gov, natural victory и возврат
Galaxy. Последний аппаратный r9 Draw около 0.56 с не является измерением r18.
Upstream/frozen/resources/saves/основная NRO/старые пакеты сохраняются.
[Два ручных запуска](../docs/matrixgame-e610-coverage-r18-readme.ru.md).

## Итоговые CPU результаты

Semantic/ownership /1 на host, ARM64/QEMU и Windows — PASS; stdout попарно
совпадает с frozen. 11828 cases, 7168 expected rejections, digest
17308508795845681009. В semantic сборке использована только counted copy
Resource CPP, в production/timing оригинальный Resource CPP. Native state
allocation/failure/lifetime guards совпали с frozen. Все source/header bytes
и условный выбор frozen/candidate зафиксированы hashes.

Final benchmark/3: frozen gather 27828/27711/30820 мкс, candidate
25217/25469/27807; median 27828 → 25469 (−8.5%). Checksum
1052755254566562048 во всех runs. Unchanged direct-decode control medians
23509 → 22754 мкс; это вариация замера, decoder не менялся. Timing получен
без счётчика и параллельных jobs. Benchmarks /1 и /2 сохранены как exploratory.
Эти значения не являются FPS Switch или полным Draw/Init.
