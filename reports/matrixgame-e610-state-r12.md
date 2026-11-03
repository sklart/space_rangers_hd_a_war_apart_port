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

## Проверенная доставка r12

Source commit: `2bba518c902cea71df7320277f4380a88ffa7996`; build ID `2bba518c902c-8a2b24fe3b49770a447b`.
ZIP: `dist\matrixgame-e610-switch-test-r12-state.zip` (10159963 байт).
ZIP SHA256: `8e7b2b75b02c5e0664824ade103d69be41a9c7c1af0f2561f471ffd79fd36aba`.

| NRO | SHA256 | ELF build ID |
| --- | --- | --- |
| matrixgame-e610-quick-smoke.nro | `804961b3f0a44e11606f1d45295af2711d080e373184c670a824c9e8f047445d` | `3b9c5ff118fe8f1e25b0beb6130fc28548f6e227` |
| matrixgame-e610-interactive.nro | `751566bbf8d30e14fc00af4837548c6e7b9ae3090c50f68cac69d3f9802612b8` | `c5d28e69f0fea8f87b3f344b5f41b4fc6bc312a8` |

Итог: **SOFTWARE PASS / R12 HARDWARE PENDING**. Обе devkitA64 compile/link,
strong undefined=0, NRO layout/font/shader/title/build ID gates PASS. Actual
compiler provenance: 321 configured C/C++ commands с -O2/-UNDEBUG, assertions/
exceptions сохранены, fast-math отсутствует. Настоящий deko3dd — 23 members,
0 deko3d, debug error path проверен nm. 701 own-source SHA совпал с metadata.
Все generated original файлы совпали с r11/r10, включая authored transition.
220 контрольных сумм прежних файлов/пакетов совпали. Main NRO, pinned upstream,
frozen, игровые ресурсы и saves сохранены.
[Полное evidence](matrixgame-e610-state-r12-evidence.json).

Frozen-equivalence/benchmark results: build/matrixgame/e610-state-r12-checks/
{windows,host,arm64,benchmark}/1/. Дополнительные восемь CPU cases прошли на
Windows O2, host ASan/UBSan/LSan O2 и ARM64/QEMU UBSan O2 в
build/matrixgame/e610-state-r12-regression/*/1/: state restores=9, capture OOM=2,
COW OOM=1, allocation-free restore, retained texture/buffer owners, expired
owner/frame guards; далее best-fit/upload bytes/packet/order/handoff/journal.
Новые software results не повышают r10/r11 transition path до hardware PASS.

Следующий ручной тест: две NRO из ZIP в `/switch/matrixgame-e610-r12-test/`,
в hbmenu MatrixGame r12 Quick Smoke → MatrixGame r12 Interactive. DATA/CFG
и saves остаются на месте. Выход Interactive — Minus+Plus, до 120 с после Init.
Передать два новых run-каталога; сравнивать profile.state_validation с draw_cpu
как вложенный timer и отдельно время Init, transition и настоящих map frames.
До этих наблюдений FPS/HID/GPU/звук нового r12 остаются HARDWARE PENDING;
Campaign/Gov/естественная победа/возврат Galaxy standalone entry не охвачены.
Дальнейшая разработка ожидает аппаратного профиля.
