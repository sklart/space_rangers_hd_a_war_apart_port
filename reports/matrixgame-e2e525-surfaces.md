# MatrixGame E2E-5.25 — native offscreen surfaces и shadow cleanup

Программно подключены offscreen GetTarget/GetDepth/CreateTarget/CreateDepth,
bindings, descriptor, lock/readback и writeback для standalone color/depth
surfaces. Полный native Init/карта/кадр остаются PARTIAL / HARDWARE_PENDING.

## Реальные изменения

- native_surfaces.hpp/.cpp: зарегистрированные opaque surface tokens без COM
  vtable, точные AddRef/Release-подобные references, ownership/session validation.
  Получение target не требует лишнего acquire/Present. Output slots исходных
  D3D callers не читаются до записи; nullptr depth сохраняется.
- Surface удерживается текущими bindings, вторым control, snapshot и pending
  draw packets. Presentation images имеют shared backing; замена presentation
  не оставляет dangling images. Живые default owners блокируют Reset, включая
  C++ owner после удаления исходного token. Session Close снимает GPU ownership
  даже при удержании dead surface/CPU lock. Цикла Surface ↔ Session нет.
- DekoDevice: настоящие attachment bindings, lazy presentation acquire и
  самостоятельные offscreen command batches. EndScene + readback выполняют
  submit/flush/waitIdle до CPU доступа. Offscreen batch не делает Present.
  Native frame entry сохраняет игровой viewport. Переключение bindings
  удерживает targets до GPU completion; одинаковые bindings повторно не пишутся.
- CaptureState составлен из presentation lease, target/depth lease и исходного
  render/buffer state lease. После recreation primary роли разрешаются в новые
  images; target восстанавливается раньше точного viewport/render state.
- DekoTextureStore: настоящие UsageRender images в уже выделенном texture arena,
  с actual layout size/alignment и Residency. Standalone targets уменьшают
  AvailableTextureBytes. CopyImageToBuffer и CopyBufferToImage используют
  собственный transfer buffer, barriers, queue completion и проверку ошибок.
  Readback возвращает CPU bytes только после успешного GPU completion; ошибки
  не превращаются в пустой bitmap. Для primary RGBA применяется BGRA conversion.
- Поддержаны color ARGB8/XRGB8/RGB565 и depth Z24S8/Z24X8/Z16; MSAA и A8
  standalone target пока честно отклоняются. Lock только у lockable color
  surface. READONLY не пишет назад; изменённый writable lock выполняет реальный
  GPU writeback. При отказе writeback lock сохраняется для retry.
- Original shadow DrawIndexedStaging направлен в существующий проверяющий
  NativeDrawIndexed, без изменения индексов, диапазонов или original materials.

## Исправления original resource paths

В генерируемом OriginalUnmanagedTexture исходные локальные tex/sm_tex получили
nullptr initialization. Native texture factory требует пустой output slot,
который исходные D3D локальные переменные раньше не гарантировали.
CTexture::MakeSolidTexture сохранил исходные 2×2 pixels и system→default upload;
staging texture теперь освобождается при create/update OOM и исключениях.

Original CVectorObject::CalcShadowTextureWOMat сохраняет все вычисления и draw
operations, но временные surface/texture/managed owners защищены RAII.
Scene заканчивается при исключении; target/depth/viewport восстанавливаются.
Ошибка восстановления явно возвращается/логируется, не маскируется успехом.
Обязательные depth/readback/indexed calls проверяются и в release. Генератор
отказывает при изменении ожидаемого количества этих исходных вызовов.
Upstream не редактировался.

## Доказательства

| Gate | Что действительно исполнилось |
| --- | --- |
| HOST | ASan/UBSan и assertions: production surface coordinator/API, refs, viewport reset, retained owner, OOM/VRAM exhaustion, read/write failure и retry, binding lease/recreate/close; exact TargetScope/SceneScope unwinding. Низкий image/clear/transfer driver — явная CPU model, не GPU. |
| ARM64/QEMU | Те же production surface/scope paths и проверки владения/ошибок. Не Switch GPU. |
| ORIGINAL CTexture | Отдельные host ASan/UBSan и ARM64/QEMU: настоящий MakeSolidTexture, точные BGRA 2×2, default pool, create OOM, update OOM, staging cleanup, retry и destructor/registry balance. Upload — CPU model. |
| WINDOWS HAL | Два исходных CGame::Init → PrepareMap → original form Draw → Deinit с обновлёнными original unmanaged/shadow TUs и scope helper. 1008 draws, 1006 indexed, 111812 primitives на кадр; оба PNG совпали с прежним hash f6f0d25c1e7b2a194f6523f8c96b9a74f7b7385525e5bb5cf7696de7f09a52bb. Настоящие reset/restore прошли. |
| WINDOWS FORMAT | Настоящий D3DXCreateTexture с render-target usage: запрос A8(28) → A8(28), A8L8(51) → ARGB8(21), ARGB8(21) → ARGB8(21). Результат относится к проверенному Windows HAL. |
| CMake/CTest | Чистая host конфигурация нового native-surfaces target, ASan/UBSan/-UNDEBUG, один адресный CTest. SDK closure собирается через CMake. |
| COMPILE/LINK | Полная отдельная devkitA64 ELF содержит оригинальные Init/PrepareMap/shadow functions, новые surface scopes, allocation/bind/readback/writeback; strong undefined = 0. |
| SWITCH HARDWARE | NOT RUN / HARDWARE_PENDING. Нативные allocation, render, readback, Init, карта и Present на Switch не подтверждены. |

Все команды, exit codes, source/link hashes и длительности процессов сохранены
в build/matrixgame/e2e525-surfaces/*/result.json. ELF находится в
build/matrixgame/e2e525-surfaces/sdk/matrixgame-native-entry.elf.
SHA256: 74e97d2b280f7530be482e5713ede84b5d629b7623f6815729377721b4c5c1b2.

## Следующий исходный блокер

При многокадровых моделях и списках объектов вызывается
ShadowProjBuildFull / ShadowProjBuildTextureList →
CVectorObject::CalcShadowTexture[WOMat](..., tex_to_update) →
shadow_texture::CreateRenderTexture(..., usage=1, format=A8(28), pool=0).
Исключение native branch: **shadow render texture SDK unavailable**.

A8 требует настоящего alpha-only render path: вывод shader alpha в R8 storage,
правильные sampling swizzle и color write mask, соблюдение blending/alpha
semantics и retained texture image ownership. Проверенный Windows HAL реально
создаёт A8, поэтому простая RGBA подмена не обоснована. Эта часть ещё не закрыта.

Далее нужны native render texture/surface import для CTexture и миникарты,
system-memory surface copy/Readback для loss/cache paths и StoredStateBackend
в CMatrixMap::ReleasePoolDefaultResources. Общий offscreen::Readback пока
остаётся guard; реализованный standalone lock читает GPU через отдельный callback.
Static BGRA shadow route подключён программно, но весь original shadow method
на Switch не исполнялся. Порядок конкретного первого объекта зависит от карты;
полного native Init runtime trace нет.

## Сохранность

Новых NRO и аппаратных тестов Switch нет. Commit/push не выполнялись.
Финальный audit проверяет main/frozen NRO, saves, robots.pkg/CFG, frozen GI
source, 107 hardware inputs, clean upstream pin и неизменный parent HEAD.
Проверяется независимая generated-source reproducibility; изменены только
OriginalUnmanagedTexture.cpp, Texture.cpp и OriginalShadowTexture.cpp overlay.

P0 repeating text, причинность GI cache, manual-publish ENOENT и исторические
Switch hangs остаются OPEN. Новые surface ownership tests не доказывают
применимость или причинность дефекта общего GI cache к повторяющемуся тексту.
