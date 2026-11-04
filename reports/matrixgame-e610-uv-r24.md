# MatrixGame E610 r24 — bulk UV defaults, active-field decode

После r23 DecodeTo просматривает dimensions всех 8 UV slots на каждую вершину,
заполняя defaults и условно копируя floats. Новый вариант делает uv.fill({0,0,0,1}),
затем декодирует active fields j<((fvf>>8)&15). Layout задаёт count<=8, dimensions
active slots в 1..4 и остальные 0; default constructor fvf=2/count=0/dims=0.
Все изменения Layout происходят только после validation; invalid Layout не
меняет эти поля. Других setters dimensions нет. Копирование state сохраняет
инвариант. Никакой новой информации/кеша в class layout нет.

Scope: только UV часть DecodeTo в fixed_vertex.cpp. Decode(vector) оставлен
независимым batch oracle. NativeDrawIndexed, r23 validation header, ownership,
quotas, shaders, GPU submit/wait/readback и ресурсы не меняются. Output successful
сравнивается по всем 192 bytes. Finite checks идут в прежнем порядке active fields.
UV defaults теперь записываются до UV decoding. На failure часть defaults может
отличаться, но контракт DecodeTo явно требует discard private unpublished output;
Record не вызывается при любой ошибке. Defaults assignment не бросает и не
меняет FP flags. Aliasing input/output по-прежнему запрещён контрактом.

Reference: полный r23 source 0d31db18d16e34a68d219c8a7ed26d3d8e268d70.
Runner использует frozen CPP/header и actual indexed body; сравнивает full stdout
на каждой платформе. Корпус r23 сохранён, добавлены UV count transitions
8→0→7→1→6→2→5→3→4 для четырёх flags и dimensions patterns, default constructor,
invalid Layout, 0xff padding (nonfinite pattern), output canaries/input immutable,
no heap DecodeTo. Все 192 bytes сравниваются с Decode(vector).

Отклонённый вариант /1: separate active/inactive loops. Gather median
270955 → 272264 мкс, общего выигрыша нет, несмотря на direct decode
89301 → 84437. Исходники/tool/test и решение сохранены в
build/matrixgame/e610-uv-r24-checks/rejected-span; benchmark/1 неизменён.
Выбранный bulk fill /2: direct 87396 → 83487, gather 269026 → 260660 мкс,
все candidate gather 262072/260542/260660 меньше frozen 268264/269026/270470.
Итоговый повторный ABBAAB после CPU gates приведён ниже.

Host CPU probe: 10 млн direct vertices, 4096 indexed draws/8388608 decoded
vertices, 25165824 indices U16; O2/UNDEBUG, no sanitizers/LTO/fast-math,
без parallel jobs. Это не GPU/whole frame/Init/FPS/бой. U32 correctness включена,
отдельное ускорение U32 не заявляется. Коэффициенты r10–r24 не перемножаются.

Ресурсы/saves/upstream/frozen/основная NRO и прежние пакеты сохраняются.
Большие боевые сценарии и полные regression suites не повторяются.
HARDWARE PENDING: Init/transition/map frames, physical GPU/HID/audio/FPS.
Standalone entry не охватывает единую Campaign/Gov, natural victory и Galaxy return.
[Два ручных запуска](../docs/matrixgame-e610-uv-r24-readme.ru.md).

## Итоговый CPU proof

Host ASan/UBSan/LSan, ARM64/QEMU UBSan и Windows O2 /1 — PASS.
24194 cases, 12983 expected rejections, UV_TRANSITIONS=289, topology=7776,
scenarios=1948, layouts=864, no_publication=216. Полный stdout frozen/current
равен попарно; final digest host 2905387506710110368, ARM64
18110286455085150656, Windows 17592794193880945263. Межплатформенная разница
FP flag constants/stdlib messages не является acceptance; сравнение локальное.
Assertions active, ARM64 UBSan/vptr сохранён (type_info workaround прежний).
Input immutable, output canaries и no-heap DecodeTo прошли; cleanup bytes=0.

Итоговый benchmark/3 ABBAAB: direct frozen 90132/90507/91803 мкс,
candidate 85497/86995/94921; median 90507 → 86995 мкс (−3.88%).
Gather frozen 283645/279769/286948, candidate 265756/262278/278749;
median 283645 → 265756 мкс (−6.31%). Все candidate gather меньше всех frozen.
Checksum 5258787986328867328 одинаков во всех runs. Последний candidate direct
медленнее baseline — вариация измерения. В предыдущей bulk серии /2 gather
улучшился на ~3.1%; величина выигрыша меняется. Не переносится на Switch FPS.
После этого дополнительные timing runs для выбора более выгодной цифры не делались.

## SDK/package и SHA

Source commit: `98a03c6a23bcf75eeebc879122d772bc16614c74`.
Build ID: `98a03c6a23bc-68e5513720e51ca7646e`.
Обе devkitA64/libnx/deko3d compile/link — PASS, strong undefined=0.
321 configured translation units: O2/UNDEBUG, exceptions, no fast-math;
prebuilt Rangers archive unchanged, не выдаётся за новую optimized сборку.
23 actual debug deko3dd object members / 0 release deko3d, mixed=false.
NRO structure/assets, embedded fonts/shaders, NACP titles и ELF→NRO build IDs
проверены. Generated original files identical r23 (3 historical delta vs E601).
714 own source hashes проверены. Production diff vs r23 — только DecodeTo UV;
дополнительно r24 titles и CPU runner. Остальные r23 build inputs unchanged.
304 прежних защищённых артефакта SHA unchanged; game/frozen/main NRO SDK gate PASS.

Actual ARM64 DecodeTo: отдельный defaults loop заполняет bytes 64..191 Vertex
шагом 16 (8 UV slots), без dimension loads/calls; затем UBFX извлекает bits 8..11
FVF один раз и active loop читает только нужные dimensions. Пер-UV finite
classification и cold NonfiniteVertexValue сохранены, NativeDrawIndexed вызывает
DecodeTo. r23 direct Locked load остаётся перед validation loop в обеих ELF.
Это proof включения изменения в NRO, не физической производительности GPU.
Полный disassembly, default fill loop и active count extraction — в evidence.

ZIP: `dist/matrixgame-e610-switch-test-r24-uv.zip` (10162705 bytes).
SHA256: `80bb8a2248a19496df85d3e60c187f3436ef626e351b972bd1e8cc0bba5b61b3`.
Ровно 6 файлов: 2 NRO, README.ru.md, manifest.json, SHA256SUMS.txt, build-info.json.
Ресурсов/сохранений нет. Manifest связывает source/input/ELF/NRO hashes и build IDs.

| NRO | SHA256 | ELF Build ID |
| --- | --- | --- |
| matrixgame-e610-quick-smoke.nro | `0eecec9322cbf1ea71efc5a1ddb6c28feabf036a842a1637a1ff4674469c55ad` | `a0394292f16e59a24a1b8a8b1627e782d27f5d60` |
| matrixgame-e610-interactive.nro | `7e021ad055b5640bed4e900ce80320a962c210fbf178e0b354e9df1c2e926a69` | `77642ba8d615f553923c25ca37b7b8bbbf0c0e4a` |

## Воспроизведение и аппаратные границы

CPU: `python tools/matrixgame_uv_span_checks.py host` (также arm64/windows),
затем `python tools/matrixgame_uv_span_checks.py benchmark` без parallel jobs.
Runner сохраняет frozen r23 CPP/header, actual gather/wrapper, commands, stdout,
source/input hashes и результаты в новый numbered каталог.
SDK: `python tools/matrixgame_current_diagnostic_sdk.py --output-root
build/matrixgame/e610-uv-r24` — создаётся новый numbered каталог.
Package: `python tools/matrixgame_e610_freeze.py package --build
build/matrixgame/e610-uv-r24/1 --source-commit 98a03c6a23bcf75eeebc879122d772bc16614c74
--output dist/matrixgame-e610-switch-test-r24-uv.zip --readme
docs/matrixgame-e610-uv-r24-readme.ru.md --rangers-root
sdmc:/switch/space-rangers-hd-a-war-apart/game` (новому выводу дать новое имя).
Локальный gate: build/matrixgame/e610-uv-r24-checks/verify_delivery.py с source
commit как аргументом. [Полный evidence](matrixgame-e610-uv-r24-evidence.json).

SOFTWARE PASS / HARDWARE PENDING. Разработка на этом шаге остановлена.
Quick → 4 настоящих map frames, затем Interactive → доступные кнопки/меню/
камера/звук и Minus+Plus. Полная инструкция в ZIP; логи отдельно в
/switch/matrixgame-diag/logs/<run-id>. Ресурсы повторно копировать не требуется.
Физические GPU/HID/audio, Init/Draw/FPS не приняты. Единая Campaign/Gov,
естественная победа и возврат Galaxy этим standalone entry не охватываются.

Evidence SHA256: `1742f61b2ea96f1996f26f15f247c5f727f4f2a5203fb689f6beaefbcc24540a`.
