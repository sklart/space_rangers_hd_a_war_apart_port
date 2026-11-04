# MatrixGame E610 r35 — чтение известных регистров в PrepareDraw

PrepareDraw уже получает RenderLookup[id], но для vertex/cull/shade/factor
повторно входил в публичный Render(id), снова выполнявший Lookup и dispatch.
Теперь локальный read вызывает тот же vertex.Render, читает cull/shade и
выполняет тот же PackQuantizedArgb(combiner.State().factor) напрямую.
Production diff — четыре строки вместо одного повторного dispatch.

Публичный Render, setter, raw/presence view, raster mapping и logical rules
не изменены. Все 103 слова читаются в прежнем порядке; inactive missing words
по-прежнему вызывают прежние map::at ошибки. Ни одной обязательной операции
не пропущено. Фактор всё ещё квантуется прежней функцией, а vertex getter
сохраняет validation/quantization. Нет COW/ownership/ABI/fields/cache изменений.
Read callback private/local и вызывается только с ID из прежнего metadata,
поэтому удалённый второй Lookup не мог отклонить дополнительный ID.
Reference source r34: da540f87431e338714cd05957405556c1017be3a.

Это следующий найденный CPU расход в пути подготовки draw; без нового
аппаратного профиля нельзя назвать его крупнейшим оставшимся расходом кадра.

## CPU проверки

Frozen — полный Git archive port/matrixgame r34; сборка frozen/current
использует соответствующие sources и headers. Raw SHA compiled archive files
и Git blob SHA сохранены отдельно; тексты полностью совпадают после нормализации
CRLF/LF. Candidate source hashes сверены с SDK metadata.

Сохранён прежний корпус: setters/getters, corrupt raw words/fields, unknown IDs,
missing inactive words, все 103 render words, 8 sampler slots, stencil, ranges,
allocation fault injection на каждом ctor node и zero live blocks после bad_alloc.
Новые 1024 cases: 256 сочетаний packed factor/global ambient, vertex controls,
material sources, cull/shade и fog × четыре режима округления. Сравниваются
полные fog/combiner output bytes, все stencil fields, исключения и FPU flags.
Stencil padding не хешируется. После теста возвращается исходный rounding mode.

| Платформа | Cases | Rejections | Digest |
| --- | ---: | ---: | ---: |
| host ASan/UBSan/LSan | 88186 | 50832 | 5496786911076415252 |
| ARM64/QEMU UBSan | 88186 | 50832 | 9072737697019856308 |
| Windows O2 /fp:strict | 88187 | 50832 | 3149951305916031855 |

Везде PASS, полные stdout frozen/current совпадают попарно. Разница Windows
cases — прежний MSVC map head node allocation. Старые corpus digests не изменены.
ARM64 local type_info version map сохраняет UBSan vptr. Большие Windows battles
и полные наборы регрессий не перезапускались.
Воспроизведение: `python tools/matrixgame_draw_read_checks.py host`
(также arm64, windows, benchmark); runner создаёт новый numbered output.

## Замеры

Benchmark использует прежний реальный PrepareDraw и прежние контрольные
операции. O2/UNDEBUG, no sanitizer/LTO/fast-math, ABBAAB. Подготовка и warmup
не входят в измеряемый интервал. Каждый вариант выполняет 100000 PrepareDraw.
Это cached host CPU, не Switch GPU/uncached память.

/1 сохранён целиком, но НЕ ПРИНЯТ для timing: несколько неизменённых controls
и target колебались в разы. /2 exploratory: PrepareDraw 58455 → 55179 мкс
(−5,61%); StencilProbe 13944 → 15379 (+10,29%) сохранён как шум/ограничение
отдельного замера, не скрыт. После semantic gates выполнен /3 без конкурирующих
наших CPU jobs. Использованы те же /2 executables: их SHA, исходные build-команды
и все compiled source hashes проверены. Изменение имени semantic test в runner
не меняло benchmark compilation inputs. Скрипт повторения и исходный runner
сохранены в ignored build; hashes внесены в evidence.

Итоговый /3, микросекунды, медиана трёх запусков:

| Проверка | Frozen r34 | R35 |
| --- | ---: | ---: |
| PrepareDraw | 56640 | 52972 |
| Setter | 23990 | 23386 |
| Mixed getter | 39395 | 39565 |
| Raster getter | 7611 | 7974 |
| StencilProbe | 12640 | 12626 |
| Ctor/dtor | 90006 | 92063 |
| Full coverage | 2639 | 2588 |
| Fragmented coverage | 34156 | 32407 |

PrepareDraw: −6.48%. Все контрольные медианы в пределах 5% либо быстрее.
Все 12 запусков /2+/3: 57017.0 → 53680.0 мкс (−5.85%).
Агрегированные controls также в пределах 5% либо быстрее. Checksum всех
18 запусков /1–/3 одинаков: 2654920981528516123. /1 не включён в accepted median.

## SDK/package и границы доказательств

Source commit: `7ab2c995772cc338b1ffc86c9d79f9dc4469a895`.
Build ID: `7ab2c995772c-98a8f650c722f2101bbb`.
SDK: `python tools/matrixgame_current_diagnostic_sdk.py --output-root
build/matrixgame/e610-draw-read-r35`; текущая сборка /1. Две стандартные
libnx/deko3d devkitA64 compile/link — PASS; strong undefined=0.
321 configured TUs: O2/UNDEBUG, exceptions, no fast-math. Link maps подтверждают
23 debug deko3dd object members / 0 release, mixed=false. Prebuilt Rangers
archives unchanged; новая оптимизация к ним не приписывается.
NRO structure/assets, fonts/shaders, NACP titles и ELF→NRO IDs проверены.
Generated originals exact r34; три прежних changes vs E601 сохранены.
726 own source hashes verified. Production delta только local read callback;
дополнительно r35 titles/runner. 381 прежний защищённый артефакт SHA unchanged;
game/frozen/main NRO gate PASS. Предыдущие пакеты/evidence не перезаписывались.

В обеих actual SDK ELF PrepareDraw: 4420 → 4336 bytes. Число статических
call sites общего NativeRenderState::Render(unsigned) const: 4 → 3.
Убран call внутри metadata traversal, оставшиеся три относятся к line/fill
проверкам вне callback и сохранены. В обходе известный vertex ID вызывает
реальный FixedVertexState::Render; scalar поля cull/shade читаются напрямую.
Все четыре FCVTAS из PackQuantizedArgb присутствуют в PrepareDraw, включая
validation-only чтение фактора с неиспользуемым результатом. Fog/combiner calls,
map iteration, исключения и _Unwind_Resume cleanup сохранены. Полные disassembly
в evidence. RenderLookup 630 bytes, LogicalRules 103, SamplerLookup 28,
Matrix r25 и Resource::Initialized r34 machine bytes exact r34.
Allocator fault hooks и test/benchmark TUs не входят в SDK ELF/compile_commands.
Это static/compile/link proof, а не исполнение на Switch.

ZIP: `dist\matrixgame-e610-switch-test-r35-draw-read.zip` (10160786 bytes).
SHA256: `2eed66ec5d84848d699e6a19de343f284c160f1c41370850c53341e2abe0b316`.
Ровно 6 файлов: две NRO, README.ru.md, manifest.json, SHA256SUMS.txt, build-info.json.
Ресурсов и сохранений в архиве нет; ZIP и распакованные файлы сверены по SHA.

| NRO | SHA256 | ELF Build ID |
| --- | --- | --- |
| matrixgame-e610-quick-smoke.nro | `46a1d71557a4343a03b490c109a3a31be655bb0debc6d38ec7617a1dcc2ca127` | `f9b5d17c0bb0dde207a0f4a81be9370cbd3d65f8` |
| matrixgame-e610-interactive.nro | `4d627b5d1b3bcaebb0642f40daeee54192fab2863ec00877b87d92b116988da0` | `8fe5311f679089646858c40b2c7a8af72ca3d89f` |

Package command: `python tools/matrixgame_e610_freeze.py package
--build build/matrixgame/e610-draw-read-r35/1
--source-commit 7ab2c995772cc338b1ffc86c9d79f9dc4469a895
--output dist/matrixgame-e610-switch-test-r35-draw-read.zip
--readme docs/matrixgame-e610-draw-read-r35-readme.ru.md
--rangers-root sdmc:/switch/space-rangers-hd-a-war-apart/game`.
Для повторения использовать новое имя вывода. Delivery gate:
`python build/matrixgame/e610-draw-read-r35-checks/verify_delivery.py
7ab2c995772cc338b1ffc86c9d79f9dc4469a895`.
[Полный evidence](matrixgame-e610-draw-read-r35-evidence.json).
Evidence SHA256: `0e9f332acbf40f1e622779e96cf1578956d4efbc5c0d7511203d0c7485888b31`.

SOFTWARE PASS / HARDWARE PENDING. Разработка на этом шаге остановлена.
Два ручных запуска: Quick Smoke до четырёх настоящих map frames, затем
Interactive до 120 секунд после Init; выход Minus+Plus. При раннем FAIL не
пытаться принудительно пропустить этап. NRO копировать отдельно в
/switch/matrixgame-e610-r35-test/. Ресурсы повторно копировать не требуется.
Логи: /switch/matrixgame-diag/logs/<run-id>.
[Полная инструкция](../docs/matrixgame-e610-draw-read-r35-readme.ru.md).
Последние реальные timings относятся к r9; вклад r35 в Draw/Init/FPS неизвестен.
Physical GPU/HID/audio, полнота Init/map frames ещё не приняты. Standalone entry
не охватывает unified Campaign/Gov, естественную победу и возврат в Galaxy.
Переноса файлов и аппаратного запуска не выполнялось.
