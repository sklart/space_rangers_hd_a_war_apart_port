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
