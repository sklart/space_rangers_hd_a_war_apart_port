# MatrixGame E610 r32 — stencil validation без временных копий

MeshStencilState::Validate создавал initializer_list из двух StencilFace,
затем временные наборы четырёх uint32 для каждой грани. Этот путь выполняется
ReadStencilState при каждом PrepareDraw. R32 читает те же поля через const
reference и проверяет тот же диапазон 1..8, в прежнем порядке: clockwise
fail/depth_fail/pass/compare, затем counterclockwise. Исключение и сообщение
неизменны. Все восемь проверок сохранены, в том числе при disabled stencil и
выключенном two-sided; reference/read_mask/write_mask не получают новой проверки.

Чтения plain integer fields не имеют callbacks/FP effects. Прежние временные
копии не имели наблюдаемого поведения; новый код не мутирует state. Публичные
сигнатуры/layout, Restore/COW/ownership, SDK GPU pipeline и shader equations
не меняются. Нет новых allocations, кешей или полей. NativeRenderState.cpp
в точности возвращён к r31; scalar-ID прототипы не поставляются.
Reference: r31 source 4bdceb16e56f611de2d7514430cbf42c427cb328.

## Отклонённые кандидаты

Сначала исследован reverse-index LogicalRules в setter: actual ARM64 r31
содержит UMULL/ASR/MUL с reciprocal для &word-NativeRenderWords.data().
Вариант с flags210 и чтением word.id: setter median 23958 → 25941 мкс
(+8,28%) — REJECTED. Вариант с передачей validated ID после Lookup:
23292 → 23995 (+3,02%), выигрыша нет — REJECTED. Ни один не принят по
статическому сокращению инструкций в отсутствие CPU выигрыша.
Архивы исходников/runner/причин и benchmark/1,/2 сохранены в
build/matrixgame/e610-scalar-id-r32-checks. Это отдельный отклонённый эксперимент,
его цифры не смешиваются с новым stencil workload. Native CPP восстановлен
точными исходниками r31 с сохранением EOL; пользовательских dirty изменений
до начала не было. Ничего из прежних пакетов или evidence не удалялось.

## CPU проверка

Frozen вариант компилируется из отдельного полного Git archive port/matrixgame
r31, включая старый stencil header. Candidate компилируется из текущих исходников.
Так исключено сравнение двух вариантов с одним новым inline header.
Прежний corpus 84231 сохранён; добавлены 2560 прямых Probe cases:
восемь fields × 20 boundary/raw values × четыре enabled/two-sided patterns ×
четыре rounding modes. Состояние до/после сравнивается логически, без padding;
сохраняются FP flags и полный текст/тип исключения. Reference/masks имеют
нестандартные значения, подтверждая отсутствие новой validation.

Probe — отдельная noinline forwarding boundary, вызывающая настоящий
production Validate; не заменяет проверки и не содержит игровой имитации.
В benchmark меняются все восемь валидных полей и reference, чтобы optimizer
не удалил вызовы. 4194304 validation calls плюс реальные NativeRenderState
setter/getter/raster/PrepareDraw controls из r31. O2/UNDEBUG, no sanitizer/
LTO/fast-math, ABBAAB без parallel CPU jobs. Одинаковый warmup.

Stencil exploratory /1: validation median 40465 → 13516 мкс (−66,60%),
PrepareDraw 61350 → 58042 (−5,39%), setter 24310 → 24439 (+0,53%),
mixed getter 45353 → 47081 (+3,81%), raster getter 8219 → 8043 (−2,14%).
Checksum 9174337692634017307 одинаков. Итоговый замер после semantic gates.
Не умножать этот коэффициент на прежние gains; частота validation в настоящем
кадре Switch и влияние на whole Draw/Init/FPS ещё не измерены.

Большие Windows-бои и полные regression suites не повторяются.
Ресурсы/saves/upstream/frozen/основная NRO и предыдущие пакеты сохраняются.
HARDWARE PENDING: physical GPU/HID/audio/Init/map frames/FPS.
Standalone entry не охватывает unified Campaign/Gov, natural victory/Galaxy return.
[Два ручных запуска](../docs/matrixgame-e610-stencil-r32-readme.ru.md).


## Итоговые CPU результаты

Host ASan/UBSan/LSan, ARM64/QEMU UBSan и Windows O2 /fp:strict /1 — PASS.
86791 cases / 50832 expected rejections. Полный stdout frozen/current совпал
попарно. Digest host 5948733542470171200, ARM64 10170752009522531552,
Windows 14793325339733720824. Прежние 84231 cases сохранили свои checksums.
Межплатформенные FP flags/messages не являются acceptance. Assertions active,
ARM64 UBSan/vptr сохранён с прежним type_info symbol workaround.

Серия /2 подтвердила validation 41850 → 13527 и PrepareDraw 63457 → 58224,
но setter control 24793 → 27055 (+9,12%, candidate 28756/23937/27055).
Из-за выбросов выполнена одна дополнительная /3 тем же кодом. Результаты
/1,/2,/3 сохраняются без исключения samples и входят в общую статистику.

ABBAAB /3:

| CPU metric, мкс | frozen r31 runs | r32 runs | median до → после |
| --- | --- | --- | --- |
| Validate | 43488 / 40431 / 40680 | 13436 / 13654 / 13440 | 40680 → 13440 (−66,96%) |
| PrepareDraw | 64409 / 61675 / 64623 | 57015 / 59383 / 60124 | 64409 → 59383 (−7,80%) |
| Setter control | 24181 / 24130 / 24097 | 24667 / 24771 / 23842 | 24130 → 24667 (+2,23%) |
| Mixed getter control | 47507 / 46171 / 44658 | 50099 / 47587 / 45497 | 46171 → 47587 (+3,07%) |
| Raster getter control | 8340 / 10751 / 8323 | 8222 / 8437 / 8320 | 8340 → 8320 (−0,24%) |

По всем 18 runs, девять на вариант:

| CPU metric, мкс | frozen median | r32 median | изменение |
| --- | --- | --- | --- |
| Validate | 40680 | 13525 | −66,75%, 3,01× |
| PrepareDraw | 61809 | 58224 | −5,80% |
| Setter control | 24310 | 24667 | +1,47% |
| Mixed getter control | 45639 | 47411 | +3,88% |
| Raster getter control | 8323 | 8248 | −0,90% |

Validation и actual PrepareDraw улучшились во всех трёх сериях. Положительный
сдвиг mixed getter не скрывается; source setter/getter unchanged, влияние
inline-header на inlining/layout и разброс не разложено по причинам.
/3 и общий набор укладываются в прежний контрольный предел 5%; /2 сам по себе
по setter его не прошёл. Цифры не означают Switch Draw/FPS gain.
Checksum 9174337692634017307 одинаков во всех runs. После /3 дополнительных
замеров для подбора цифры не выполнялось. Scalar-ID rejected trials отдельно.

## Воспроизведение

CPU: `python tools/matrixgame_stencil_validate_checks.py host` (также arm64/windows),
затем `python tools/matrixgame_stencil_validate_checks.py benchmark` без parallel jobs.
Runner создаёт новый numbered каталог и полный frozen port Git snapshot,
сохраняет commands/stdout/source/header hashes. Новые тесты не ослабляют старые.
SDK: `python tools/matrixgame_current_diagnostic_sdk.py --output-root
build/matrixgame/e610-stencil-r32`. Новому выводу всегда дать новое имя.
Локальный delivery gate: build/matrixgame/e610-stencil-r32-checks/verify_delivery.py
с source commit как аргументом.


## SDK/package и stencil proof

Source commit: `3d4c8c8a2dbffb60cd0023df9f988d1b206d3324`.
Build ID: `3d4c8c8a2dbf-5b84f3b72e2b93c33766`.
Обе отдельные стандартные devkitA64/libnx/deko3d compile/link — PASS,
strong undefined=0. 321 configured TUs: O2/UNDEBUG, exceptions, no fast-math.
Настоящие 23 debug deko3dd object members / 0 release, mixed=false.
Prebuilt Rangers archives unchanged, не выдаются за новую optimized сборку.
Все три frozen dependency archives проверены: MeshStencilState/ReadStencilState
symbols отсутствуют; старая weak stencil implementation не подменяет новый код.
NRO structure/assets, встроенные fonts/shaders, NACP titles и ELF→NRO IDs проверены.
Generated original files identical r31 (три исторических delta vs E601).
723 own source hashes проверены. Production diff — только Validate body в
stencil_state.hpp; дополнительно r32 titles/CPU runner. NativeRenderState.cpp
и остальные CPP/headers exact r31. Scalar-ID прототипов в NRO нет.
360 прежних защищённых артефактов SHA unchanged; game/frozen/main NRO gate PASS.

Validator встроен GCC16 в настоящий NativeRenderState::PrepareDraw.
По DWARF source lines выделены инструкции Validate в обеих actual SDK ELF.
Старый блок содержит временные STP W на стек; в новом их нет. Новый блок:
восемь SUB(value,1), два CMP #7 и шесть conditional CCMP #7 с unsigned
условием LS. Это все восемь прежних range checks. Clockwise success переходит
к counterclockwise, failure обеих граней ведёт в исходный invalid_argument.
Приложены полный PrepareDraw disassembly с debug lines и выделенные Validate
instructions. Это статическая проверка linked SDK-кода, не target execution.
RenderLookup 630 bytes, LogicalRules 103 bytes, SamplerLookup 28 bytes и
Matrix r25 machine bytes exact r31. Probe существует только в CPU tests.

Техническая ошибка verifier сохранена: Git archive экспортировал CRLF по
checkout attributes, а git show вернул LF blob. Вместо ошибочного равенства
byte hashes теперь отдельно проверяются recorded compiled bytes и полный
исходный текст после CRLF/LF normalization, с сохранением обоих SHA.
Никакой source/test input не менялся, assertions эквивалентности не ослаблены.

ZIP: `dist/matrixgame-e610-switch-test-r32-stencil.zip` (10161040 bytes).
SHA256: `01e60cdbae7b40e1c023f6d3ad13199dbbbc6d5021f1dd887a94be134316926c`.
Ровно 6 файлов: две NRO, README.ru.md, manifest.json, SHA256SUMS.txt, build-info.json.
Игровых ресурсов и сохранений нет. ZIP и распакованный пакет повторно
проверены по SHA; manifest связывает source/input/ELF/NRO hashes и build IDs.

| NRO | SHA256 | ELF Build ID |
| --- | --- | --- |
| matrixgame-e610-quick-smoke.nro | `338fb596ad3ddfc7e2926210d22963e097b9e33b78fec5d10c308510cdccb503` | `21660c27bd9d83bcc10bc246c32d9944c6687af9` |
| matrixgame-e610-interactive.nro | `270d7df7b81902d8f8a822f9a6f12c868d5f714f65f84cd5f260ad784e258749` | `dc2f1a6259a84ba692ed459ae2fa61bb5f640ec7` |

SDK output: build/matrixgame/e610-stencil-r32/1. Package command:
`python tools/matrixgame_e610_freeze.py package --build build/matrixgame/e610-stencil-r32/1
--source-commit 3d4c8c8a2dbffb60cd0023df9f988d1b206d3324
--output dist/matrixgame-e610-switch-test-r32-stencil.zip
--readme docs/matrixgame-e610-stencil-r32-readme.ru.md
--rangers-root sdmc:/switch/space-rangers-hd-a-war-apart/game`.
Новому выводу всегда дать новое имя. [Полный evidence](matrixgame-e610-stencil-r32-evidence.json).
Evidence SHA256: `2e6e659c699eedef38aa4e3813023a1653dcc5fbf5361bcaca234b2afbc878fb`.

SOFTWARE PASS / HARDWARE PENDING. Разработка на этом шаге остановлена.
Quick → четыре настоящих map frames; затем Interactive → доступные курсор/
кнопки/меню/камера/звук, штатный выход Minus+Plus. Полная инструкция в ZIP,
логи отдельно в /switch/matrixgame-diag/logs/<run-id>.
Повторно копировать ресурсы не требуется. Нового аппаратного исполнения нет;
последние реальные timing относятся к r9, вклад r32 в Draw/Init/FPS неизвестен.
Physical GPU/HID/audio и полнота Init/map frames ещё не приняты.
Unified Campaign/Gov, естественная победа и возврат Galaxy standalone entry не охвачены.
