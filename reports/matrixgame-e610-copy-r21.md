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
