# MatrixGame E610 r12 — render state metadata lookup

После r11 оптимизации SDK allocations следующим измеренным CPU участком
выбран production NativeRenderState::ValidateDraw/Render. Аппаратных результатов
r11 нет: это host CPU bottleneck, не новый hardware профиль или итоговый FPS.
Frozen reference — native_device_state.cpp из source 203f563d6453b9a311af432a4e14428ad2f86ce7.

## Причина и изменение

ValidateDraw читает 103 NativeRenderWords. Старый Render getter для каждого
слова снова линейно искал metadata, затем vertex/raster принадлежность. Цикл
валидации дополнительно повторял string comparisons для определения правила.
Это лишние повторные поиски immutable metadata на каждом draw.

Lookup теперь constexpr: ordinal metadata index, raster index, vertex flag по
serialized register ID. Duplicated/missing metadata выявляется на compile time.
Unknown IDs по-прежнему дают тот же out_of_range. DrawRules также constexpr;
условия Zero/One/Fill/FloatZero/SeparateAlpha те же. Каждый из 103 getter calls
остаётся в исходном порядке — даже inactive missing words должны продолжать
давать исходную ошибку. PixelFog/Stencil/combiner/sampler/point/line/multisample
checks выполняются как прежде. Ни результат валидации, ни изменяемое состояние
не кешируются. COW/snapshot/map ownership и logical values не меняются.

Для следующего аппаратного профиля добавлен отдельный state_validation timer
вокруг ValidateDraw. Он вложен в draw_cpu; totals не складывать. Input/output
bytes, native vertex decoder, shaders, draw order, allocation flags, completion
owners, physical quota и таймауты не меняются. R10 transition fix и r11 reuse
сохранены, upstream/frozen/resources/saves/основная NRO не изменяются.

## Измерение

Тот же benchmark production ValidateDraw+vertex.Prepare при O2, без LTO и
fast-math; последовательный frozen/candidate/candidate/frozen/frozen/candidate,
три замера каждой версии. Host median 20000 checks: 103656 → 23313 мкс,
ускорение 4.45x конкретного участка. Decoder control 200000 vertices:
5043 → 5005 мкс (не оптимизировался). Checksum 14249933621601823491 и четыре
rejection guards совпали во всех шести запусках. Это synthetic valid CPU
workload, не бой, не GPU emulator, не обещание такого же роста FPS.

## Семантический oracle

Один probe скомпилирован отдельно с полным frozen CPP и candidate CPP при тех
же headers/layouts (проверены по Git). 4039 случаев: getters IDs 0..511,
все 103 слова/8 вариантов value, setters/getters/ValidateDraw, удалённые
inactive map words, active/inactive samplers, point/line/MSAA/quality modes.
В обоих вариантах 1768 rejections. Полный outcome digest (включая категории
и what()) совпал попарно: host/ARM64 15117106717077238944; Windows
18123531672501331632. Между платформами std::exception messages отличаются;
их hashes не смешиваются. Frozen reference не редактировался.
Host ASan/UBSan/LSan O2, ARM64/QEMU UBSan O2, Windows /O2 — PASS. Known local
ARM64 type_info workaround сохраняет UBSan/vptr. Source/function inputs и
runtime commands записаны в build/matrixgame/e610-state-r12-checks/*/1/.

Дополнительные короткие state/COW/OOM/restore/frame/ownership/data tests
проверяются отдельно; SDK compile/link и delivery gates приводятся ниже.
Большие боевые сценарии/общие регрессии не перезапускаются.
[Инструкция](../docs/matrixgame-e610-state-r12-readme.ru.md).

## Границы

R12 HARDWARE PENDING. Полный Init→transition→четыре map frames и GPU/HID/FPS
нового пути ещё не приняты. Campaign/Gov/естественная победа/возврат Galaxy не
вызываются standalone entry. Host acceleration не заменяет Switch measurement.
