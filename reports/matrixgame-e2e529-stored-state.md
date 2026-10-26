# E2E-5.29 — original CDeviceState / native stored state

Итог: **STORED SCALAR STATE SOFTWARE PASS / FULL NATIVE INIT PARTIAL**.
Окончательные HOST, ARM64/QEMU, Windows CPU, Windows HAL и SDK проверки
прошли аудит. 252 generated original файла неизменны; 15 защищённых входов
и 107 аппаратных входов совпали по hashes.
Evidence: reports/matrixgame-e2e529-stored-state-evidence.json.
SDK: 179 link inputs, 0 strong undefined; ELF SHA256
4305245294cde162be6ce37b8c5203f4e1ecdc8796d5359aee6ff1d97f15bc49.

StoredStateBackend подключён к настоящему DekoDevice. Завершение software
этапа принимает только tools/matrixgame_stored_state_audit.py после успешных
HOST, ARM64/QEMU, Windows CPU, Windows HAL и SDK профилей.
Полный native Init, карта и первый кадр на Switch остаются PARTIAL / NOT RUN.

## Реальная реализация

Сохранён исходный CDeviceState::StoreState/RestoreState: 103 render ordinals,
13 sampler ordinals ×8, 8 lights/enabled, полный material. Таблица
native_state_layout.hpp сопоставляет original ordinals с настоящими numeric IDs;
116 static_assert проверяют имена/порядок generated original enums.
Аудит отдельно сравнивает IDs с локальным официальным d3d9types.h.
Ordinal1 FILLMODE соответствует numeric8, а не numeric1.

Normal DekoDevice setters и stored adapter используют один NativeRenderState.
Getters читают реальные raster/vertex/combiner/sampler values; это важно и для
прямых изменений shader state, которые не должны читаться из устаревшего mirror.
Остальные scalar параметры хранятся в принадлежащей состоянию конфигурации.
Начальные значения описывают именно уже существовавший native pipeline,
включая lighting=off, SRCALPHA/INVSRCALPHA, alpha reference8 и MSAA=off;
они не выдаются за обнаруженные defaults настоящего Windows HAL.

Параметры выключенного fog/stencil, неиспользуемых MRT и patch/displacement
возможностей сохраняются для точного scalar restore. Это не реализация их GPU
отрисовки. Перед draw/clear проверяется применимость: активные fog, stencil,
scissor, depth bias, wrap, изменённая multisample mask, separate alpha blending, sRGB/maxMip/displacement
sampling и другие неподдержанные запросы явно отказывают до GPU-команд.
Нативная point/line rasterization с неподтверждёнными size/LASTPIXEL semantics
также явно отказывает. MSAA, flat/specular lighting остаются явно unsupported.
Поддержанные исходные triangle/material/FVF пути продолжают реально рисовать.

FixedVertexState теперь хранит исходный material целиком, включая Specular и
Power, и отдаёт полные raw lights с их неиспользуемыми directional-параметрами.
GPU VertexUniforms layout не менялся: 1328 байт.

Scalar snapshot не удерживает textures, surfaces, VB/IB и не создаёт внешний
state lease. Original CMatrixMap владеет CDeviceState через HNew(g_MatrixHeap),
сохраняет его в ReleasePoolDefaultResources, восстанавливает и удаляет в
CreatePoolDefaultResources; destructor карты также удаляет владельца.
ResetPrepared освобождает текущие bindings, а original restore возвращает
только предусмотренные исходным алгоритмом группы, без подмены transforms/FVF.
Правило original Type=0 сохранено: absent light при restore отключается, но
новое определение, появившееся после capture, не стирается.
Отказ recapture инвалидирует только запрошенную группу, как в оригинале.

## Native recovery entry

В исходном CFormMatrixGame::Takt первым вызывается CheckLostDevice.
Одноразовый native probe ранее вызывал Init/Draw без этой precondition, поэтому
линкер удалял ReleasePoolDefaultResources/StoreState. Теперь probe выполняет
настоящий g_MatrixMap->CheckLostDevice до BeginNativeFrame/acquire.
В ELF реально сохранены CheckLostDevice, ReleasePoolDefaultResources,
CDeviceState::StoreState и RestoreState. Сам probe по-прежнему не является
полным Takt/game loop и не доказывает Switch reset runtime.

## Проверки и границы

- HOST ASan/UBSan: исходный CDeviceState, все 103/13×8 values, полный raw
  lights/material, 1024 ARGB cases, 3 capture/reset/restore цикла и точное
  совпадение shader uniforms. Default texture и staging VB освобождаются при
  живом scalar snapshot. Bounds/null/invalid-state и failed recapture проверены.
- ARM64/QEMU: те же production/original TUs с assertions; не Switch GPU.
- HOST/ARM64 leases: прежние production COW/retain/restore/OOM тесты расширенного
  состояния: 9 restores, 2 capture OOM, 1 COW OOM, restore без allocations.
- WINDOWS CPU: исходный CDeviceState/native adapter, те же scalar/shader checks.
  Это отдельная категория доказательства, не D3D HAL.
- WINDOWS HAL: 1476 D3D9/GLSL material cases без расхождений, 84 fixed-vertex
  cases (84 GLSL feedback, 80 D3D9 ProcessVertices). Полный Windows Init/frame
  из E2E-5.28 не повторяется: original Windows provider и общие Init/Draw/
  resource API contracts не менялись; новые guards относятся к native renderer.
- COMPILE/LINK: отдельный полный devkitA64 native-entry ELF с исходным
  recovery/reset/store path; strong undefined отсутствуют. NRO не создаётся.
- SWITCH HARDWARE: HARDWARE_PENDING. Init/map/frame/battle/reset не исполнялись.

Результаты: build/matrixgame/e2e529-stored-state/{host,arm64,windows,sdk}/result.json
и materials/windows/result.json. Source hashes проверяются до/после каждого
state run; изменения входов во время проверки не могут дать PASS.
Начальный scalar compile выявил неоднозначность SetSamplerState между Device
и stored interface; добавлены явные using declarations. Начальная SDK-проверка
выявила удаление release ветки, после чего подключена реальная precondition,
а не принудительное удержание символов ради PASS.

## Следующая настоящая граница

Прежний guard **map stored device state backend unavailable** снят для
DekoDevice; complete scalar Store/Restore исполнялся на CPU/QEMU.
Полный CMatrixMap release/recreate на Switch ещё не исполнялся.

Source-traced следующий участок первого кадра при MMFLAG_FOG_ON:
CMatrixMap::Draw → map_device::RasterCaps → **map raster caps backend unavailable**.
Нужны настоящие fog capabilities/coordinate convention и оригинальная fog
формула в native shader, не выключение fog ради кадра.
При stencil shadows: CMatrixMap::DrawShadows сначала запрашивает SHADEMODE=FLAT
(native flat shading not implemented), затем StencilCaps и stencil pipeline.
Выбор фактической первой ошибки зависит от карты/CFG и остаётся NOT OBSERVED
без разрешённого native execution. Никакая из этих веток не объявлена готовой
по наличию scalar storage или успешной линковке.

Upstream, основная NRO, frozen, ресурсы и сейвы сохраняются. Commit/push нет.
P0, причинная роль GI cache в повторяющемся тексте, manual-publish ENOENT и
исторические Switch hangs остаются OPEN.
