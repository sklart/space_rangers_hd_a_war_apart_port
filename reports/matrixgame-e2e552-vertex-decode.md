# E2E-5.52 — одновершинный FVF decode без временного vector

В NativeDrawIndexed каждый новый referenced vertex декодировался через
`Decode(one_stride).at(0)`. Decode возвращает std::vector, поэтому gather
создавал и освобождал отдельный heap block для каждой уникальной вершины.
Это расход портирующего glue, не требование оригинального indexed draw.

Добавлен FixedVertexState::DecodeOne, возвращающий Vertex значением и
проверяющий один полный stride. Позиция/normal, diffuse/specular ARGB,
texcoords 1–4 компонентов, defaults и Finite проверки сохранены.
Прежний batch Decode, Prepare, EvaluateVertex, uniform layout и shaders
не изменены. В NativeDrawIndexed заменён только Decode(...).at(0) на
DecodeOne(...); исходные bounds/base/minimum/index/retained-buffer проверки
и Record payload остаются прежними.

## Доказательства

| Gate | Фактический результат |
| --- | --- |
| HOST ASan/UBSan | 864 layout/stride cases, 3456 вершин побайтно совпали с неизменённым batch Decode. 2592 отрицательных случая. DecodeOne valid-input path работает при запрещённом heap allocation. |
| ARM64/QEMU | Те же численные показатели и bit-exact результаты. Не является Switch CPU/GPU performance измерением. |
| Exact indexed gather | 6144 вызова прежнего и нового извлечённых production method bodies на настоящем buffers::Context, после release владельцами VB/IB. Проверены U16/U32, nonzero minimum/start, negative base, repeated/sparse indices и bounds rejection. Все Vertex bytes, indices, GPU base, topology/count/format совпали. |
| Heap count | В этих 6144 gathers устранены 18 432 heap allocation, ровно по одной на каждую декодированную уникальную вершину. Сам gather по-прежнему выделяет выходной vector/bitmap, а настоящий Record — GPU packets. Это не обещание allocation-free кадра или измеренный FPS. |
| COMPILE/LINK | Actual DekoDevice indexed path с DecodeOne в полном diagnostic devkitA64 ELF; результат и symbol audit в SDK profile. |
| WINDOWS HAL | Не повторялся: Windows original D3D9 Draw не вызывает DecodeOne; прежние batch/reference math и shader code сохранены. |
| SWITCH HARDWARE | NOT RUN / HARDWARE_PENDING; карта, кадр и FPS ещё не подтверждены. |

FVF basis охватывает XYZ/XYZRHW, normal где допустим, color combinations,
0–8 texcoord sets и все размеры texcoords в basis patterns, tight/padded
strides. Indexed cases используют шесть FVF из прежнего reference набора:
112/212/352/3d2/10112/144 (hex). Это описанная матрица, не утверждение о
всех возможных произвольных FVF bit combinations.

Тела gather извлекаются прямо из platform_switch.cpp. Для baseline сохранён
точный прежний method; runner допускает только одну заданную замену вызова.
CPU harness переименовывает qualifier и направляет Record в data sink для
проверки payload, не изображает successful Init или GPU render. Primitive
mapping также взят из production source. Неизменность прежнего декодера
дополнительно проверяется восстановлением SHA-256 fixed_vertex.cpp E551
после удаления только добавленного DecodeOne.

```powershell
python tools/matrixgame_decode_one_checks.py host
python tools/matrixgame_decode_one_checks.py arm64
python tools/matrixgame_actual_draw_checks.py sdk --output build/matrixgame/e2e552-vertex-decode --diagnostic-source build/matrixgame/e2e543-journal/source
```

Команды, binaries, inputs: `build/matrixgame/e2e552-vertex-decode`.
Итог: `matrixgame-e2e552-vertex-decode-evidence.json`.
Original generated Init/Draw sources, основная NRO, оба переданных diagnostic
пакета, upstream/frozen, игровые ресурсы и сейвы сохраняются. Третьей NRO,
commit/push и новых аппаратных запусков нет. Последняя наблюдённая аппаратная
граница остаётся прежней; это офлайн-продвижение исходников, не первый кадр.
