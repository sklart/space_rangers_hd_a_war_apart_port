# MatrixGame E610 r28 — dispatch sampler-регистров

Устранены линейные поиски в NativeRenderState::Sampler setter/getter.
Прежний getter дополнительно собирал все восемь основных слов даже для extra
или unknown ID. Новый constexpr lookup содержит 14 строк по два байта:
1-based word/extra индексы из исходных SamplerRegisters и ExtraSamplerRegisters.
Нулевой ID и ID выше границы сохраняют прежние ошибки. Таблица неизменяема;
новых mutable fields, ABI, allocations, кэша значений или ownership изменений нет.

Проверка samplers.at(slot) остаётся первой. Setter extra сохраняет sRGB BOOL
проверку и sampler_extra.at запись. Основные/unknown ID проходят тот же
MeshSamplerState::Apply: unknown register отклоняется до Validate, остальные
значения проходят полную Validate и атомарную запись next. Getter сохраняет
Words()[index] для основных регистров. Words pure noexcept, без FP-арифметики,
validation/callback/allocations; пропущенные чтения для extra/unknown не влияют
на значения, ошибки или FP flags, включая sNaN в повреждённом lod_bias.
PrepareDraw extra checks, всё остальное CPP и публичный header exact r27.
Путь используется реальными DekoDevice::SetSamplerState/ReadSamplerState и
NativeStoredStateBackend getter/setter (capture/restore), а также Rangers canvas.
Оригинальные MatrixCursor/MatrixEffect/MatrixHint вызывают SetSamplerState.
Число этих вызовов и доля времени в настоящем кадре пока не измерены.
Reference: r27 source b9122aadf9b346f56fc77a019084c3da4510f683.

## CPU и эквивалентность

66311 cases / 49296 expected rejections — PASS: host ASan/UBSan/LSan,
ARM64/QEMU UBSan, Windows O2 /fp:strict. Из них 40800 sampler operations:
все 8 слотов + два invalid slots, regular/extra/unknown/boundary IDs,
разные значения, corrupt anisotropy/address/sNaN, getter/setter,
FP flags в четырёх rounding modes, полное состояние после success/failure.
Сохранён прежний scalar corpus 25511 cases/19120 rejections (21472 setters).
Полный stdout frozen/current совпал попарно на каждой платформе.
Digest host 8810794293108803072, ARM64 2600753383352677024,
Windows 8008852560463349944. Межплатформенные FP flags/messages не сравниваются.
Assertions active, ARM64 UBSan/vptr с прежним type_info symbol workaround.

Benchmark — реальные production overloads frozen/current CPP, O2/UNDEBUG,
no sanitizers/LTO/fast-math, ABBAAB, одинаковый warmup; 3407872 setters,
3407872 getters, 100000 настоящих PrepareDraw на отдельном unchanged control.
Checksum 15814044235430741187 одинаков во всех runs.

| CPU metric, мкс | frozen r27 runs | r28 runs | median до → после |
| --- | --- | --- | --- |
| Setter | 15956 / 15972 / 15945 | 15468 / 12565 / 12789 | 15956 → 12789 (−19,85%) |
| Getter | 14387 / 14197 / 14041 | 6618 / 6735 / 6802 | 14197 → 6735 (−52,56%) |
| PrepareDraw control | 60376 / 57861 / 60587 | 55997 / 59136 / 57660 | 60376 → 57660 (−4,50%) |

Exploratory /1: setter 16207 → 12155 (−25%), getter 13861 → 6604 (−52%).
Итоговый /3 выполнен после semantic gates; один setter candidate outlier.
Ускорение повторилось, дополнительных timing runs для выбора цифры не было.
Это throughput отдельных CPU operations. Их вклад в Draw/Init/FPS неизвестен,
не перемножается с предыдущими benchmark коэффициентами. Нового Switch замера нет.
Большие бои и полные regression suites не повторялись.

Ранние технические неуспехи сохранены в ignored checks: host/1, arm64/1,2,
benchmark/2 — Wsl/E_ACCESSDENIED до compilation; windows/1 — неверный namespace
нового test fixture, исправлен на srhd_awa::matrixgame::TextureAddress.
SDK/1 — denied чтение Git submodule metadata при configure. Повторные проверки
в отдельных каталогах прошли после разрешённого локального доступа; защитные
ограничения и проверки не обходились, tests/assertions не ослаблялись.

## Воспроизведение

CPU: `python tools/matrixgame_sampler_lookup_checks.py host` (также arm64/windows),
затем `python tools/matrixgame_sampler_lookup_checks.py benchmark` без parallel jobs.
Runner сохраняет frozen CPP, commands/stdout/source hashes в новый numbered каталог.
SDK: `python tools/matrixgame_current_diagnostic_sdk.py --output-root
build/matrixgame/e610-sampler-r28`. Прежние пакеты не перезаписывать.
Локальный delivery gate: build/matrixgame/e610-sampler-r28-checks/verify_delivery.py
с source commit как аргументом. [Два запуска](../docs/matrixgame-e610-sampler-r28-readme.ru.md).

HARDWARE PENDING. Последние реальные аппаратные измерения относятся к r9.
Standalone entry не охватывает unified Campaign/Gov, natural victory/Galaxy return.
Ресурсы/saves/upstream/frozen/основная NRO и предыдущие пакеты сохраняются.


## SDK, пакет и контроль исходников

Source commit: `7ca1c4db4d896bfb6bcc2ba348ff34fe54708a26`.
Build ID: `7ca1c4db4d89-2fa3d77cca73aae814f9`.
Обе отдельные стандартные devkitA64/libnx/deko3d compile/link — PASS,
strong undefined=0. 321 configured TUs: O2/UNDEBUG, exceptions, no fast-math.
Настоящие 23 debug deko3dd object members / 0 release, mixed=false.
Prebuilt Rangers archive unchanged; новой optimized Rangers сборкой не называется.
NRO structure/assets, встроенные fonts/shaders, NACP titles и ELF→NRO IDs проверены.
Generated original files identical r27 (три исторических delta vs E601).
719 own source hashes проверены; production diff ограничен private table и
двумя Sampler overloads. Прочие build inputs unchanged, кроме r28 titles/tool.
332 прежних защищённых артефакта SHA unchanged; game/frozen/main NRO gate PASS.

В обеих actual SDK ELF таблица SamplerLookup — 28 bytes; все 14 word/extra
индексов совпали с извлечёнными frozen enum/list. Setter/getter выполняют slot
bound до ID bound и table loads. Getter extra branch не строит Words.
GCC16 встроил неизменный Apply/Validate в setter: проверены три address masks,
finite float classifier, anisotropy bound и сохранённые exception paths.
Изначальная гипотеза verifier об отдельном вызове Apply отвергнута; неудача
сохранена отдельно, source/tests при этом не менялись. Actual disassembly в evidence.
RenderLookup 630 bytes, LogicalRules 103 bytes и Matrix r25 machine bytes exact r27.

ZIP: `dist/matrixgame-e610-switch-test-r28-sampler.zip` (10160802 bytes).
SHA256: `a57fb2fa6528885e174b31cf8ec9653049ca6de4b1730a137e78e696692b4472`.
Ровно 6 файлов: две NRO, README.ru.md, manifest.json, SHA256SUMS.txt, build-info.json.
Ресурсов и сохранений нет. Содержимое ZIP и распакованного пакета повторно
проверено по SHA; manifest связывает source/input/ELF/NRO hashes и build IDs.

| NRO | SHA256 | ELF Build ID |
| --- | --- | --- |
| matrixgame-e610-quick-smoke.nro | `1f1a1d43449615e4e5263b6abcac949a30d3c1b5f95d8e0351379eef502c501f` | `a543bd2efabc7e872d826f792833856b05f6612b` |
| matrixgame-e610-interactive.nro | `3e5709f9b9159020d0d7b69a8f41a0866242c8eca9e30512c9665648311c0d3e` | `b960e5f690ee78954dcbcfdbcf61fa1688e13527` |

SDK output: build/matrixgame/e610-sampler-r28/2. Package command:
`python tools/matrixgame_e610_freeze.py package --build build/matrixgame/e610-sampler-r28/2
--source-commit 7ca1c4db4d896bfb6bcc2ba348ff34fe54708a26
--output dist/matrixgame-e610-switch-test-r28-sampler.zip
--readme docs/matrixgame-e610-sampler-r28-readme.ru.md
--rangers-root sdmc:/switch/space-rangers-hd-a-war-apart/game`.
Новому выводу всегда дать новое имя. [Полный evidence](matrixgame-e610-sampler-r28-evidence.json).
Evidence SHA256: `20d89215398a1cd8f2fbf041ca92fc0c91fa4cd5baa73625b6de4bc55d303f79`.

SOFTWARE PASS / HARDWARE PENDING. Разработка на этом шаге остановлена.
Quick → 4 настоящих map frames; затем Interactive → доступные курсор/кнопки/
меню/камера/звук, выход Minus+Plus. Логи отдельно в
/switch/matrixgame-diag/logs/<run-id>. Повторно копировать ресурсы не требуется.
Физические GPU/HID/audio, реальный FPS и полнота Init/map frames ещё не приняты.
Единая Campaign/Gov, естественная победа и возврат Galaxy standalone entry не охвачены.
