# MatrixGame E610 r17 — direct indexed vertex decode

Следующий CPU расход после r16 найден в NativeDrawIndexed: DecodeOne возвращает
192-byte Vertex во временную stack область, после чего caller копирует её в
vertices[local]. Это подтверждено actual r16 SDK ELF: после BL DecodeOne идут
192 bytes Q-register loads со stack и 192 bytes stores в vector. Никакого
аппаратного профиля/FPS Switch этим исследованием не получено.

## Изменение

DecodeTo пишет в private unpublished Vertex без return-value copy. Парсер
сохраняет прежний порядок position/normal/diffuse/specular/8 UV и все проверки
FVF/stride/single-record extent/finite. Первые 64 bytes defaults и все UV
инициализируются до успешного возврата; UV не обнуляются заранее только для
того, чтобы затем записать их снова. DecodeOne остаётся value-return wrapper.
Новая DecodeTo требует disjoint input/output; при ошибке output может быть
частичным и должен быть отброшен. Это новый API contract, не ослабление
существующего DecodeOne: его результат при exception не публикуется как прежде.

NativeDrawIndexed меняет ровно один вызов: вместо DecodeOne+assignment — DecodeTo.
ValidateIndexed, budgets, storage bounds, index order, base/minimum math,
decoded bitmap, declared count, profile scopes и Record остаются прежними.
Output vector принадлежит этому вызову, disjoint от retained input buffers;
после exception он уничтожается до Record/NativeDrawUpload. Неиспользуемые
index holes сохраняют старый Vertex{} (UV w=0), decoded UV w=1 как прежде.
Нет кеша, новых state fields, изменений shader/uniform, GPU ordering/ownership,
лимитов или оригинальной игровой логики. NativeDraw/UP batch decode unchanged.

## CPU proof

Один probe отдельно скомпилирован с полными frozen r16 CPP из source
2ff779a1b60c1b0623063e9846938129197d3b30 и candidate CPP. Тот же actual extracted
NativeDrawIndexed body, различие строго ограничено вызовом DecodeTo. NativePrimitive
и ProfileScope настоящие. GPU Record заменён только внутри CPU fixture явным
сборщиком данных: это не GPU execution и не fake PASS production NRO.

9864 cases, 5400 expected rejections; 864 FVF/padding layouts и полные 192 bytes
сравниваются с независимым unchanged batch Decode. 144 indexed gathers при
U16/U32, six FVF и base -2/0/3: exact vertices, holes/defaults, indices/order,
base/count/topology. Проверены decode без heap, sentinel output, успешное reuse
после NaN, wrong stride/extent/nonfinite input. 216 failures (negative effective
base, declared range, index extent, locked VB/IB, late NaN после bounds validation)
не вызывают Record и сохраняют прежний CPU sink. Buffers release подтверждён
AllocatedBytes=0. All-platform stdout digest 16179663140020244155.
Host ASan/UBSan/LSan O2, ARM64/QEMU UBSan O2, Windows O2 — PASS. Assertions on.
ARM64 local type_info version script сохраняет UBSan/vptr.
COW/snapshot/OOM/restore: restores=9 capture_OOM=2 COW_OOM=1 restore_allocations=0
и frame/expired-owner/held-resource guards — PASS отдельно на трёх платформах.
Большие бои/полные suites не повторяются.

## Timing

Первый exploratory вариант с v=Vertex{} перед parser не дал устойчивого gain;
его snapshot и ABBAAB результаты сохранены в benchmark/1. Итоговый вариант
не очищает UV дважды. Benchmark/2 без параллельных jobs, O2/UNDEBUG,
ffp-contract=off, без sanitizers/LTO/fast-math:

- 1000000 direct decodes: frozen 24761/24681/24691 мкс, candidate
  23024/22220/23862; median 24691 → 23024 (−6.8%).
- Actual CPU indexed gather, 256 draws, 524288 decoded vertices, 1572864 indices:
  frozen 30117/28501/29296 мкс, candidate 26737/26579/28829;
  median 29296 → 26737 (−8.7%).
- Checksum 1052755254566562048 совпал во всех шести runs.

Record benchmark потребляет CPU данные, не выделяет GPU memory и не submit.
Synthetic vertex/gather workload — другой участок, чем uniform-preparation
benchmark r16; коэффициенты нельзя перемножать или выдавать за FPS/Init.
Runner: tools/matrixgame_decode_target_checks.py; evidence root
build/matrixgame/e610-decode-r17-checks, semantic/ownership /1, benchmark /2.

## Границы

HARDWARE PENDING: Init→transition→4 настоящих map frames, физические GPU/HID/
audio/FPS требуют ручных запусков. Standalone entry не проверяет единую
Campaign/Gov, естественную победу и возврат Galaxy. Last r9 hardware Draw
около 0.56 с не является измерением r17. R10–r16 fixes сохранены.
Upstream/frozen/resources/saves/основная NRO/старые пакеты не меняются.
[Инструкция двух запусков](../docs/matrixgame-e610-decode-r17-readme.ru.md).
