# MatrixGame E610 r11 — bounded best-fit и immutable block metadata

Основание: r9 Interactive hardware counters: 117086 pool misses, создание
SDK blocks 36.268 с за весь запуск (включая Init), hits 377194. Распределение
реальных requested sizes не записывалось, точную трассу размеров восстановить
из этих totals нельзя. R10 с authored transition fix пока аппаратно не проверен.

## Изменения

CompletedBlockPool хранит size metadata при Put (один настоящий getSize).
Take/BestFitSize ищут по CPU metadata без SDK size calls. Exact Take сохранён,
новый bounded best-fit выбирает самый маленький свободный block >=requested
и <=maximum. Upload limit максимум 2x request и quota минус uniform. Uniform
memory остаётся exact 4096. Free slots=128/free bytes<=16 МиБ, live draw limit
64 и byte quota 256 МиБ не повышены. Cache eviction остаётся bounded.

PlanUploadReuse инспектирует свободные blocks без их выдачи. Если нужен drain,
сначала выполняется submit/wait, затем план пересчитывается. В batch.Add входит
физическая capacity всего upload block, включая padding, плюс uniform block.
Single draw около byte limit по-прежнему допускается: worst-case 2x заранее
не резервируется. Oversize/overflow отвергаются до изменения owners. Actual
NativeDrawUpload capacity сверяется с планом перед публикацией DrawPacket.
Memory не приобретается до выполнения требуемого drain.

NativeDrawUpload кеширует immutable size/CPU/GPU addresses при создании.
Проверка unique owner остаётся на каждом использовании, TakeCompletedMemory
инвалидирует metadata. Descriptor write не делает повторный SDK getCpuAddr;
address getters не вызывают getGpuAddr. Material Bind получает один реальный
uniform GPU address вместо повторных запросов, те же offsets/4 bindings.
SDK memory flags, vertex/index/descriptor/uniform bytes и per-draw cache
invalidation barriers не меняются. ReleaseCompleted — единственное место
возврата packet blocks после успешного queue completion; failure owners
сохраняются. Текстуры/shader/command/image blocks в пул не попадают.

Counters: block_pool_larger_hits, upload_requested_bytes, upload_reserved_bytes.
Reserved-requested — cumulative padding, не live memory. Cached free memory
учитывается отдельно; max_live_draw_bytes отражает physical packet capacity.

## Измерения и проверки без Switch

Controlled CPU size-sweep: 512x64=32768 draws, 512 submissions, одинаковый
порядок/содержимое. Frozen exact pool из source dd7f671 сравнен с новым:
allocation requests 6645 → 4751 (-28.5%), SDK size-query endpoints
1095345 → 65536 (около 16.7x меньше). Cumulative padding 83632128 байт,
peak live packet bytes 4214784. Это synthetic sizes/CPU owner endpoints,
не real GPU mapping latency, не восстановленная аппаратная size trace и не
прогноз FPS. Во всех Windows/host/ARM64 результаты counts совпали.

На сохранённых 92 frame counts: 208808 draws и 3310 submissions сохранены.
88 production upload layouts сравниваются с прежним four-block baseline:
exact payload/indices/descriptors/offsets/alignment/padding, fault unwind,
257 moved packet owners и partial allocation failures. Отдельно poisoned
блок 2x capacity переиспользован с прежними ranges; descriptor set очищен,
unused tail сохранён. Counters SDK endpoints доказывают отсутствие повторных
size/CPU/GPU-address запросов у live upload metadata getters/writes.

Shared production planner/native batch проверены на replan после drain,
физический byte accounting при padding, exact uniforms, single draw ровно
у quota, overflow/missing budget, getter-free lookup, queue failure retention.
Uniform packet byte tests и original Init handoff/journal failure checks
сохранены. Windows /O2, host ASan/UBSan/LSan /O2, ARM64/QEMU UBSan /O2 —
адресные проверки; результаты и SDK build заверяются отдельно при доставке.
Большие бои/общие регрессии не повторяются. Fixtures/CPU endpoints не входят
в NRO: приложение использует настоящие SDK allocation/copy/command methods.

## Граница

R11 HARDWARE PENDING. Содержит r10 authored transition fix без изменений;
его real capture/texture upload/animation ещё не проверены устройством.
Критерий четырёх map frames, таймаут 120 секунд, шаг игрового времени,
порядок draw и resource guards не ослаблены. Физический HID/FPS/звук, outcome,
Campaign/Gov/возврат Galaxy не приняты из CPU evidence. До Switch можно
доказать механизм и корректность данных/ownership, не итоговый GPU FPS.
[Инструкция](../docs/matrixgame-e610-reuse-r11-readme.ru.md).

## Проверенная доставка r11

Source commit: `203f563d6453b9a311af432a4e14428ad2f86ce7`; build ID `203f563d6453-f0031abf2bd4f34eacea`.
ZIP: `dist\matrixgame-e610-switch-test-r11-reuse.zip` (10160086 байт).
ZIP SHA256: `8ad13a187f531a41bff8b846af7ac998c8f56ded5bc20d1f65bc47a175177517`.

| NRO | SHA256 | ELF build ID |
| --- | --- | --- |
| matrixgame-e610-quick-smoke.nro | `a56fd1ac3cef6e9160dcff14669c95a11545c084156e817a9a06f94fbcf61e59` | `240545390a2526492eaf6008882babe672cfc5e7` |
| matrixgame-e610-interactive.nro | `97d38901a6729e7c91103cdb8aa4b8a28f4424e80c5aee7dbd6e8db2d1529952` | `8eeca38b8a28ac57ced0e388472baf1c1844ea92` |

Итог: **SOFTWARE PASS / R11 HARDWARE PENDING**. Обе devkitA64 compile/link,
strong undefined=0, NRO structure/font/shader/build-ID/title PASS. Compiler
provenance: 321 configured C/C++ units с O2/UNDEBUG, без выключения assertions/
exceptions или fast-math. SDK map/nm: 23 real deko3dd objects, 0 deko3d.
700 собственных source SHA совпали с metadata. Все generated original файлы
побайтово совпали с r10, включая authored transition fix. CPU preview/SDK
OriginalTransition hash совпал с историческими r10 oracle checks; аппаратный
путь r10 не объявляется принятым. Проверены 213 прежних файлов/пакетов без
изменений; main NRO/upstream/game resources/saves/frozen сохранены.
[Полное evidence](matrixgame-e610-reuse-r11-evidence.json).

Windows, host ASan/UBSan/LSan и ARM64/QEMU UBSan O2 — все 7 targeted cases
PASS: best-fit, actual upload bytes/larger storage/address cache, exact pool,
uniform packet, ordered batch, Init handoff, journal failures. Финальные
результаты: `build/matrixgame/e610-reuse-r11-checks/{windows,host,arm64}/2/`.
Frozen exact pool проверен по исходнику r10 source commit, все .inc inputs
сверены по SHA. Source audit подтверждает replan после drain и actual capacity
check до публикации packet; единственное место возврата blocks — retirement.

upload_requested_bytes считает исходный SDK Plan, включая его выравнивание
и descriptor slices. upload_reserved_bytes считает действительную capacity;
разность показывает дополнительный запас от best-fit reuse, не весь padding.

Для следующего ручного теста достаточно r11: обе NRO из ZIP →
`/switch/matrixgame-e610-r11-test/`. Названия: MatrixGame r11 Quick Smoke,
затем MatrixGame r11 Interactive. DATA/CFG и saves остаются на местах.
Interactive: до 120 с после Init, выход Minus+Plus. Передать два новых run
каталога, новые counters и наблюдения о скорости/переходе/изображении/HID.
До этого новое ускорение GPU/FPS, полный Init→transition→четыре map frames
и физический runtime r10/r11 остаются HARDWARE PENDING. Дальнейшие изменения
планировать по аппаратному профилю; CPU-model gain не заменяет это измерение.
