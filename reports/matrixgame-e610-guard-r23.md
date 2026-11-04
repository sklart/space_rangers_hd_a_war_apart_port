# MatrixGame E610 r23 — validation-local binding predicate

После r22 ValidateIndexed выполняет buffer-null/Locked/stride guards для
каждого индекса. В этом callback-free read-only validation scope buffer,
lock и stride неизменны. VertexCoverage теперь сохраняет только boolean
invalid_binding_ в constructor; Require по-прежнему проверяет predicate и
negative effective index после declared-index range guard каждого элемента.
Ошибка не бросается в constructor. Приоритет первой ошибки сохранён.

Scope: только private local class в staging_draw.hpp. Нового persistent state,
кеша между draws, изменения Resource/Context API/layout или GPU state нет.
Каждый следующий draw/Discard создаёт новый predicate. Constructor делает
только pure noexcept reads. Context удерживает resource на время validation.
Initialized fallback не вызывает callbacks и не меняет буфер. Поддерживаемый
контракт — один владеющий engine/render thread, как и прежде.

Reference: полный r22 source 14d8a8186d943bb52ae49011a42a5510c58950a8.
CPU runner использует настоящий frozen/current staging header с wrapper в
отдельном каталоге; NativeDrawIndexed body, native state, fixed vertex,
buffer resources неизменны. Full output bytes/errors сравниваются попарно.

CPU tests содержат r22 finite/FP/state tests, плюс full/partial initialized,
Discard→следующий draw, holes, missing/locked VB/IB, negative base, extreme
strides/offsets и диапазоны. Отдельные topology probes для U16/U32, 0..7,
нулевого/ненулевого primitive count, start/minimum до UINT32_MAX, bases -1/0/
INT_MAX, declared range и index values до UINT32_MAX (U16 truncation настоящая).
Проверяются первые ошибки и неизменные buffer versions/lock/cleanup.

Отклонённый кандидат: U16/U32 dispatch вне validation loop, median gather
285469 → 281207 мкс (~1.5%) при перекрытии sample ranges. В production не
включён. Источник/tool/test/решение и результаты сохранены отдельно:
build/matrixgame/e610-validation-r23-checks/rejected и benchmark/1.
Production header восстановлен до r22, затем применён только predicate diff.

Guard candidate benchmark/1: gather median 275216 → 267277 мкс (−2.88%);
все candidate 266548/270489/267277 меньше frozen 276281/275216/272781.
Direct decode control неизменён: 88534 → 87561 мкс (вариация ~1.1%).
Итоговая повторная серия после CPU gates приведена ниже. Это synthetic host
CPU gather U16, не GPU/whole frame/Init/FPS/бой. U32 проходит correctness,
отдельное ускорение U32 не заявляется. Коэффициенты r10–r23 не перемножаются.

Ресурсы/saves/upstream/frozen/основная NRO и прежние пакеты сохраняются.
Большие боевые сценарии и полные regression suites не повторяются.
HARDWARE PENDING: Init/transition/map frames, physical GPU/HID/audio/FPS.
Standalone entry не охватывает единую Campaign/Gov, natural victory и Galaxy return.
[Два ручных запуска](../docs/matrixgame-e610-guard-r23-readme.ru.md).

## Итоговые CPU результаты

Host ASan/UBSan/LSan, ARM64/QEMU UBSan и Windows O2 /1 — PASS.
23905 cases, 12983 expected rejections, 7776 topology cases, 1948 scenarios,
full_scopes=2, partial_scopes=2. Полный stdout frozen/current равен попарно.
Final digest host 4637017888770985336, ARM64 9782670429877900952,
Windows 18147103199290582331. Межплатформенная разница FP flags/messages
не является acceptance; сравнение локальное. Assertions on, ARM64 UBSan/vptr
сохранён (type_info version workaround как прежде). Cleanup bytes=0;
ошибки draw не публикуют результат, очередность и versions/locks сохранены.

Benchmark/2 ABBAAB: frozen gather 278602/281828/321909 мкс,
candidate 268541/271439/294565; median 281828 → 271439 мкс (−3.69%).
Direct decoder control code unchanged: frozen 93126/89274/90582,
candidate 89588/86713/96316; median 90582 → 89588 мкс (~1.1% variation).
Checksum 5258787986328867328 во всех шести runs совпал. Последние samples
обеих версий медленнее остальных; ranges в /2 перекрываются. Изменение median
повторилось в /1 и /2, выигрыш небольшой и не превращается в оценку FPS.
Первый /1 был полностью разделён по ranges. Дальнейшие timing runs для
подбора более выгодной цифры не выполнялись.
