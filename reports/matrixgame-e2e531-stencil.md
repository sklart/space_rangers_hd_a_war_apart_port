# E2E-5.31 — native flat/stencil pipeline

Прежние guards SHADEMODE=FLAT и map StencilCaps закрыты программно.
Финальный matrixgame_stencil_audit.py подтвердил SOFTWARE PASS: профили,
исходные входы, восемь полных shadow buffers и защищённые входы проверены.
Полный native Init/map/frame остаётся PARTIAL /
HARDWARE_PENDING. Новая NRO и аппаратный запуск не выполнялись.

## Реальный backend

NativeRenderState сохраняет FLAT/GOURAUD и все original CW/CCW stencil states.
Stencil() строит реальный pipeline plan из того же authoritative state;
отдельного disconnected mirror нет. Scalar CDeviceState и COW lease возвращают
полные 32-bit ref/masks, faces, two-sided flag и shade mode.

Vertex shader отдаёт дополнительный flat diffuse после исходного lighting.
Fragment shader выбирает flat или smooth color во всех DIFFUSE/alpha аргументах.
Deko rasterizer явно получает First provoking vertex. UV остаются smooth;
индексы/геометрия оригинала не переписываются. PHONG остаётся Unsupported.
Material272/Vertex1328 ABI не менялись. RasterControls16 хранится в offset768
того же uniform packet, рядом с fog offset512; allocation по-прежнему4096,
поэтому дополнительный owner/allocation или изменение frame budget не нужны.
Каждый draw получает явный shade control, в том числе overlays/diagnostics.

Настоящий dk::DepthStencilState получает enable, fail/zfail/pass/compare для
front(CW) и back(CCW). При выключенном two-sided back использует CW параметры,
как в D3D9. commands.setStencil устанавливает ref/read/write mask для обеих
сторон. Восемь ops включают saturating и wrapping increment/decrement.
Младшие восемь бит reference/masks подтверждены actual D24S8 HAL, включая
reference256/511 и старшие mask bits. Enum IDs сверены SDK static_assert.

NativeStencilCaps сообщает только реализованные восемь операций + TWO_SIDED
(0x1ff), после проверки shader pair. Реальный Record до binding/allocation
отказывает при включённом stencil без D24S8 attachment. Clear продолжает
использовать самостоятельные clear masks; pipeline write mask его не подменяет.

Правила: [Microsoft shading modes](https://learn.microsoft.com/en-us/windows/win32/direct3d9/shading-modes),
[triangle fans](https://learn.microsoft.com/en-us/windows/win32/direct3d9/triangle-fans),
[two-sided stencil](https://learn.microsoft.com/en-us/windows/win32/direct3d9/two-sided-stencil).

## Что исполнялось

- WINDOWS HAL/production GLSL: 2310 stencil cases, mismatches0;
  eight ops, stencil fail/zfail/pass, eight comparisons, boundary values,
  read/write masks, high ref bits, CW/CCW, one/two-sided, disabled stencil,
  disabled depth. D3D stencil читается независимо через восемь color-resolve
  passes, GL — прямым stencil readback.
- WINDOWS shading: 72 FLAT/GOURAUD/cull/list/strip/fan cases с разными RGB/alpha
  и RHW; color delta0. Используются оба настоящих production shader stages.
- WINDOWS shadow sequences: 8 source-derived sequences с controlled geometry/
  fragment alpha: one-pass two-sided и two-pass fallback, alpha-tested REPLACE
  mask с color-write0, затем SRCALPHA MODULATE factorC0 quad. Все color/stencil
  buffers совпали побайтно. Альфа0/7 отсекается, 8/255 проходит; one/two-pass
  результаты одинаковы. Это fixture последовательностей исходного DrawShadows,
  а не исполнение полного CMatrixMap::DrawShadows или реальных map resources.
- HOST ASan/UBSan и ARM64/QEMU: production state/model по 2310 actual HAL
  goldens, exact; исходный CDeviceState render restore и COW lease restore,
  typed caps, bounds/unsupported shade/op, D24S8 attachment guard. Не GPU/Init.
- WINDOWS regression после общего shader interface change: 1476 material
  cases и 84 vertex cases без расхождений; fog1344/168 gradients/193536 pixels
  delta0; A8/R8 9184 cases delta0. Полный Windows Init/Draw не повторялся:
  original Windows provider/Init/resource contracts не менялись.
- COMPILE/LINK: UAM shader pair и полный devkitA64 native-entry ELF с исходными
  CGame::Init, map DrawShadows и recovery; 179 link inputs, 0 strong undefined.
- SWITCH HARDWARE: NOT RUN. Native Init, map/frame, stencil/depth attachments,
  output image, reset и FPS на консоли не подтверждены.

Профили: build/matrixgame/e2e531-stencil/{windows,host,arm64,sdk}/result.json,
materials/windows/result.json, fog/windows/result.json, windows-alpha/result.json.
stencil.golden: 2310×72 байта; shadow-scenes.bin: восемь полных HAL/GL color +
stencil buffers. Аудит независимо разбирает и сравнивает сохранённые bytes.
ELF SHA256: 8d4ca0cbc3a90c0ebbea0809a2dbc7cf8de4d8052406328777b33781151ecf08.
Финальный аудит: 15 защищённых артефактов и 107 аппаратных входных файлов
совпали с прежними fingerprints; upstream pin и его чистота сохранены.
Полные результаты: matrixgame-e2e531-stencil-evidence.json.

## Исправленная ошибка oracle

Первые scene fixtures расходились на краевых пикселях, хотя отдельные stencil
ops и CPU model совпадали. Причина — GL oracle использовал LowerLeft с flipped
readback, тогда как настоящий deko device уже использовал UpperLeft/ZeroToOne.
На tie edges эти conventions различаются. Oracle теперь использует те же
UpperLeft/ZeroToOne и top-left viewport/readback. Production geometry и stencil
операции ради PASS не менялись. Диагностика сохранена в edge-diagnostics.
Fog regression подтвердил прежние gradient результаты с уточнённым origin.
Спецификация: [Khronos glClipControl](https://github.com/KhronosGroup/OpenGL-Refpages/blob/main/gl4/glClipControl.xml).

## Где находится настоящая цепочка

Original Init/PrepareMap/DrawShadows/recovery действительно слинкованы. Typed
flat/stencil guards сняты, исходная shadow ветка не отключена и не заменена
пустыми handlers. 252 generated original файла не менялись.

Нового наблюдавшегося первого отказа native Init нет: Switch запуск запрещён,
а CPU/QEMU tests не являются выполнением Init через deko. Оставшиеся explicit
native guards включают point/line rasterization, scissor/depth bias, отдельные
sampling/render features и неканонические fog projections. По выполненному
source search depth-bias/scissor/wrap/sRGB/maxMip встречались только в списках
stored states, а не в активных setters MatrixGame/3G; это не доказательство
прохождения всего Draw. Нельзя назначить им первый runtime отказ без trace.

Следующий кодовый приоритет — оставшиеся реально вызываемые native paths и
полный original Takt/game loop вместо одноразового Init/Draw probe, затем
настоящая загрузка карты/первый кадр и управление боем. Полный map release/
recreate и восстановление после GPU queue failure также остаются открытыми.

NRO, frozen, ресурсы и сейвы сохраняются; commit/push нет. P0, GI repeating-text
causality, manual-publish ENOENT и исторические Switch hangs остаются OPEN.
