# MatrixGame E610 r21 — fixed-size FVF field copies

Следующий подтверждённый CPU расход после r20: DecodeTo вызывает memcpy с
runtime size для position и UV fields. FVF хранит 1–4 floats в поле; size
проверяется Layout. Выбран switch на memcpy размеров 4/8/12/16 bytes внутри
DecodeTo. Fallback default оставлен, layout/API/stride не меняются.
Read/copy/finite order внутри поля и между полями тот же. Нет прямых unaligned
float loads через cast, sizeof(Vertex)=192, class layout не изменяется.
Decode(vector) оставлен как независимый batch oracle. DecodeOne по-прежнему
обёртка DecodeTo. Output на failure остаётся unpublished; draw не доходит Record.

CPU proof сравнивает полные native state и fixed_vertex.cpp frozen r20 source
33356be5cf30e47513ec66bb8ce7a885c3bcfa8d с текущими; actual NativeDrawIndexed
body одинаков. Изменён только floats lambda в DecodeTo. Resource Unmap r20,
coverage r18, input hoist r19, ownership/quotas/shaders/upload order сохранены.

Тесты: 864 FVF/padding layouts, сравнение всех 192 bytes с Decode(vector),
indices/base/U16/U32/holes/rebind, zero heap в DecodeTo, OOM-before-state access,
216 rejects без Record. Добавлены 2250 field probes: разные position/normals/
UV dimensions, alignment 1/2/3, +/-zero, +/-subnormal, max finite, +/-Inf,
quiet/signaling NaN. Каждый float field проверяется отдельно. FP exception
flags, ошибки и полные successful output bytes попарно frozen/current.
Итого 12122 cases, 5404 expected rejections. Не сравниваем платформенные
exception messages/checksums между разными стандартными библиотеками.

Первая exploratory серия benchmark/1: direct median 218127 → 206937 мкс,
gather 383785 → 366526 мкс. Полный fixture финализирован после неё;
exploratory runner архивирован. Ниже приведён итоговый ABBAAB без parallel jobs.
10 млн direct vertices и 4096 indexed draws / 8388608 decoded vertices /
25165824 indices. Host O2 UNDEBUG, no sanitizers/LTO/fast-math.
CPU decoder/gather, не GPU/whole frame/Init/бой/FPS. Коэффициенты разных
оптимизаций не перемножаются; последний Switch Draw r9 не является r21 timing.

Ресурсы/saves/upstream/frozen/основная NRO и старые пакеты сохраняются.
Большие Windows боевые сценарии и полные regression suites не повторяются.
HARDWARE PENDING: Init→transition→4 map frames, physical HID/GPU/audio/FPS.
Standalone NRO не охватывает единую Campaign/Gov, natural victory и возврат Galaxy.
[Два ручных запуска](../docs/matrixgame-e610-copy-r21-readme.ru.md).

## Итоговый CPU результат

Host ASan/UBSan/LSan, ARM64/QEMU UBSan и Windows O2 /1 — PASS. Full stdout
frozen/current совпал попарно. FP flags включены в digest: host
13555136019721900891, ARM64 1261388803015844955, Windows 10244804245530054132.
Разница digest между платформами ожидаема из-за FP flag constants/exception
messages; ни один межплатформенный checksum не используется как acceptance.
Assertions активны, ARM64 workaround скрывает только type_info symbol,
UBSan/vptr остаются включены. Allocation-free DecodeTo и no-publication errors
сохранены. Historical r20 ownership/Unmap proof не перезапускался: API/layout,
state/buffer behavior unchanged; новый decoder проверен адресно.

Итоговый benchmark/2 ABBAAB:
- direct frozen 216373/217560/214030 мкс, candidate 197590/199372/197337;
  median 216373 → 197590 мкс (−8.68%).
- gather frozen 382694/375974/378306, candidate 356058/366180/368495;
  median 378306 → 366180 мкс (−3.21%).
- checksum 5258787986328867328 одинаков во всех 6 runs.

Gather выигрыш повторился относительно exploratory серии /1 (−4.5%).
Числа относятся к этому CPU probe; физический GPU и FPS не измерялись.

## SDK/package и контрольные суммы

Source commit: `3282afcd569f84a633108abefbfbf53e3e4bb1a0`.
Build ID: `3282afcd569f-30cc928d55b517e630be`.
Обе devkitA64/libnx/deko3d compile/link — PASS. Strong undefined = 0.
321 configured translation units: O2/UNDEBUG, exceptions, no fast-math.
Prebuilt Rangers archive не менялся и не выдаётся за новый optimized build.
23 actual debug deko3dd object members / 0 release deko3d; mixed=false.
NRO segments/assets, fonts/shaders, NACP titles и ELF→NRO build IDs проверены.
Generated original files идентичны r20; 3 исторических delta vs E601.
711 own source hashes проверены. Production diff vs r20 — только DecodeTo
lambda; дополнительно две NACP titles и новый CPU runner. Все другие r20
build inputs совпадают. Все 283 прежних защищённых артефакта SHA unchanged;
основная NRO/game/frozen checks SDK gate прошли.

Actual ARM64 DecodeTo: memcpy call sites 2 → 1. Position 3/4 floats теперь
копируется load/store без вызова memcpy. Единственный остаточный memcpy
обслуживает default switch; public Layout задаёт dimensions только 1–4.
Finite вызовы/stride guards сохранены, NativeDrawIndexed вызывает DecodeTo,
r19 Read/Storage hoist сохранён, DecodeOne copy в indexed path отсутствует.
Full disassembly обеих NRO хранится в evidence.

ZIP: `dist/matrixgame-e610-switch-test-r21-copy.zip` (10162430 bytes).
SHA256: `2e4ff22b279990718adfd8d23361e348672eb99146e8ec5c798c9671b86680a2`.
Ровно 6 файлов: две NRO, README.ru.md, manifest.json, SHA256SUMS.txt,
build-info.json. Игровых ресурсов/сохранений нет. Runtime — HARDWARE PENDING.

| NRO | SHA256 | ELF Build ID |
| --- | --- | --- |
| matrixgame-e610-quick-smoke.nro | `db309e4042181043ae46c2d88c5fa7a71d64495c0f7db3c3038071733539d25b` | `14a7736a5f372046bc9ccd3851940d9eb46b8b62` |
| matrixgame-e610-interactive.nro | `4a9f0489304401ca4502fa643aa26e51cdad920343ba893641167513c7c9e1c5` | `13ce729bb88dcc7c6972d2f6bb85270a1ea8560b` |

## Воспроизведение и границы

CPU: `python tools/matrixgame_vertex_copy_checks.py host` (также arm64/windows),
затем `python tools/matrixgame_vertex_copy_checks.py benchmark` без parallel jobs.
Runner автоматически сохраняет frozen r20 CPP, actual gather body, command logs,
source/input hashes, stdout и результаты в отдельный numbered checks каталог.
Требуются установленные WSL GCC, ARM64 GCC15/QEMU и Windows MSVC2022.
SDK: `python tools/matrixgame_current_diagnostic_sdk.py --output-root
build/matrixgame/e610-copy-r21` — создаёт новый numbered каталог.
Package: `python tools/matrixgame_e610_freeze.py package --build
build/matrixgame/e610-copy-r21/1 --source-commit 3282afcd569f84a633108abefbfbf53e3e4bb1a0
--output dist/matrixgame-e610-switch-test-r21-copy.zip --readme
docs/matrixgame-e610-copy-r21-readme.ru.md --rangers-root
sdmc:/switch/space-rangers-hd-a-war-apart/game` (для нового вывода взять новое имя).
Локальный gate: build/matrixgame/e610-copy-r21-checks/verify_delivery.py с source
commit как аргументом. [Полный evidence](matrixgame-e610-copy-r21-evidence.json).

SOFTWARE PASS / HARDWARE PENDING. На этом этапе разработка остановлена.
Два ручных запуска: Quick → 4 настоящих map frames, затем Interactive → доступные
управление/меню/камера/звук и Minus+Plus. Логи — отдельные run-id каталоги в
/switch/matrixgame-diag/logs. Полная инструкция есть в ZIP и linked README.
Физический GPU/HID/audio, Init, Draw/FPS не приняты. Campaign/Gov, естественная
победа и возврат результата Galaxy этим standalone entry не охватываются.

Evidence SHA256: `127e20d69d5d2fd1bf506edb3b9309d3687f99fdefcdc07378267fee3bd0ff62`.
