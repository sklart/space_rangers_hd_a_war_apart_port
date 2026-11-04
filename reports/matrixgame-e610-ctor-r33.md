# MatrixGame E610 r33 — hinted insertion начального render state

NativeRenderState constructor вставлял 103 уже отсортированных NativeRenderWords
через обычный emplace с поиском от корня дерева. R33 использует
emplace_hint(registers.end(),id,initial). Никакие значения или узлы не пропускаются.
Список unique/sorted уже проверяется constexpr при построении metadata.
End hint сокращает поиск позиции; стандартная map сохраняет собственную
проверку корректности hint. Контейнер/allocator/layout/ключи/порядок значений
не меняются, кеш или пул узлов не добавлен.

Это реальный путь NativeStateStore/TargetSet defaults, а также временных
NativeRenderState overlays в DekoDevice::Impl::DrawCursor и font-session draw.
Overlay state создаётся при курсоре/выводе шрифта. Частота этих операций и
доля стоимости в настоящем Switch кадре не измерены. Constructor benchmark
не выдаётся за whole Draw/Init/FPS. GPU resource ownership/cleanup не меняются.
Reference: r32 source 3d4c8c8a2dbffb60cd0023df9f988d1b206d3324.

## Не включённые combiner-прототипы

Исследованы короткое замыкание diffuse scan и вариант пропуска этого scan
при включённой alpha первой стадии. Первый не показал общего выигрыша:
14182 → 14224 мкс на короткой смеси 12 сцен; длинный diffuse-first выигрывал,
но некоторые no-diffuse случаи были медленнее, присутствовали выбросы.
Во втором число повторов на сцену увеличено 8×: aggregate 118751 → 120275 мкс,
надёжного target gain нет. Оба NOT SELECTED / NOT SHIPPED. Их source/runner/
benchmark и результаты сохранены в build/matrixgame/e610-combiner-r33-checks.
Нельзя сравнивать абсолютное время разных workloads или выдавать эти trials
за прошедшие semantic gates. material_combiner.cpp exact r32.

## Эквивалентность и cleanup

Сохранён весь corpus r32 (86791 cases). Добавлена проверка настоящего
конструктора/деструктора с fault injection global new в отдельном test TU.
Обычный успешный constructor подтверждает все 103 начальных keys/values,
затем new разрешается на N шагов и отклоняет следующий. Проверены каждый
allocation failure, последний узел и два success budgets. После каждого
успеха/исключения live allocations равны нулю; ctor не публикует partial state.
Инжектор не связан с NRO и не используется в benchmark.

Host ASan/UBSan/LSan и ARM64/QEMU UBSan: 86896 cases / 50832 прежних rejections,
105 allocation cases, 103 injected failures/constructor allocations.
Windows O2 /fp:strict: 86897 cases, 106 allocation cases, 104 injected failures/
allocations из-за служебного head node MSVC map. Prefix r32 checksums сохранены;
полный stdout frozen/current одинаков попарно на каждой платформе.
Digest host 15109168818705714315, ARM64 17451480286970648043,
Windows 7817331884564294482. Assertions active, ARM64 UBSan/vptr с прежним
local type_info workaround. Межплатформенные digest/messages не сравниваются.

Frozen вариант — полный Git archive port/matrixgame r32 с исходными headers;
candidate — текущие источники. Сохраняются byte hashes compiled snapshots
и полная normalized-text сверка Git blob (archive применяет EOL attributes).

## CPU workload

32768 настоящих constructor/destructor pairs с обычным C++ allocator,
2048-pair warmup. Параллельно не запускаются другие CPU jobs. Контрольные
setters/getters/raster getters/PrepareDraw/StencilProbe — прежний workload r32.
O2/UNDEBUG, no sanitizer/LTO/fast-math, ABBAAB. Checksum
16076417499527976475 одинаков в exploratory /1.

/1: ctor median 102936 → 94069 мкс (−8,61%), setter 25274 → 25491 (+0,86%),
PrepareDraw 58299 → 56861 (−2,47%). Getter control 43802 → 50596 (+15,51%)
шумен: candidate 51970/50596/44097; этот контроль требует итогового замера
после semantic gates. Число allocation calls не сокращается.

Большие Windows-бои и полные regression suites не повторяются.
Ресурсы/saves/upstream/frozen/основная NRO и предыдущие пакеты сохраняются.
HARDWARE PENDING: physical GPU/HID/audio/Init/map frames/FPS.
Standalone entry не охватывает unified Campaign/Gov, natural victory/Galaxy return.
[Два ручных запуска](../docs/matrixgame-e610-ctor-r33-readme.ru.md).


## Итоговый CPU timing

ABBAAB /2 после semantic gates, checksum 16076417499527976475:

| CPU metric, мкс | frozen r32 runs | r33 runs | median до → после |
| --- | --- | --- | --- |
| Ctor+dtor | 105395 / 105639 / 105247 | 93642 / 93024 / 95753 | 105395 → 93642 (−11,15%) |
| Setter control | 26160 / 25074 / 25209 | 28607 / 25077 / 25132 | 25209 → 25132 (−0,31%) |
| Mixed getter control | 46348 / 46975 / 45507 | 45684 / 46068 / 46231 | 46348 → 46068 (−0,60%) |
| Raster getter control | 8073 / 7963 / 8103 | 8115 / 7928 / 7933 | 8073 → 7933 (−1,73%) |
| PrepareDraw control | 57869 / 61173 / 59382 | 58869 / 63712 / 57726 | 59382 → 58869 (−0,86%) |
| StencilProbe control | 51415 / 14012 / 13601 | 14429 / 13607 / 15972 | 14012 → 14429 (+2,98%) |

Выигрыш target повторился в /1 (~8,6%) и /2 (~11,2%). В /2 диапазоны ctor
не пересекаются. Getter контроль из /1 не повторил замедление; в /2 все
контрольные medians в пределах 5%. Сильный frozen StencilProbe outlier 51415
не исключён. Последующих timing runs для подбора более выгодной цифры нет.

По всем 12 runs (/1,/2, шесть на вариант): ctor 105321 → 93855,5 мкс (−10,89%).
Setter 25241,5 → 25311,5 (+0,28%), mixed getter 45927,5 → 46149,5 (+0,48%),
raster getter 8001,5 → 7968 (−0,42%), PrepareDraw 58840,5 → 57632 (−2,05%),
StencilProbe 13716,5 → 14444 (+5,30%). Небольшой aggregate сдвиг последнего
контроля фиксируется, хотя его source unchanged; отделить layout/частоту CPU/
внешнюю нагрузку короткими host сериями не удалось. Target и /2 убедительнее
первого шумного getter замера. Отдельного ускорения Draw/validation не заявляется.
Не суммировать и не перемножать gains разных CPU workloads.

## Воспроизведение

CPU: `python tools/matrixgame_state_ctor_checks.py host` (также arm64/windows),
затем `python tools/matrixgame_state_ctor_checks.py benchmark` без parallel jobs.
Runner создаёт новый numbered каталог и полный frozen port Git snapshot,
сохраняет commands/stdout/source/header hashes. Benchmark не линкует allocator
fault TU; hooks существуют только в semantic tests, не в production.
SDK: `python tools/matrixgame_current_diagnostic_sdk.py --output-root
build/matrixgame/e610-ctor-r33`. Новому выводу всегда дать новое имя.
Локальный delivery gate: build/matrixgame/e610-ctor-r33-checks/verify_delivery.py
с source commit как аргументом.


## SDK/package и end-hint proof

Source commit: `c8f793daf5921619785064fe30e391c94565757c`.
Build ID: `c8f793daf592-4c5e73770f79446c22fc`.
Обе отдельные стандартные devkitA64/libnx/deko3d compile/link — PASS,
strong undefined=0. 321 configured TUs: O2/UNDEBUG, exceptions, no fast-math.
Настоящие 23 debug deko3dd object members / 0 release, mixed=false.
Prebuilt Rangers archives unchanged, не выдаются за новую optimized сборку.
NRO structure/assets, встроенные fonts/shaders, NACP titles и ELF→NRO IDs проверены.
Generated original files identical r32 (три исторических delta vs E601).
724 own source hashes проверены. Production diff — одна вставка emplace_hint,
дополнительно r33 titles/CPU runner. Combiner CPP, stencil header и прочие
build inputs exact r32. 367 прежних защищённых артефактов SHA unchanged;
game/frozen/main NRO gate PASS. Старые packages/evidence не перезаписывались.

Actual ARM64 constructor в обеих ELF: 768 → 508 bytes. Защитный общий
_M_get_insert_unique_pos fallback сохранён; это не замена map обходом проверок.
End-hint fast block читает rightmost key, сравнивает новый key и при строго
большем key переходит прямо к настоящему _Rb_tree_insert_and_rebalance.
Первый empty insert может использовать общий fallback, следующие 102 unique
sorted keys соответствуют fast path. Сохраняются operator new (40-byte node),
удаление неиспользованного узла, _M_erase и _Unwind_Resume cleanup.
Из actual NativeRenderWords table (103×24 bytes) извлечены все ID/initial pairs:
совпадают с frozen metadata, unique и строго возрастают. Никаких initial words
не выкинуто. Полные constructor disassembly и fast block в evidence.

RenderLookup 630 bytes, LogicalRules 103 bytes, SamplerLookup 28 bytes и
Matrix r25 machine bytes exact r32. Allocation Probe отсутствует в ELF symbols,
его test TU и benchmark не входят в SDK compile_commands; hooks не поставляются.
Это compile/link/static proof, не исполнение NRO и не аппаратная приёмка.

Первая упаковка в ограниченном процессе получила WinError5 на mkdir новой
папки dist (до записи файлов). Повтор с разрешённым локальным доступом успешно
создал новый пакет; существующие пакеты не менялись.

ZIP: `dist/matrixgame-e610-switch-test-r33-ctor.zip` (10160553 bytes).
SHA256: `391950ce35507939a75c0ec2c7efbb8457a20e8cf3a9f619c92cc6400f3b8221`.
Ровно 6 файлов: две NRO, README.ru.md, manifest.json, SHA256SUMS.txt, build-info.json.
Игровых ресурсов и сохранений нет. ZIP и распакованный пакет повторно
проверены по SHA; manifest связывает source/input/ELF/NRO hashes и build IDs.

| NRO | SHA256 | ELF Build ID |
| --- | --- | --- |
| matrixgame-e610-quick-smoke.nro | `b772b9bbd48696e9997f3ab79d136527418bc9ae06ba2fc1f5f309d185f37b80` | `705f2a18824e2ba431e6aaf3a83febfe53d3ce68` |
| matrixgame-e610-interactive.nro | `ac516a1221fe00c50c7544f0cba4d9ab08505c6ac9240169cee1ce1793dc3ddc` | `2e522711b2ec4476afcd96e093ad5ecff7cd83f0` |

SDK output: build/matrixgame/e610-ctor-r33/1. Package command:
`python tools/matrixgame_e610_freeze.py package --build build/matrixgame/e610-ctor-r33/1
--source-commit c8f793daf5921619785064fe30e391c94565757c
--output dist/matrixgame-e610-switch-test-r33-ctor.zip
--readme docs/matrixgame-e610-ctor-r33-readme.ru.md
--rangers-root sdmc:/switch/space-rangers-hd-a-war-apart/game`.
Новому выводу всегда дать новое имя. [Полный evidence](matrixgame-e610-ctor-r33-evidence.json).
Evidence SHA256: `0bbdc94b4c5283e537bddd0bc5dd46386823e3e85a800a2f07207cee7dcd3904`.

SOFTWARE PASS / HARDWARE PENDING. Разработка на этом шаге остановлена.
Quick → четыре настоящих map frames; затем Interactive → доступные курсор/
кнопки/меню/камера/звук, штатный выход Minus+Plus. Полная инструкция в ZIP,
логи отдельно в /switch/matrixgame-diag/logs/<run-id>.
Повторно копировать ресурсы не требуется. Нового аппаратного исполнения нет;
последние реальные timing относятся к r9, вклад r33 в Draw/Init/FPS неизвестен.
Physical GPU/HID/audio и полнота Init/map frames ещё не приняты.
Unified Campaign/Gov, естественная победа и возврат Galaxy standalone entry не охвачены.
