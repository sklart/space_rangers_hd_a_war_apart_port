# E2E-5.30 — original pixel fog / native raster convention

Итог: **PIXEL FOG/RASTER SOFTWARE PASS / FULL NATIVE INIT PARTIAL**.
Окончательный аудит всех шести profiles прошёл. 252 generated original файла,
15 защищённых входов и 107 аппаратных входов сохранились.
Evidence: reports/matrixgame-e2e530-fog-evidence.json.
SDK ELF SHA256: b77c7b2504c429dd0e7512cb5cd7d73a251c835f4f328f2bf99a26bfb2460c3b.

Fog ветка CMatrixMap::Draw подключена к native RasterCaps и настоящему fragment
shader. Общий native Init, первая карта/кадр и Switch reset остаются PARTIAL /
HARDWARE_PENDING. Итог software этапа принимает matrixgame_fog_audit.py.

## Что реализовано

Оригинальные SetRenderState и вычисление start/end в CMatrixMap::Draw сохранены:
FOG_NEAR_K=0.5, FOG_FAR_K=0.7, MAX_VIEW_DISTANCE=4000×(1+0.01×perc).
PixelFogCaps сообщает только реализованные TABLE/WFOG/ZFOG bits 0x00300100;
vertex/range fog не рекламируются. map_device::RasterCaps теперь вызывает
типизированный NativeMapQueries provider, DekoDevice проверяет наличие shader
pair. Windows путь по-прежнему читает настоящие D3D9 caps.

PreparePixelFog читает authoritative scalar state и текущую projection matrix.
Для canonical perspective [0,0,1,0] используется 1/gl_FragCoord.w, для affine
[0,0,0,1] — gl_FragCoord.z. Это относится и к XYZRHW. LINEAR, EXP, EXP2
вычисляются в fragment stage, с clamp фактора [0,1]. Fog меняет только RGB
после texture cascade, сохраняя alpha перед alpha test/framebuffer blending.
Vertex/manual fog, noncanonical projection и некорректные активные параметры
явно отказывают до GPU draw; disabled параметры можно хранить и восстанавливать.

FogUniforms — отдельный std140 block48 байт на fragment binding1, offset512
в уже существующем packet uniform allocation. Старые Material272/Vertex1328
ABI сохранены; новая allocation не нужна, прежний frame budget достаточен.
Packet удерживает immutable bytes до queue completion. Каждый пакет явно
получает enabled/disabled fog block, включая cursor/font overlays и diagnostic
callers, поэтому незаданный UBO не используется как случайное состояние.
Scalar CDeviceState и внешний COW lease восстанавливают fog точно.

Спецификация: [Microsoft Pixel Fog](https://learn.microsoft.com/en-us/windows/win32/direct3d9/pixel-fog),
[Fog Formulas](https://learn.microsoft.com/en-us/windows/win32/direct3d9/fog-formulas),
[Fog Parameters](https://learn.microsoft.com/en-us/windows/win32/direct3d9/fog-parameters).

## Воспроизведённый дефект viewport

Первый настоящий HAL/GLSL differential: 384 cases, 48 gradients. Постоянная
глубина совпала; 22 gradient cases не прошли, максимальное RGB отклонение60.
Baseline exe, runtime/build/result, goldens и точные oracle/shader snapshots
сохранены в build/matrixgame/e2e530-fog/baseline-windows.

HAL использует integer pixel centers D3D9, native rasterizer — half-integer
convention. Физический viewport не компенсировал это различие. Общий
LegacyRasterViewport теперь смещает physical top-left viewport на (+0.5,+0.5).
Его используют настоящий DekoDevice::Record и production-stage GL oracle.
Logical viewport, FVF decoding/ProcessVertices transforms, projection matrices,
queries и clear rectangles не меняются. Это исправление native raster path,
не изменение исходной геометрии ради совпадения теста.

Повтор тех же 384 cases дал mismatches0/max_delta0. Расширенный набор тоже
совпал побайтно. Изображения на Switch не наблюдались; причинный вывод относится
к воспроизведению HAL/GLSL и соответствующему source path.
Правила: [Microsoft D3D9 rasterization](https://learn.microsoft.com/en-us/windows/win32/direct3d9/rasterization-rules),
[mapping texels/pixels](https://learn.microsoft.com/en-us/windows/win32/direct3d9/directly-mapping-texels-to-pixels).
Проверен SDK source: [deko viewport setup](https://github.com/devkitPro/deko3d/blob/master/source/maxwell/gpu_3d_base.cpp).

## Что исполнялось

- WINDOWS HAL/GLSL: 1344 cases, 168 gradients, 193536 sampled pixels,
  mismatches0/max_delta0. Perspective/affine, XYZ/XYZRHW, все три формулы,
  исходный диапазон2000…2800 и Z0.5…0.7, near/far/clamp, alpha0/129/255,
  fog color alpha, alpha-test reject/pass, SRCALPHA blend, enabled/disabled.
  Используются оба настоящих production shader stages и настоящая геометрия.
- HOST ASan/UBSan: production fog/native state, 840 actual HAL goldens,
  максимальное CPU отклонение1 уровень (exp/quantization); alpha точна.
  Исходный CDeviceState render group restore и COW lease restore точны;
  logical viewport сохранён, typed RasterCaps dispatch и отказы проверены.
- ARM64/QEMU: те же original/production TUs, assertions и HAL goldens; PASS.
  Это CPU evidence, не Switch GPU.
- WINDOWS regression: 1476 material cases без расхождений и 84 fixed-vertex
  cases (84 GLSL feedback / 80 ProcessVertices). A8 HAL против production GLSL
  R8: 9184 cases, максимальная delta0. Повтор оправдан изменением fragment shader.
  Полный Windows Init/Draw из E2E-5.28 не повторялся: original Windows
  Init/provider/render/resource contracts не менялись; viewport fix native-only.
- COMPILE/LINK: UAM fragment shader и полный devkitA64 native-entry ELF с
  Init/PrepareMap/Draw/recovery, 179 link inputs, 0 strong undefined.
- SWITCH HARDWARE: NOT RUN. Init, GPU upload, карта, первый Present, reset,
  пиксельная точность и FPS на консоли не подтверждены.

Команды/exit codes/source hashes: build/matrixgame/e2e530-fog/{windows,host,
arm64,sdk}/result.json, materials/windows/result.json, windows-alpha/result.json.
840 HAL goldens находятся в windows/fog.golden. Inputs state runner
проверяются до/после исполнения; PASS не допускается при изменении исходников.

## Следующий исходный блокер

Прежний guard CMatrixMap::Draw → RasterCaps снят для DekoDevice, fog не
отключается ради PASS. Далее при stencil shadows original DrawShadows
запрашивает SHADEMODE=FLAT: **native flat shading not implemented**.
После него нужны StencilCaps и настоящие stencil operations/двухсторонний
pipeline. Это source-traced conditional ветка, не наблюдавшаяся первая ошибка
полного native Init. Реальная цепочка Init на Switch ещё не исполнялась.
Полный Takt/input/battle/main-game integration и восстановление после GPU
queue failure также остаются отдельными открытыми gates.

Upstream, основная NRO, frozen, ресурсы и сейвы сохраняются. Новую NRO не
создавали, commit/push не выполняли. P0/повторяющийся текст, причинная роль
GI cache, manual-publish ENOENT и исторические Switch hangs остаются OPEN.
