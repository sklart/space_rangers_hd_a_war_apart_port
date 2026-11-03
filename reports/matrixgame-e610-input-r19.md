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

## Проверенная доставка r19

Source commit: `a2f87f3a40d72153a10b76a0fb4193535c935a9f`.
Build ID: `a2f87f3a40d7-0a8972578dd5db930f98`.

Свежие SDK targets в build/matrixgame/e610-input-r19/1 — PASS. Strong undefined
symbols отсутствуют; ELF/NRO build IDs, segments, icon/NACP, встроенные fonts/
shaders проверены. Actual ARM64 NativeDrawIndexed в обеих NRO содержит 0 calls
к NativeStateStore::Read и Resource::Storage в decode backedge против r18 по
1 call site каждого. State Read выполняется один раз до loop; Storage дважды
в caller (IB и VB), оба вне decode-loop. Scope/адреса записаны в evidence.
R18 coverage implementation и r17 DecodeTo без 192-byte return copy сохранены.
Это target instruction proof, не измерение GPU/консоли.

Compiler provenance: 321 configured TUs, RelWithDebInfo/O2/UNDEBUG, assertions/exceptions on, fast-math off.
Неизменённый prebuilt Rangers/UI archive не пересобирался и не объявляется
оптимизированным. По 23 deko3dd objects в link maps, без deko3d; SDK debug
SetContext/RaiseError retained. Generated originals совпадают с r18,
относительно E601 остаются те же три прежние overlay delta.

709 own source hashes и финальные CPU inputs совпали точно. 269 прежних
защищённых файлов, upstream/frozen, ресурсы, saves, основная NRO и старые
NRO/ZIP не изменены. [Машинное evidence](matrixgame-e610-input-r19-evidence.json).

ZIP: `dist/matrixgame-e610-switch-test-r19-input.zip`, 10162353 bytes.
SHA256: `8e8133756b33d4b404c51c28efb08ff3005297828de44c97d93456de58ac8ddf`.

| NRO | SHA256 | ELF build ID |
| --- | --- | --- |
| matrixgame-e610-quick-smoke.nro | `a56e0c15da048c11bb6d3e1d3e6b1803dc2adf5be3ba033db56d4b2b6787ebfb` | `1dcda8f1b867245e11fc0834334cbed79ae5b60d` |
| matrixgame-e610-interactive.nro | `7ba15e32860c0cc54823995f1157a71210d94b531450053d966ffc8e1b827a3d` | `035de985f6a9b540d3e5efc4ef9e229422a6b780` |

В ZIP две NRO, README.ru.md, manifest.json, SHA256SUMS.txt и build-info.json.
Ресурсов/сохранений нет. SD-папка `/switch/matrixgame-e610-r19-test/`,
DATA/CFG roots прежние. В hbmenu **MatrixGame r19 Quick Smoke** /
**MatrixGame r19 Interactive**. Application Mode, Quick затем Interactive,
выход Minus+Plus. Передать два новых каталога
`/switch/matrixgame-diag/logs/<run-id>/` и фото фактического результата.

**SOFTWARE PASS / HARDWARE PENDING.** Адресное изменение завершено;
следующие исправления — по новому аппаратному профилю.
