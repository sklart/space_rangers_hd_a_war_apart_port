# MatrixGame E2E-5.37 — native aliased lines для оригинальной миникарты

**CMinimap LINESTRIP SOFTWARE PASS / SWITCH RUNTIME PARTIAL.** Блокер
E2E-5.36 закрыт программно для исходного LASTPIXEL=TRUE. Полный native Init,
карта и первый кадр на Switch пока не исполнялись.

## Реализация и граница

NativeRenderState разрешает solid aliased lines при LASTPIXEL=TRUE и
ANTIALIASEDLINEENABLE=FALSE. DekoDevice::Record записывает реальные Lines /
LineStrip, явно задаёт line width=1 и выключает stipple. Существующая привязка
RasterizerState выключает smoothing, сохраняет first provoking vertex;
viewport с исходным half-pixel mapping, shaders, depth/stencil, blend и
retained vertex/index uploads остаются настоящими GPU путями.

Линия не пропускается, не заменяется пустой работой или фиксированной картинкой.
Геометрия CMinimap::Draw сохранена: исходные campts, LINESTRIP из четырёх
отрезков и campts[4]=campts[0]. CPU rasterizer вместо GPU не вводился.

LASTPIXEL=FALSE остаётся явным Unsupported при line draw. Значение хранится и
восстанавливается scalar/COW lease без подмены на TRUE; triangles от него
не зависят. Deko SDK не имеет независимого переключателя LASTPIXEL. На данном
HAL false/true дали одинаковое покрытие, но этого недостаточно для обещания
поддержки FALSE на другом устройстве. В исходной рамке миникарты используется
TRUE. AA lines, points, scissor и прочие неподдержанные режимы не разрешены
ради PASS.

Описания исходных правил:
[Microsoft D3D9 rasterization](https://learn.microsoft.com/en-us/windows/win32/direct3d9/rasterization-rules),
[LASTPIXEL](https://learn.microsoft.com/en-us/windows/win32/direct3d9/outline-and-fill-state).
Сопоставлены установленный deko3d SDK и
[реальная реализация line width/stipple/smoothing](https://github.com/devkitPro/deko3d/blob/master/source/maxwell/gpu_3d_state.cpp).
Это анализ API/команд, не запуск Tegra GPU.

## Адресные проверки GPU и настоящей карты

1. 398 HAL vs production vertex/fragment GLSL случаев: 16 направлений,
   fractional endpoints, оба порядка концов, LINE LIST/STRIP, clipping,
   замкнутые контуры, degenerate, alpha blending, FLAT/GOURAUD, variable RHW,
   оба SDK LASTPIXEL значения. GetRenderState подтверждает запрошенное значение.
   mismatches=0, max_delta=0; false остаётся неподдержанным native режимом.
2. Синхронный observer дополнительно сохраняет точные UP bytes линий после
   успешного SDK draw, не меняя исходные данные/device state. Из двух настоящих
   игровых сеансов получены 20 рамок камеры: исходные 5×XYZRHW, target 800×600,
   viewport (13,442,145,145), исходные color/alpha/stages/raster state.
3. Эти 20 original draw воспроизведены настоящим HAL и обоими production GLSL
   shaders на host GPU. Сравнены все 9600000 RGBA pixels: mismatches=0,
   max_delta=0. Контролируемый фон oracle — общий для обеих сторон; это не
   сравнение полного изображения игры с будущим Switch кадром.

GPU oracle использовал NVIDIA GeForce RTX 5060, OpenGL 4.6 NVIDIA 617.42.
Предварительный probe имел неверный `/Fo`: два промежуточных OBJ и новый probe
OBJ попали в старый build/e2e530-fog/windows. Путь исправлен; финальный runner
пишет только в e2e537-lines/windows. Старый fog EXE не был целью `/Fe`, golden
побайтно совпадает с прежним SHA. NRO/frozen/ресурсы этим не затронуты.

## Раздельные результаты

| Категория | Подтверждение |
| --- | --- |
| WINDOWS HAL full game | Два original Init → RobotPreload → stor.Load → PrepareMap → form loop (8 Draw / 125 ms) → focus release/recreate → дополнительные Draw/readback → Deinit. Gamma/window/leases/slots checks PASS. Native-state rejected sessions=0, exit 0. |
| WINDOWS HAL/GLSL line oracle | 398 addressed cases + 20 actual minimap draws, exact RGBA, exit 0. Это host GPU, не deko3d execution. |
| HOST ASan/UBSan | Production replay всех 33690 draw / 16574 snapshots: failures=0. Scalar/COW capture/restore сохраняет LASTPIXEL=FALSE и корректный отказ line draw; AA/point/scissor guards сохранены. PASS. |
| ARM64/QEMU | Тот же production replay и guard/restore scenario, PASS. Не Horizon/GPU. |
| COMPILE/LINK devkitA64 | Полная native-entry ELF, 183 reachable own object/archive inputs, 0 strong undefined; 255 generated original source files совпадают с E2E-5.35. |
| SWITCH HARDWARE | NOT RUN / HARDWARE_PENDING. Native allocation/upload, Init/PrepareMap, submit/Present и реальная рамка на консоли не подтверждены. |

Два новых бинарных state capture **побайтно совпали** с E2E-5.36 baseline:
изменился production native guard/backend, а не записанные игровые состояния.
Baseline FAIL artifacts E2E-5.36 сохранены. Большие fog/material/stencil наборы
не повторялись: shaders и triangle path не менялись; выполнена требуемая полная
Windows регрессия изменённых Draw/resources contracts и адресная проверка линий.

## Настоящая цепочка и следующий шаг

Source-confirmed CMinimap::Draw → native line guard больше не мешает проверенному
первому кадру. В записанном map stream новых state rejects нет. Это не означает
успешный Init на Switch: resource lookup, GPU allocations/residency, upload,
queue/frame ordering и Present ещё требуют настоящего выполнения. Первый
аппаратный runtime отказ не наблюдался.

Следующий адресный программный сценарий — оригинальные MouseKey/drag selection
и действия боя с тем же наблюдением draw/state/lifetime; source-derived
непроверенные ветки не объявляются новыми runtime blockers без воспроизведения.
LASTPIXEL=FALSE, AA/point/wireframe, queue-loss recovery и integrated host
transition не закрываются этим этапом. F9 screenshot остаётся Unsupported.

Профили: build/matrixgame/e2e537-lines/{windows,host,arm64,sdk,map-state/*}.
Исходные line bytes: map-state/windows/line-geometry-0/1.bin.
[Сводные hashes/commands/evidence](matrixgame-e2e537-lines-evidence.json).
Актуальный [Init/cleanup plan](../docs/matrixgame-init-sequence.md).

Upstream, основная NRO, frozen-комплекты, игровые ресурсы и сейвы не менялись.
Commit/push не выполнялись. P0, GI repeating-text causality, manual-publish
ENOENT, прежние Switch hangs и два E2E-5.34 raw HAL retry FAIL остаются OPEN.
