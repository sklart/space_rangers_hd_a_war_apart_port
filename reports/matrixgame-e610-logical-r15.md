# MatrixGame E610 r15 — draw-local logical register view

После r14 общий validation loop уже читает map одним проходом, но PixelFog
и Stencil до него продолжают делать отдельные tree lookups (6 fog + 13 stencil),
а raster getters повторно создают массив 11 Words. Следующий измеренный CPU
расход — подготовка этих значений до записи draw. Аппаратного профиля r10–r15
нет: это CPU workload, не доказательство крупнейшего bottleneck или FPS Switch.

В PrepareDraw один проход заполняет stack raw_words и presence. Unknown keys
внутри диапазона не используются getters, после MaximumRenderId обход
останавливается. Heap/GPU allocation нет. MaximumRenderId<256 проверяется
при компиляции (сейчас 209; до 1280 bytes raw/presence + 44 bytes raster view).
Renders special/vertex words используют прежний Render; raster words читаются
из одного raster.Words() value. Fog, stencil и все 103 logical words читаются
из этого draw-local view. Между draws/setters/snapshots ничего не кешируется.

Missing raw words остаются отличимы от нулей: read вызывает настоящий
registers.at(id), сохраняя std::exception type/what() платформы. MSAA check идёт
первым, далее неизменённый scissor guard, fog, stencil, все 103 words в прежнем
порядке, point/line/combiner/sampler checks. Предварительное копирование uint32
и noexcept raster.Words не добавляет ошибок и не меняет исходный failure order.
Public ValidateClear/PixelFog/Stencil/Render getters и неизвестные ID unchanged.
State layout/API/std::map/COW/snapshot/restore/allocations не меняются.
R10/r11/r12/r13/r14 behavior сохраняется, за исключением замены обхода raw values
r14 на единый view и повторного использования raster words в пределах draw.
Shader/uniform layout, GPU order/limits/lifetime, оригинальная игровая логика
не меняются. Upstream, frozen, ресурсы, saves, основная NRO и старые пакеты
сохраняются.

## CPU proof

Полный frozen native_device_state.cpp из r14 source
573cae7b5d968e5c68e9ecd643865141c295fb2b компилируется отдельно от candidate,
при одинаковых headers. 4245 случаев: исходные 4197 r14 и 48 новых комбинаций
scissor/MSAA/quality и missing fog/stencil keys, включая почти пустую map.
Getters 0..511, 103 words/8 values, setters/raw corruption/missing inactive,
24 extraneous-key cases, samplers, successive fog/stencil/factor/shade changes,
points/lines/MSAA/quality и simultaneous invalid fields. На успехе сравниваются
полные 4096 bytes (padding/alpha-target) и все stencil fields, на отказе
категория/what(). Stdout совпадает попарно внутри платформы; std messages
разных платформ не смешиваются. Отдельный frozen/current native_state test
проверяет COW/snapshot/OOM/restore/frame/ownership. Большие бои и полные suites
не повторяются; неизменённые upload/batch/journal checks сохранены как history.

Итоговая host O2 ABBAAB медиана full CPU draw preparation:
90945 → 77508 мкс / 100000 calls (−14.8%). Production PrepareDraw + vertex
Prepare + uniform_packet::Write, checksum 4000058935132545121 совпал в шести
запусках. Это synthetic valid CPU workload, не настоящий бой/GPU/FPS/Init.
Assertions on, без sanitizers/LTO/fast-math для timing, ffp-contract=off.
Итоговый замер и SDK результаты приводятся ниже.

## Границы

HARDWARE PENDING: Init→transition→4 настоящих map frames и физический GPU/HID/
audio/FPS требуют пользовательских запусков. Standalone entry не вызывает
единую Campaign/Gov, естественную победу и возврат Galaxy. Последние реальные
r9 Draw около 0.56 с не являются измерением r15.
[Инструкция двух запусков](../docs/matrixgame-e610-logical-r15-readme.ru.md).

## Итоговые CPU результаты

Host ASan/UBSan/LSan O2, ARM64/QEMU UBSan O2 и Windows O2 — PASS.
4245 cases / 1822 rejections; host/ARM64 digest 9044150971185390360,
Windows 12831936036603207056; frozen/current stdout попарно совпал.
Native-state ownership: restores=9 capture_OOM=2 COW_OOM=1,
restore_allocations=0; frame/expired-owner/held resource guards retained.
ARM64 local type_info linker workaround сохраняет UBSan/vptr.
Все проверенные source bytes совпадают с текущими точно, без EOL исключений.

Final benchmark/2: frozen 90945/89272/97913 мкс; candidate
77508/75998/80484 мкс. Медианы 90945 → 77508 (−14.8%). Первичная серия /1
до добавления stack bound/early stop дала −22.4%; сохранены оригинальные
compiled candidate/runner snapshots с проверенными recorded SHA.
Основное performance evidence — окончательная серия /2, без параллельных jobs.
Все checksum = 4000058935132545121. Timing — CPU synthetic valid workload.
Evidence: build/matrixgame/e610-logical-r15-checks/{host,arm64,windows}/1,
{host-ownership,arm64-ownership,windows-ownership}/1 и benchmark/2.
Runner: tools/matrixgame_logical_view_checks.py.

## Проверенная доставка r15

Source commit: `c1d27c2684c7c273c419e28b9c79999d051af7aa`.
Build ID: `c1d27c2684c7-47800742faa5aaa37b8c`.

Стандартные отдельные Quick Smoke/Interactive targets свежей SDK-сборки
`build/matrixgame/e610-logical-r15/1` — PASS. Неразрешённых strong symbols нет;
ELF/NRO build IDs, segments, icon/NACP, fonts/shaders проверены. PrepareDraw
реально связан с обеими NRO; Campaign/Gov entry не добавлена.

Compiler provenance: 321 configured TUs, RelWithDebInfo/O2/UNDEBUG. Assertions и exceptions retained, fast-math off.
Неизменённый prebuilt Rangers/UI archive не пересобирался и не объявляется
оптимизированным. Link maps: по 23 объекта deko3dd, без deko3d; debug
SetContext/RaiseError сохранены. Generated originals совпадают с r14;
относительно E601 остаются те же три прежние overlay delta.

704 own source hashes проверены точно. 241 старый защищённый файл,
старые ZIP/NRO, основная NRO, ресурсы/saves, frozen и upstream pins не менялись.
[Машинное evidence](matrixgame-e610-logical-r15-evidence.json).

ZIP: `dist/matrixgame-e610-switch-test-r15-logical.zip`, 10163325 bytes.
SHA256: `89842c9cd50e92a5d070d38abdf54b751753b6c498dff73cccddb8de06af54d3`.

| NRO | SHA256 | ELF build ID |
| --- | --- | --- |
| matrixgame-e610-quick-smoke.nro | `e15003e9311a2039fce823f6980ea8c3675d7e4b056307fbb9e13b4c3ce9136e` | `ac8f77d67d609d3a70ec2a06323a3e4b7b794591` |
| matrixgame-e610-interactive.nro | `9898c9bb6c19c01c5d09494ea216417900a14d60b8a3a06dea74da48c4ebf556` | `559a6253e58b6399e1c5f7b9eed58b37cdff9d08` |

В ZIP две NRO, README.ru.md, manifest.json, SHA256SUMS.txt и build-info.json.
Ресурсов/сохранений нет. Отдельная SD-папка `/switch/matrixgame-e610-r15-test/`,
прежние DATA/CFG roots сохраняются. Hbmenu: **MatrixGame r15 Quick Smoke** /
**MatrixGame r15 Interactive**. Application Mode, сначала Quick, затем
Interactive; штатный выход Minus+Plus. Собрать два новых каталога
`/switch/matrixgame-diag/logs/<run-id>/` и фото фактического результата.

**SOFTWARE PASS / HARDWARE PENDING.** Адресное изменение закончено;
дальнейшие исправления — по новому аппаратному профилю.
