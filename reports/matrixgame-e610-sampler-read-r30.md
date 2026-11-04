# MatrixGame E610 r30 — прямое чтение sampler word

В getter NativeRenderState::Sampler после lookup основного ID выполнялось
state.Words()[index]. Words строил временный массив из всех восьми 32-bit полей.
В SDK ARM64 r29 это реальные загрузки и запись 32 bytes на стек, затем чтение
одного элемента. R30 возвращает нужное поле через switch по SamplerRegister.
Extra ID по-прежнему используют bounded SamplerLookup и sampler_extra.at.

Проверка samplers.at(slot) остаётся первой. Unknown IDs сохраняют исходную
std::out_of_range и её текст. Getter не валидировал значения раньше и не
валидирует теперь: invalid enum/anisotropy bits остаются наблюдаемыми.
LodBias возвращается std::bit_cast<uint32_t>, без арифметики, canonicalization
или FP conversion. Signed zero/NaN payload/sNaN flags должны сохраниться.
Чтения остальных plain fields не имеют callbacks, validation или side effects.
Публичные headers, setter, настоящий Apply/Validate и всё остальное CPP unchanged.
Новых mutable fields, API, allocations, кеша или изменений ownership нет.
Reference: r29 source 8ca57b1fc2909d7cc0c415f91d3aea9c31256ece.

Путь используется DekoDevice::ReadSamplerState и NativeStoredStateBackend getter
при capture/restore состояния. Частота и доля времени этих вызовов в реальном
кадре Switch пока неизвестны. Новый код не объявляется ускорением whole Draw/Init/FPS.

## CPU проверка

Используется неизменённый corpus r28/r29: 66311 cases, из них 40800 sampler
operations и 25511 scalar cases/21472 setters. Все 8 слотов + invalid slots,
regular/extra/unknown/boundary IDs, разные values, corrupt anisotropy/address/sNaN,
getter/setter, FP flags в четырёх rounding modes, full state после success/failure.
Полный stdout сравнивается frozen/current отдельно на каждой платформе.

Benchmark — неизменённый r28 workload: 3407872 настоящих setters,
3407872 getters, 100000 actual PrepareDraw на отдельном unchanged control,
одинаковый warmup. Production CPP at O2/UNDEBUG, no sanitizer/LTO/fast-math,
ABBAAB без параллельных CPU jobs. Это throughput sampler CPU operations.
Коэффициенты предыдущих benchmark не перемножаются.

Exploratory /1: getter median 6610 → 5844 мкс (−11,59%),
setter 12007 → 11990 (−0,14%), PrepareDraw 56202 → 56491 (+0,51%).
Checksum 15814044235430741187 одинаков во всех runs.

Большие Windows-бои и полные regression suites не повторяются.
Ресурсы/saves/upstream/frozen/основная NRO и предыдущие пакеты сохраняются.
HARDWARE PENDING: physical GPU/HID/audio/Init/map frames/FPS.
Standalone entry не охватывает unified Campaign/Gov, natural victory/Galaxy return.
[Два ручных запуска](../docs/matrixgame-e610-sampler-read-r30-readme.ru.md).


## Эквивалентность

Host ASan/UBSan/LSan, ARM64/QEMU UBSan, Windows O2 /fp:strict /1 — PASS.
66311 cases / 49296 expected rejections. Неизменённые tests/assertions r28/r29;
полный stdout frozen/current совпал попарно. Digest host 8810794293108803072,
ARM64 2600753383352677024, Windows 8008852560463349944.
Межплатформенные FP flags/messages не являются acceptance. Assertions active,
ARM64 UBSan/vptr сохранён с прежним type_info symbol workaround.

Первая попытка Windows runner из ограниченного процесса не смогла создать
build/.../windows (WinError5, до compilation). После разрешённого локального
доступа новый /1 прошёл; код и tests не менялись. Это не semantic failure.

ABBAAB /2: getter 6669 → 5799 (−13,05%), setter 12145 → 12949 (+6,62%),
PrepareDraw 56262 → 57678 (+2,52%). Candidate PrepareDraw outlier 69747;
последний setter candidate 14578 при самом быстром getter 5725.
Неизменённый setter вышел за контрольный предел 5%, поэтому результат /2
сам по себе не принят; выполнен один дополнительный замер /3 тем же кодом.
Все /1,/2,/3 включаются в evidence и суммарную статистику; выбросы не удаляются.


## Итоговый CPU timing

Дополнительный ABBAAB /3 тем же кодом:

| CPU metric, мкс | frozen r29 runs | r30 runs | median до → после |
| --- | --- | --- | --- |
| Getter | 6861 / 6850 / 6644 | 6079 / 5957 / 5901 | 6850 → 5957 (−13,04%) |
| Setter control | 12395 / 12211 / 11973 | 11843 / 12290 / 11996 | 12211 → 11996 (−1,76%) |
| PrepareDraw control | 56899 / 52398 / 55898 | 57873 / 58006 / 56453 | 55898 → 57873 (+3,53%) |

Ни один результат не исключён. По всем 18 runs (/1,/2,/3, девять на вариант):

| CPU metric, мкс | frozen median | r30 median | изменение |
| --- | --- | --- | --- |
| Getter | 6644 | 5901 | −11,18% |
| Setter control | 12145 | 12153 | +0,07% |
| PrepareDraw control | 56202 | 57188 | +1,75% |

Во всех трёх сериях getter быстрее; даже максимальный candidate getter 6079
меньше минимального frozen 6581. Контрольный setter в сумме почти неизменён;
PrepareDraw имеет небольшой положительный сдвиг и перекрывающиеся диапазоны.
Причина его колебаний без аппаратного профиля не устанавливается. Предел 5%
не ослаблен: /3 и общий набор укладываются. Изменён только getter, никакого
выигрыша PrepareDraw или целого кадра не заявляется. Дополнительные timing runs
после /3 для подбора цифры не выполнялись. Все checksums одинаковы.

## Воспроизведение

CPU: `python tools/matrixgame_sampler_read_checks.py host` (также arm64/windows),
затем `python tools/matrixgame_sampler_read_checks.py benchmark` без parallel jobs.
Runner сохраняет frozen CPP, команды/stdout/source hashes в новый numbered каталог.
SDK: `python tools/matrixgame_current_diagnostic_sdk.py --output-root
build/matrixgame/e610-sampler-read-r30`. Новому выводу всегда дать новое имя.
Локальный delivery gate: build/matrixgame/e610-sampler-read-r30-checks/verify_delivery.py
с source commit как аргументом.


## SDK/package и direct-read proof

Source commit: `84615a9f00bc7593f8db6fa53a211d598ce63145`.
Build ID: `84615a9f00bc-44f41e91ba90233a595e`.
Обе отдельные стандартные devkitA64/libnx/deko3d compile/link — PASS,
strong undefined=0. 321 configured TUs: O2/UNDEBUG, exceptions, no fast-math.
Настоящие 23 debug deko3dd object members / 0 release, mixed=false.
Prebuilt Rangers archive unchanged, не выдаётся за новую optimized сборку.
NRO structure/assets, встроенные fonts/shaders, NACP titles и ELF→NRO IDs проверены.
Generated original files identical r29 (три исторических delta vs E601).
721 own source hashes проверены. Production diff vs r29 — только getter,
дополнительно r30 titles/CPU runner; прочие build inputs unchanged.
346 прежних защищённых артефактов SHA unchanged; game/frozen/main NRO gate PASS.

В обеих actual SDK ELF getter не содержит SIMD/FP load/store Words или
FP conversion/arithmetic. Обычный frame уменьшен с 64 до 32 bytes; оставшийся
frame обслуживает вызовы исключений, а не временный массив значений.
Bounded static instruction-path audit проследил 64 regular ID/slot пути:
после slot bound и ID dispatch ровно один integer ldr правильного поля,
со stride 32 и offsets 0..28 из исходного layout. LodBias читается ldr w0
по offset24 — без FP conversion. Это статический аудит фактического ARM64-кода,
не исполнение NRO/QEMU и не аппаратная приёмка. Disassembly обеих ELF в evidence.
RenderLookup 630 bytes, LogicalRules 103 bytes, SamplerLookup 28 bytes и
Matrix r25 machine bytes exact r29.

Две технические ошибки verifier сохранены отдельно: parser сначала выбрал
минимальный x0-relative offset из cold exception path вместо базы Words;
затем regex горизонтальных пробелов захватывал следующую инструкцию после ret.
Исправлены только parsing/address tracing; production/test inputs не менялись,
acceptance assertions не ослаблялись. Все 64 пути после исправления прошли.

ZIP: `dist/matrixgame-e610-switch-test-r30-sampler-read.zip` (10160944 bytes).
SHA256: `3ed7ad76263f13f2a2aa1694f5650d680a06644569b6a6979a8b6237c4edf200`.
Ровно 6 файлов: две NRO, README.ru.md, manifest.json, SHA256SUMS.txt, build-info.json.
Игровых ресурсов и сохранений нет. ZIP и распакованный пакет повторно
проверены по SHA; manifest связывает source/input/ELF/NRO hashes и build IDs.

| NRO | SHA256 | ELF Build ID |
| --- | --- | --- |
| matrixgame-e610-quick-smoke.nro | `324dda5cf163637f57f2e9cdeab3bd536f3fcf11e2afccab8cc59683f699c16f` | `1f4ce68370fe834f45800000a332574eeecd1f6b` |
| matrixgame-e610-interactive.nro | `08fb816909a437b912975ee556e13a49ec5c0475d8d9c8d4256f570c9b5f2d8b` | `d4627be1f1436d8880e985ddf5e73ff16b792686` |

SDK output: build/matrixgame/e610-sampler-read-r30/1. Package command:
`python tools/matrixgame_e610_freeze.py package --build build/matrixgame/e610-sampler-read-r30/1
--source-commit 84615a9f00bc7593f8db6fa53a211d598ce63145
--output dist/matrixgame-e610-switch-test-r30-sampler-read.zip
--readme docs/matrixgame-e610-sampler-read-r30-readme.ru.md
--rangers-root sdmc:/switch/space-rangers-hd-a-war-apart/game`.
Новому выводу всегда дать новое имя. [Полный evidence](matrixgame-e610-sampler-read-r30-evidence.json).
Evidence SHA256: `ca9dd97e810ceafc3db7d74b2b9a05f2176d12f9203e27d3286d3888b402a810`.

SOFTWARE PASS / HARDWARE PENDING. Разработка на этом шаге остановлена.
Quick → четыре настоящих map frames; затем Interactive → доступные курсор/
кнопки/меню/камера/звук, штатный выход Minus+Plus. Полная инструкция в ZIP,
логи отдельно в /switch/matrixgame-diag/logs/<run-id>.
Повторно копировать ресурсы не требуется. Нового аппаратного исполнения нет;
последние реальные timing относятся к r9, вклад r30 в Draw/Init/FPS неизвестен.
Physical GPU/HID/audio и полнота Init/map frames ещё не приняты.
Unified Campaign/Gov, естественная победа и возврат Galaxy standalone entry не охвачены.
