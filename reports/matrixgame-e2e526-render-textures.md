# MatrixGame E2E-5.26 — native render textures и texture surfaces

Снят программный guard shadow CreateRenderTexture(A8): исходный shadow effect
создаёт настоящую texture в deko image arena, получает surface уровня и может
использовать её как render target и затем sampled texture. Полный native Init,
первая карта и кадр на Switch ещё НЕ исполнялись: PARTIAL / HARDWARE_PENDING.
Предыдущий этап original Text::Render/DeviceIdentity с Font.2Ranger сохранён;
его отдельные данные и результаты описаны в [E2E-5.24](matrixgame-e2e524-text.md).

## Реализация

- Render textures: usage=1, pool=DEFAULT, один mip, A8/BGRA8/XRGB8.
  Реальный layout UsageRender, residency allocation, upload и sampled descriptor.
  A8 хранится в R8; sampling swizzle возвращает (1,1,1,A). Замены на RGBA нет.
- Material GLSL сохраняет исходные cascade equations и routing. Флаг в свободном
  бите prepared uniforms направляет logical alpha в red; выходная alpha остаётся
  исходной для alpha test и blending. ABI uniforms остаётся 272 bytes.
- Для A8 logical ALPHA write mask переводится в физический RED; destination alpha
  blend factors используют destination red; alpha factor SrcAlphaSaturate равен 1.
  Clear пишет logical ARGB alpha в red. Raw original render/material state не меняется.
- Shadow CreateRenderTexture вызывает native texture factory; ошибки VRAM/heap
  возвращаются раздельно, отсутствующая GPU allocation не выдаётся за успех.
- Surface уровня текстуры удерживает original texture owner; повторный запрос
  возвращает тот же token с дополнительным reference. Default owners блокируют
  reset. System-memory и managed surfaces не считаются default GPU owners.
- GetSurfaceLevel не требует COM vtable. Валидация device/session, mip и формата;
  system surface имеет CPU storage, без фиктивного GPU resource.
- Imported surface разрешает текущую image version. Recorded GPU packets
  удерживают именно разрешённую image до queue completion, включая смену версии
  через UpdateTexture. Render target feedback явно отклоняется.
- Readback использует copyImageToBuffer с barrier/submit/flush/waitIdle и проверкой
  queue error. Данные destination публикуются после успешного чтения. Поверхностное
  копирование поддерживает одинаковые extent/format без фильтрации/конверсии.
  Несовместимые копии отклоняются, а не заполняются нулями.
- Texture/surface locks уровня взаимно исключаются. Render texture LockRect
  отклоняется; CPU доступ идёт через system readback. Managed writable surface
  обновляет CPU backing и публикует новую GPU image после успешного upload;
  при отказе lock сохраняется для retry. Другой locked mip не обходится.
- System -> default UpdateTexture поддерживает original restoration render texture.
  Потеря уже созданной RT image не вызывает скрытый upload старых CPU нулей:
  требуется original recreation/restore. Shutdown/Close завершают lifetime явно.
- Texture factories теперь соблюдают output-only D3D контракт и не читают
  неинициализированные original output locals; выход обнуляется до allocation.
  Это позволяет использовать исходные newTexture/loaded_tex/out миникарты.

## Что действительно исполнилось

| Gate | Результат и граница |
| --- | --- |
| HOST ASan/UBSan | Production texture/surface coordinator: allocation failures, canonical references, второй удерживающий owner, readback failure atomicity, locks/retry, RT invalidation, UpdateTexture restoration и managed writeback. PASS. Low-level transfer driver явно CPU model; GPU/Init не исполнялись. |
| ARM64/QEMU | Тот же адресный contract executable. PASS. Это ARM64 CPU, не Switch GPU. |
| WINDOWS HAL | Настоящие A8 texture target/system texture surface и D3D9 draw/readback против того же GLSL на R8: 9184 cases, maximum delta=0. Blend factors/operations, все write masks и alpha comparisons. NVIDIA RTX 5060. |
| WINDOWS material/FVF | Проверена совместимость общего GLSL oracle: 1476 D3D9/GLSL material comparisons без mismatches; 84 vertex cases/feedback, 80 D3D9 ProcessVertices. |
| COMPILE/LINK | Полный devkitA64 native-entry ELF, 178 link inputs, 0 strong undefined. Обновлённый fragment GLSL реально скомпилирован uam в DKSH. |
| WINDOWS full Init | Новый полный прогон не запускался: изменения production paths относятся к native backend; прежний полный Windows Init/frame/reset остаётся историческим доказательством E2E-5.25. |
| SWITCH HARDWARE | NOT RUN / HARDWARE_PENDING. Нативный GPU draw/upload/readback/Present и реальный Init пока не проверены. |

ELF: build/matrixgame/e2e526-render-textures/sdk/matrixgame-native-entry.elf

SHA256: 65278c98f78698a4f1dcd5f9ca5b8cfeeb65b27333250ed3b41a7304785e7ba2

Команды: tools/matrixgame_render_texture_checks.py render-host / render-arm64 / sdk;
tools/matrixgame_alpha_target_windows.py. Машинные результаты находятся в
build/matrixgame/e2e526-render-textures; строгий audit —
tools/matrixgame_render_texture_audit.py. Доказательства привязаны к hashes.
Подготовленные 252 original/generated sources совпадают с E2E-5.25:
оригинальные алгоритмы shadow, Init и миникарты не заменялись.

## Реальная цепочка и оставшиеся препятствия

CGame::Init -> RobotPreload -> stor.Load -> PrepareMap ->
CreatePoolDefaultResources(true) -> StaticPrepare2 -> projected shadow ->
CalcShadowTextureWOMat -> CreateRenderTexture(A8) / GetSurfaceLevel / original draw.
Backend этой ветки теперь подключён, но весь вызов Init на Switch не выполнялся.

Следующий явный source-traced guard после StaticPrepare2:
CreatePoolDefaultResources(true) -> CMinimap::RenderBackground ->
PathToOutputFiles(FOLDER_NAME_CACHE) -> output_paths::PersonalLocation:
**native output personal-folder mapping unavailable**.
Нужно определить отдельный native cache/output provider и корректную обработку
native path, не писать в игровые ресурсы или пользовательские сейвы.
Это не аппаратно наблюдавшийся первый отказ: более ранние GPU ошибки неизвестны.

Далее нужны exception-safe original minimap target/depth/viewport/scene cleanup,
настоящие terrain/water/objects submissions в этой цели и выполнение первой карты.
При release/reset/recreate остаётся точный guard:
CMatrixMap::ReleasePoolDefaultResources -> map_device::StateBackend ->
**map stored device state backend unavailable**. DekoDevice пока не реализует
original StoredStateBackend с его register/light/material capture/restore.
CaptureState/lease другого уровня не закрывает этот guard.

## Ограничения и независимые открытые дефекты

NRO не создавалась. Commit/push не выполнялись; upstream не изменялся.
Основная NRO, frozen-комплекты, robots.pkg/CFG, ресурсы, сейвы и аппаратные входы
проверяются hashes в отдельном evidence audit. Новых Switch hardware tests нет.

P0 основной игры, GI repeating-text causality, manual-publish ENOENT и исторические
Switch hangs остаются OPEN. Surface retained-owner тест не воспроизводит GI cache
проблемного экрана и не устанавливает причину повторяющегося текста.
