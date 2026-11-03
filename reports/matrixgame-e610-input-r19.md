# MatrixGame E610 r19 — validation-local checked draw inputs

Следующий кандидат после r18 — NativeDrawIndexed вызывает state.Read() и
VB Storage() для каждой unique vertex. Оба дают одни и те же checked inputs
в callback-free read-only decode loop. Это CPU overhead, не hardware profile.

После ValidateIndexed и обеих vector allocations inputs получаются один раз:
const FixedVertexState reference и const VB byte span. Allocations остаются
раньше state/VB access, сохраняя bad_alloc vs expired-state порядок. C++17
postfix state.Read().vertex предшествует Storage argument как и прежде.
Смена bindings/state возможна до/после draw, но в decode loop нет callbacks,
Map/Unmap или Edit. Context удерживает VB, store удерживает state, decoder
работает только с disjoint unpublished output. View не сохраняется между draws.

Прежние decoded.at, subspan, FVF/stride/finite checks и index/base math остаются.
Ошибки всё ещё прерывают до Record. Decoder, Resource/NativeStateStore APIs,
state layout, refs/COW, allocation/cleanup, shaders/uniform/GPU order/quotas
не меняются. Функционально изменён только один draw body; r10–r18 retained.
Upstream/frozen/resources/saves/основная NRO/старые пакеты сохраняются.

## CPU proof

Reference: полный NativeDrawIndexed, native_device_state.cpp и fixed_vertex.cpp
из r18 source 1ef3da82d0f2909368bca7c598394bb35dfc4090. Условно выбирается actual
frozen/candidate gather body; decoder r17 DecodeTo в обеих версиях одинаков,
r18 staging header unchanged. NativeStateStore, buffers, ProfileScope настоящие.
Record заменён только внутри CPU fixture data sink, не в production.

Все r17 9864 cases, 864 FVF/padding layouts, полные 192 bytes, holes/defaults,
indices/order/base/topology и 216 failures без Record. Дополнительно U16/U32
rebind нового VB и изменение FVF между draws, затем закрытие state: OOM должен
остаться перед Read, expired-state не должен публиковать result. Semantic stdout
сравнивается с frozen отдельно для host/ARM64/Windows. COW/OOM/restore отдельно.
Большие боевые сценарии не повторяются.

## Timing

Короткая exploratory серия benchmark/1: gather median 24962 → 23830 мкс,
но unchanged direct-decode control заметно колебался. Замер расширен до
4096 indexed draws / 8388608 decoded vertices / 25165824 indices, direct control
10000000 decodes. Benchmark/2: frozen gather 398959/402023/397397 мкс,
candidate 375753/376100/378577; median 398959 → 376100 (−5.7%).
Control median 224602 → 218034 мкс; его code не менялся, это вариация измерения.
Primary final timing приводится ниже. Host O2 ABBAAB, UNDEBUG, ffp-contract=off,
no sanitizers/LTO/fast-math, no parallel jobs. Synthetic CPU gather, не GPU/бой/
FPS/Init; checksum 5258787986328867328 совпал во всех long runs.
Runner: tools/matrixgame_input_scope_checks.py; checks root e610-input-r19-checks.

## Границы

HARDWARE PENDING: Init→transition→4 map frames, physical GPU/HID/audio/FPS.
Standalone entry не проверяет единую Campaign/Gov, natural victory и возврат
Galaxy. Последний r9 hardware Draw около 0.56 с не является замером r19.
[Два ручных запуска](../docs/matrixgame-e610-input-r19-readme.ru.md).

## Итоговые CPU результаты

Semantic/ownership /1 на host, ARM64/QEMU и Windows — PASS. 9872 cases / 5404
expected rejections. Host/ARM digest 15352612592711171727, Windows
12932972604742491771 (bad_alloc/std messages отличаются между платформами).
Full stdout совпал попарно с frozen. Все 864 FVF/padding layouts, 192 bytes,
indices/holes и прежние failure guards сохранены; новые OOM/expired/rebind
случаи прошли без Record. Native state: restores=9 capture_OOM=2 COW_OOM=1
restore_allocations=0, held/expired/frame guards retained. Assertions on,
ARM64 type_info workaround сохраняет UBSan/vptr.

Final benchmark/3: frozen gather 410574/394799/414906 мкс, candidate
376993/377678/373061; medians 410574 → 376993 (−8.2%). Unchanged direct-decode
control 225382 → 215217 мкс; code не менялся, это вариация измерения/окружения,
не отдельная decoder optimization. Результат host microbenchmark не даёт
гарантии равного выигрыша на Switch. Все checksum 5258787986328867328.
Финальные source bytes совпадают точно. Все exploratory runs сохранены.
Автопроверка разрешения первого финального запуска завершилась тайм-аутом;
одна повторная попытка успешно выполнила замер, ограничения не обходились.
