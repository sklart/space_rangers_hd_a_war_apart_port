# MatrixGame E610 r25 — ordered four-step matrix accumulation

В actual r24 ARM64 Multiply есть SIMD по четырём columns, но остаётся inner
k loop из четырёх итераций. FixedVertexState::Prepare вызывает Multiply дважды
на native draw packet: camera=world*view и clip=camera*projection. Источник
Record по-прежнему вызывает vertex.Prepare один раз на draw; per-draw guards,
normal/light preparation, uniforms/upload и GPU operations не пропускаются.

Принята compiler-specific форма: GCC выполняет четыре явных += в прежнем
порядке k=0..3 с начальным float value=0; MSVC использует исходный реальный
r/c/k loop с out{}. Обе ветки считают настоящую полную Matrix. На проверенных
компиляторах сохранены raw NaN payloads и FP flags, включая signed zero.
Reassociation и FMA не включены. Это compiler-specific optimization, не stub.
Input matrices const, output private stack Matrix, alias a==b допустим.
Никаких кешей, новых полей/ABI, allocations или изменений transforms API.
Scope: только Multiply в fixed_vertex.cpp. DecodeTo/UV r24 и validation r23 untouched.

Reference: полный r24 source 98a03c6a23bcf75eeebc879122d772bc16614c74.
Runner использует frozen CPP/header и actual indexed body; весь прежний корпус
сохранён. Новые matrix probes проверяют все 64 bytes и FP flags в FE_TONEAREST,
FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO; случайные finite matrices с независимым
original-loop oracle, a==b, каждый field в обоих operands с +/-zero/subnormal/
min normal/max finite/Inf/quiet/signaling NaN, dual NaN payloads. Edge outputs
попарно сравниваются с actual frozen CPP через digest, без канонизации NaN.
Полные lit/unlit VertexUniforms bytes также включены в сравнение. Inputs
неизменны, Multiply без heap. Rounding mode восстанавливается после probes.

Отклонённая явная развёртка /1 дала Multiply median 110597 → 48613 мкс,
Prepare 120890 → 101965, но strict Windows comparison не прошёл. Trace из
7744 прямых случаев выявил 56 различий dual-NaN payload; первое 0x7fc12345 →
0x7fc54321 при тех же FP flags. GCC host/ARM64 этот вариант проходили; на Windows
finite independent oracle тоже проходил, но полный raw frozen digest отличался.
Тесты не ослаблены и NaN не канонизируются. Исходники/failing evidence/trace
сохранены в checks/rejected-explicit, windows/1 и windows-trace2. Первый trace
build не имел tests include directory; его compile failure также сохранён.

GCC pragma unroll 4 тоже отклонена: host/2 и arm64/2 strict digest отличаются
от frozen; Windows/2 с исходным loop проходит. Исходник/решение в rejected-pragma.
Тесты не менялись и NaN не канонизируются. Принята явная форма только на GCC,
которая проходит host/ARM64, и прежний MSVC loop, сохраняющий Windows baseline.

Дополнительный SDK gate: полный fixed_vertex.cpp скомпилирован devkitA64 теми
же target/O2/FP flags, что r24 SDK. Relocation-free Multiply section извлекается
в raw code, Linux ARM64 ABI wrapper вызывает эти настоящие SDK инструкции под
QEMU. Identity проверяет ABI hidden return, 7744 cases печатают все 64 bytes и
FP flags в 4 rounding modes, включая distinct dual NaNs. Сравнение полного
stdout, без digest-only/canonicalization. Source/probe/commands/bin hashes сохраняются.
Это реальный SDK CPU function proof, не NRO runtime и не физическая Switch.
SDK binary /2 — PASS: old 80 bytes, new 92 bytes, stdout одинаков.
Raw hashes: old c2732b68cbbd5c8af91d0ed01aad129d9f4b0d4ac95a52c15f56380cb8a1d06d;
new a575632d8915cd0fd1bc6df10d9c3564789a270a1baad17eae38bd3bd0be5195.
После полной SDK сборки gate должен подтвердить byte identity этих functions
с соответствующими NRO ELF, чтобы связать QEMU proof именно с deliverable.

Benchmark: 10 млн Multiply и 3 млн настоящих FixedVertexState::Prepare.
Prepare без Record/GPU substitution; trace не включает GPU. Свет выключен в
performance probe; lit path проверяется на correctness. World меняется раз
в 1024 iterations; matrices/uniforms действительно вычисляются и читаются.
Host O2/UNDEBUG, no sanitizers/LTO/fast-math, без parallel jobs.

Это CPU uniform preparation, не indexed gather/whole Draw/Init/FPS/бой.
Количество таких операций и вклад в кадр на Switch не измерены. Коэффициенты
r10–r25 не перемножаются; прежний hardware Draw r9 не является r25 timing.
Ресурсы/saves/upstream/frozen/основная NRO и прежние пакеты сохраняются.
Большие боевые сценарии и полные regression suites не повторяются.
HARDWARE PENDING: Init/transition/map frames, physical GPU/HID/audio/FPS.
Standalone entry не охватывает единую Campaign/Gov, natural victory и Galaxy return.
[Два ручных запуска](../docs/matrixgame-e610-matrix-r25-readme.ru.md).

## Принятые CPU результаты

Semantic /3: host ASan/UBSan/LSan, ARM64/QEMU UBSan и Windows O2 — PASS.
32450 cases, 12983 expected rejections; MATRIX_EDGE=8256, UV=289, topology=7776.
Raw stdout frozen/current равен на каждой платформе. Final digest host
9926359832099014635, ARM64 4369608258136481195, Windows 876880510143895092.
Межплатформенная разница flags/messages не является acceptance; сравнение локальное.
Assertions active; ARM64 type_info workaround не отключает UBSan/vptr.

Выбранный вариант benchmark/2: Multiply 108801 → 47528 мкс,
Prepare 122381 → 102243 мкс (−16.46%). Итоговый ABBAAB /3:
- Multiply frozen 107013/108482/106770, candidate 47746/47569/47234;
  median 107013 → 47569 мкс (−55.55%) на 10 млн.
- Prepare frozen 124212/124834/121488, candidate 97597/98593/101797;
  median 124212 → 98593 мкс (−20.63%) на 3 млн.
- Checksum 11336931503703773827 одинаков во всех runs.

Величина Prepare выигрыша между сериями отличается (~16–21%). Это только
изолированная CPU подготовка uniforms, не выигрыш всего draw/frame.
После финального замера дополнительные timing runs для выбора цифры не делались.

## SDK/package и окончательная связка proof

Source commit: `d6ad833e2a4bb9418abfe5480065a1ce7717be64`.
Build ID: `d6ad833e2a4b-6ecaa7dda756977f5dbf`.
Обе devkitA64/libnx/deko3d compile/link — PASS, strong undefined=0.
321 configured translation units: O2/UNDEBUG, exceptions, no fast-math;
prebuilt Rangers archive unchanged, не выдаётся за новую optimized сборку.
23 actual debug deko3dd object members / 0 release deko3d, mixed=false.
NRO structure/assets, embedded fonts/shaders, NACP titles и ELF→NRO IDs проверены.
Generated original files identical r24 (3 historical delta vs E601).
716 own source hashes проверены. Production diff vs r24 — только Multiply;
дополнительно r25 NACP titles и два CPU/SDK runners. Остальные build inputs unchanged.
311 прежних защищённых артефактов SHA unchanged; game/frozen/main NRO gate PASS.

Окончательный gate связал SDK binary QEMU proof с deliverables: raw bytes
Multiply из archived r24 ELF совпадают с frozen SDK object (80 bytes), а raw
bytes из обеих r25 ELF совпадают с tested candidate object (92 bytes).
Prepare в обеих r25 NRO вызывает именно эту Multiply дважды. В Multiply
остался один outer loop/backedge вместо двух loops/backedges, FMUL/FADD
сохранены, FMA отсутствует. Никаких relocations/external calls в tested leaf
function нет; ABI identity probe и полный 7744-case stdout совпали.
UV r24 и direct Locked hoist r23 включены unchanged и проверены assembly gate.
Это CPU function execution под QEMU и software package proof, не hardware acceptance.

ZIP: `dist/matrixgame-e610-switch-test-r25-matrix.zip` (10162665 bytes).
SHA256: `ccc33856b8dd97b5d1ed07a69ba2288301f15d510592b09fe8c1429e5833ec24`.
Ровно 6 файлов: 2 NRO, README.ru.md, manifest.json, SHA256SUMS.txt, build-info.json.
Ресурсов/сохранений нет. Manifest связывает source/input/ELF/NRO hashes и build IDs.

| NRO | SHA256 | ELF Build ID |
| --- | --- | --- |
| matrixgame-e610-quick-smoke.nro | `14b10f2e5186f625cd97cbf1a8ea11a692ffe7573888e37528611d664f7365d2` | `fb5ac7df0948d9b59d781291d0a0bb7e73d9ed8c` |
| matrixgame-e610-interactive.nro | `eb9fabe8d37da13731c8fa07c83b6c0e0f856c369028bfff8c97c7d10cf1cc19` | `084e8e14f8fefa4c49688738e924aa853048b451` |

## Воспроизведение и аппаратные границы

CPU: `python tools/matrixgame_matrix_unroll_checks.py host` (также arm64/windows),
затем `python tools/matrixgame_matrix_unroll_checks.py benchmark` без parallel jobs.
Actual SDK kernel: `python tools/matrixgame_matrix_sdk_checks.py` — отдельный
numbered каталог с SDK object/section/bin/relocation proof, ABI wrapper, probe,
полными raw outputs и hashes. SDK flags берутся из verified r24 build metadata;
gate дополнительно требует exact byte identity с настоящими NRO ELF.
SDK: `python tools/matrixgame_current_diagnostic_sdk.py --output-root
build/matrixgame/e610-matrix-r25` — создаётся новый numbered каталог.
Package: `python tools/matrixgame_e610_freeze.py package --build
build/matrixgame/e610-matrix-r25/1 --source-commit d6ad833e2a4bb9418abfe5480065a1ce7717be64
--output dist/matrixgame-e610-switch-test-r25-matrix.zip --readme
docs/matrixgame-e610-matrix-r25-readme.ru.md --rangers-root
sdmc:/switch/space-rangers-hd-a-war-apart/game` (новому выводу дать новое имя).
Локальный gate: build/matrixgame/e610-matrix-r25-checks/verify_delivery.py с source
commit как аргументом. [Полный evidence](matrixgame-e610-matrix-r25-evidence.json).

SOFTWARE PASS / HARDWARE PENDING. Разработка на этом шаге остановлена.
Quick → 4 настоящих map frames, затем Interactive → доступные кнопки/меню/
камера/звук и Minus+Plus. Полная инструкция в ZIP; логи отдельно в
/switch/matrixgame-diag/logs/<run-id>. Ресурсы повторно копировать не требуется.
Вклад matrix preparation в реальный кадр неизвестен; fps/Init не измерены.
Физические GPU/HID/audio не приняты. Единая Campaign/Gov, естественная победа
и возврат Galaxy этим standalone entry не охватываются.

Evidence SHA256: `8d9c8a8ae132c6f79cb80c09809cdbd20726eb18a76d8afe550e34b738e136b4`.
