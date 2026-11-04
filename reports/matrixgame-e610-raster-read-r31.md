# MatrixGame E610 r31 — прямое чтение raster word

После r30 NativeRenderState::Render getter для raster ID выполнял
raster.Words()[index]: строил временный массив из всех 11 значений ради одного.
R31 читает нужное поле через switch по типизированному RasterRegister.
Lookup(id) остаётся первым, cull/shade/factor/vertex идут в прежнем порядке,
остальные logical registers читаются тем же registers.at(id).

Getter раньше не вызывал Validate и сохраняет это поведение: invalid enum/
mask/reference values возвращаются как raw uint32. Bool поля читаются как bool;
тестируются все 16 легальных сочетаний, invalid representation bool не создаётся.
Missing map key у raster ID по-прежнему не мешает чтению component state.
Полные Words() остаются в PrepareDraw, где сразу нужны все значения; этот путь
не меняется. Setter/Apply, sampler getter r30 и всё остальное CPP unchanged.
Новых mutable fields, API, allocations, таблиц, кеша или ownership изменений нет.
Reference: r30 source 84615a9f00bc7593f8db6fa53a211d598ce63145.

Путь используется DekoDevice::ReadRenderState и NativeStoredStateBackend getter
при capture/restore состояния. Доля времени этих запросов в реальном кадре
Switch неизвестна; улучшение throughput не выдаётся за whole Draw/Init/FPS.

## Проверка

Сохранён предыдущий corpus 66311 cases (25511 scalar, 40800 sampler operations).
Добавлены 17920 raster states: четыре rounding modes, 16 bool patterns,
каждое из семи числовых полей, 20 значений включая bounds/raw NaN-like bits,
обычная и отсутствующая в map группа raster keys. По 11 getter reads на state,
снятие FP flags и full-state snapshot после запросов. Новые 197120 reads
проверяют отсутствие validation/мутации на invalid raw fields.
Всего 84231 cases/49296 expected rejections.

Benchmark: 6750208 actual Render getters всех 103 words, отдельно 2883584 raster
getters, 1687552 Render setters и 100000 PrepareDraw controls. Production CPP
at O2/UNDEBUG, no sanitizer/LTO/fast-math, ABBAAB без parallel CPU jobs.
Одинаковый warmup: initial setters, 10000 PrepareDraw, 1000×103 getters.
Checksum 3953759491520818715 одинаков во всех runs первого замера.

Exploratory /1: mixed getter median 49646 → 39045 мкс (−21,35%),
raster getter 11144 → 8181 (−26,59%), setter 26325 → 24714,
PrepareDraw 68145 → 57766. Разброс велик: frozen mixed getter 44801..65302.
Итоговый замер выполняется после semantic gates; из первого контроля нельзя
заключать о самостоятельном ускорении PrepareDraw.

Большие Windows-бои и полные regression suites не повторяются.
Ресурсы/saves/upstream/frozen/основная NRO и предыдущие пакеты сохраняются.
HARDWARE PENDING: physical GPU/HID/audio/Init/map frames/FPS.
Standalone entry не охватывает unified Campaign/Gov, natural victory/Galaxy return.
[Два ручных запуска](../docs/matrixgame-e610-raster-read-r31-readme.ru.md).


## Итоговые CPU результаты

Host ASan/UBSan/LSan, ARM64/QEMU UBSan и Windows O2 /fp:strict /1 — PASS.
Полный stdout frozen/current совпал попарно. Digest host 17542823901314383552,
ARM64 16879654082888188000, Windows 12350428784088286840.
Межплатформенные FP flags/messages не являются acceptance. Assertions active,
ARM64 UBSan/vptr сохранён с прежним type_info symbol workaround.
Прежний corpus 66311 сохранил все свои checksums; новый segment добавлен сверху,
а не заменил старые проверки. Никакие assertions не ослаблены.

ABBAAB /2 после semantic gates, checksum 3953759491520818715 одинаков:

| CPU metric, мкс | frozen r30 runs | r31 runs | median до → после |
| --- | --- | --- | --- |
| Mixed getters | 44689 / 48701 / 46745 | 39089 / 39959 / 38662 | 46745 → 39089 (−16,38%) |
| Raster getters | 10706 / 10485 / 10959 | 8261 / 8324 / 8580 | 10706 → 8324 (−22,25%) |
| Setter control | 25351 / 28377 / 24681 | 22643 / 24761 / 23163 | 25351 → 23163 (−8,63%) |
| PrepareDraw control | 62406 / 62575 / 61701 | 56552 / 60556 / 55418 | 62406 → 56552 (−9,38%) |

Выигрыш обоих getter workloads повторился в /1 и /2; диапазоны final getter
не пересекаются. Setter/PrepareDraw source не изменён; их разница фиксируется
как контроль, без отдельного утверждения об ускорении этих операций.
Изменение формы getter может влиять на inlining/layout соседних вызовов;
цель не включает объяснение всего сдвига контроля по короткому host benchmark.
После /2 дополнительные timing runs для подбора более выгодной цифры не выполнялись.

## Воспроизведение

CPU: `python tools/matrixgame_raster_read_checks.py host` (также arm64/windows),
затем `python tools/matrixgame_raster_read_checks.py benchmark` без parallel jobs.
Runner сохраняет frozen CPP, команды/stdout/source hashes в новый numbered каталог.
SDK: `python tools/matrixgame_current_diagnostic_sdk.py --output-root
build/matrixgame/e610-raster-read-r31`. Новому выводу всегда дать новое имя.
Локальный delivery gate: build/matrixgame/e610-raster-read-r31-checks/verify_delivery.py
с source commit как аргументом.


## SDK/package и direct-read proof

Source commit: `4bdceb16e56f611de2d7514430cbf42c427cb328`.
Build ID: `4bdceb16e56f-c127e1439ddb7b94b3a7`.
Обе отдельные стандартные devkitA64/libnx/deko3d compile/link — PASS,
strong undefined=0. 321 configured TUs: O2/UNDEBUG, exceptions, no fast-math.
Настоящие 23 debug deko3dd object members / 0 release, mixed=false.
Prebuilt Rangers archive unchanged, не выдаётся за новую optimized сборку.
NRO structure/assets, встроенные fonts/shaders, NACP titles и ELF→NRO IDs проверены.
Generated original files identical r30 (три исторических delta vs E601).
722 own source hashes проверены. Production diff vs r30 — только render getter,
дополнительно r31 titles/CPU runner; прочие build inputs unchanged.
353 прежних защищённых артефактов SHA unchanged; game/frozen/main NRO gate PASS.

В обеих actual SDK ELF временная Words-копия исчезла: frame 96 → 32 bytes,
нет прежних ZIP1 и STUR D на стек. GCC16 также встроил Lookup: ID<=209 bound
и word-presence guard направляют ошибку в один прежний out_of_range block,
до component dispatch. Существующий vertex forwarding сохранён.
Bounded static instruction-path audit проследил все 11 raster ID после native
validation: ровно один правильный integer load на путь, четыре LDRB для bool,
семь LDR W для enum/uint32. Типы полей/offsets подтверждены frozen header и
исходным layout. Это статический аудит фактического ARM64-кода, не исполнение
NRO/QEMU и не аппаратная приёмка. В factor-ветке сохранены четыре FMUL/FCVTAS;
source PackQuantizedArgb и числовые правила неизменны.

RenderLookup 630 bytes, LogicalRules 103 bytes, SamplerLookup 28 bytes и
Matrix r25 machine bytes exact r30. Полный disassembly обеих ELF в evidence.
Таким образом, mixed getter gain не ограничивается стоимостью 11 полей:
изменение формы функции влияет и на оптимизацию её общего dispatch/Lookup.
Глобальный вклад в Draw/Init/FPS без аппаратного профиля неизвестен.

ZIP: `dist/matrixgame-e610-switch-test-r31-raster-read.zip` (10160869 bytes).
SHA256: `c624b13ed17b6bcdeefcf138fe02275817d22f59f6bcffcf1a8854b6d6646944`.
Ровно 6 файлов: две NRO, README.ru.md, manifest.json, SHA256SUMS.txt, build-info.json.
Игровых ресурсов и сохранений нет. ZIP и распакованный пакет повторно
проверены по SHA; manifest связывает source/input/ELF/NRO hashes и build IDs.

| NRO | SHA256 | ELF Build ID |
| --- | --- | --- |
| matrixgame-e610-quick-smoke.nro | `bb17b62418a825f0deaae8e7da904bf399d3ddf14631e209f02377ce5dda46e2` | `1424001dcbc6fec070785689f1740e7e014f870c` |
| matrixgame-e610-interactive.nro | `2af2e9771e97648c2a9da61cef75f2c35e03f163deb5f28193228043cef36076` | `a42086b2e6aeef4e15e2170425802415ac6c5ad2` |

SDK output: build/matrixgame/e610-raster-read-r31/1. Package command:
`python tools/matrixgame_e610_freeze.py package --build build/matrixgame/e610-raster-read-r31/1
--source-commit 4bdceb16e56f611de2d7514430cbf42c427cb328
--output dist/matrixgame-e610-switch-test-r31-raster-read.zip
--readme docs/matrixgame-e610-raster-read-r31-readme.ru.md
--rangers-root sdmc:/switch/space-rangers-hd-a-war-apart/game`.
Новому выводу всегда дать новое имя. [Полный evidence](matrixgame-e610-raster-read-r31-evidence.json).
Evidence SHA256: `cde956ebdb820f541293e7dec0c1f477100de03439653c8372170eebf37e2f6b`.

SOFTWARE PASS / HARDWARE PENDING. Разработка на этом шаге остановлена.
Quick → четыре настоящих map frames; затем Interactive → доступные курсор/
кнопки/меню/камера/звук, штатный выход Minus+Plus. Полная инструкция в ZIP,
логи отдельно в /switch/matrixgame-diag/logs/<run-id>.
Повторно копировать ресурсы не требуется. Нового аппаратного исполнения нет;
последние реальные timing относятся к r9, вклад r31 в Draw/Init/FPS неизвестен.
Physical GPU/HID/audio и полнота Init/map frames ещё не приняты.
Unified Campaign/Gov, естественная победа и возврат Galaxy standalone entry не охвачены.
