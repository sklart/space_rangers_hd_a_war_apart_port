# E2E-5.21: texture residency budget и native texture backend

Исторический срез. [Продолжение CaptureState/restore](matrixgame-e2e521-state.md)
заменяет приведённые ниже сведения об исходном отказе CaptureState.

25.10.2026. **PARTIAL; Switch runtime NOT RUN.** Реализованы настоящая
image-capable arena, её allocator и texture upload/sampling backend.
`DekoDevice::AvailableTextureBytes` больше не содержит безусловного отказа.
Первый известный следующий блокер исходного Init — `Device::CaptureState`.
Первой загрузки карты на Switch пока нет.
[Команды, результаты и SHA-256](matrixgame-e2e521-residency-evidence.json).

## Бюджет, обеспеченный памятью

`DekoTextureStore` сначала создаёт настоящий `DkMemBlock` с флагами
`GpuCached | Image`. Только после успешного создания backing доступен budget.
Конструктор `DekoDevice` принимает размер arena; default request — 64 MiB.
Это размер запрашиваемого собственного backing, а не оценка свободной памяти
консоли. Отказ создания не заменяется успешным caps-ответом или нулевой текстурой.

Каждая image использует фактические `ImageLayout::getSize/getAlignment` SDK.
Allocator учитывает padding, деление/слияние диапазонов и освобождение конкретной
allocation. `AvailableTextureBytes` возвращает самый большой свободный
непрерывный диапазон; сумма свободных диапазонов хранится отдельно.
Процессная RAM, swapchain, command buffers и frame-upload limit в это число
не входят. Arena не растёт автоматически; окончательный размер для карты
потребует аппаратного измерения. Принудительная eviction не реализована.

Использование image backing сверено с
[кодом deko3d MemBlock](https://github.com/devkitPro/deko3d/blob/master/source/dk_memblock.cpp),
а mip transfers и compressed block extents — с
[реализацией deko3d image copy](https://github.com/devkitPro/deko3d/blob/master/source/dk_image.cpp).
Установленные SDK headers захэшированы в evidence. Это проверка контракта,
не аппаратное доказательство правильного sampling.

## Оригинальные ресурсы и владение

Сохранены исходные CTexture/CTextureManaged methods, material callbacks и Init/
Draw algorithms. Существующий native `TextureAsset` декодирует оригинальные
PNG/JPEG/DDS/STRG и готовит исходную mip policy. Native bridge подключает
Create/Decode/Describe/Lock/Unlock/Preload/Priority/Release к реальному ресурсу.
Поддержаны A8R8G8B8/X8R8G8B8, R5G6B5, X1R5G5B5/A1R5G5B5/A4R4G4B4,
A8/L8/A8L8 и DXT1/3/5; supplied BC blocks остаются compressed.
Форматные swizzles учитывают исходные BGRA/alpha/luminance соглашения.

Через старые pointer fields передаются зарегистрированные opaque native tokens.
У них нет COM vtable; они не вызываются как IDirect3DTexture9 и не считаются
GPU image по значению указателя. Незарегистрированная identity отклоняется.
SystemMemory texture хранит CPU levels и не допускается к sampling. Managed
и обычные Device textures получают настоящую image. `nullptr` по-прежнему
очищает binding; подставная белая текстура не создаётся.

Binding удерживает свой ресурс независимо от original owner. Каждый записанный
draw хранит конкретную shared image version и отдельный image descriptor packet
до queue completion. Изменение CPU levels создаёт новую image version; старый
кадр продолжает удерживать старую. Ошибка upload не заменяет прежнюю image или
binding. Release original handle не освобождает image, которую ещё держит
другой владелец либо кадр. Device shutdown после waitIdle снимает GPU ownership
и отключает дальнейший upload через старую session.

Upload использует настоящий staging block, отдельный transfer command buffer,
copyBufferToImage для каждого mip, barriers/invalidation и завершение queue перед
освобождением staging. Освобождение диапазона arena не выделяет heap memory,
включая OOM rollback. Create/Decode возвращают исходным вызывающим методам
`E_OUTOFMEMORY` либо `D3DERR_OUTOFVIDEOMEMORY`; незакрытые операции явно отказывают.
`unmanaged_texture::Describe/Dirty/Update` также подключены; Update публикует
новые CPU levels/image только после успешной загрузки.

Это не полный D3DX/resource contract: render/depth/autogen usages, surfaces,
GPU readback, shadow offscreen, некоторые formatted conversions (RGB→DXT),
исходный TRIANGLE/DITHER bitmap path и неподдержанные lock flags ещё отклоняются.
Их исходные алгоритмы не заменены упрощённым PASS.

## Что исполнилось

| Проверка | Результат |
| --- | --- |
| Host ASan/UBSan/leaks | PASS: 4000 allocator шагов, 12 форматов, 8 decode inputs; bounds, fragmentation/coalescing, allocation failure, rollback и совместное владение |
| ARM64/QEMU | Те же CPU проверки PASS, включая original bridge HRESULT при CPU/GPU OOM |
| Windows x86 | CPU resource/byte/lifetime проверки PASS; настоящие Windows ветви legacy/unmanaged bridge compile PASS |
| devkitA64 | Новые store/texture bridge/provider/probe compile/link PASS с сохранёнными оригинальными Init/frame archives; strong undefined=0 |
| CMake SDK material target | Configure/build с texture_store PASS |
| Switch | Arena allocation, transfer, sampling, AvailableTextureBytes invocation, Init/map/frame — NOT RUN |

Четыре controlled DDS inputs проверяют RGBA и BC1/2/3, остальные четыре —
настоящие DDS/PNG/JPEG/STRG entries из robots.pkg; имена/хэши в JSON.
Проверены alpha и все mip bytes после BGRA conversion без изменения input bytes.
CPU tests используют явно обозначенный upload model для проверки владения и
ошибок. Он не представляет успешную GPU allocation или native Init.
Полный Windows Init/frame suite не повторялся: Windows execution branches
этого backend не менялись. Новая чистая сборка всего native-engine через CMake
не заявляется; полный SDK closure проверен адресной линковкой новых objects
с прежними original archives и хэшами всех link inputs.

## Следующая точная граница

После успешного настоящего создания device/swapchain/shaders/texture backing:

```text
CGame::Init
 → original_game::EnterPlatform
 → NativeInitRuntime::EnterNativePlatform
 → NativeInitRuntime::AttachDeviceImpl
     platform3g::InitAsNative: ReadCaps / AvailableTextureBytes
 → Device::CaptureState
     Unsupported("backend state capture unsupported")
```

`DekoDevice` пока не переопределяет CaptureState. Это первая известная
исходная граница после budget; её достижение на Switch не наблюдалось.
Ранее возможны аппаратные ошибки allocation/alignment — прежний fix ещё
не принят на устройстве. Original Init не обойдён и готовая карта не заявлена.

Далее нужны native state capture/restore и original sample-state backend,
video/window/font/cursor/input providers, surfaces/offscreen/shadows и
требуемые format paths. После этого — аппаратные allocation/upload/sampling/
lifetime checks, настоящий original Init/PrepareMap и первый кадр карты.

## Сохранность и открытые приоритеты

Аудит сверяет 15 protected inputs, 107 hardware-файлов, robots.pkg/CFG и
frozen GI cache source с прежними SHA-256. Основная NRO, frozen-комплекты,
ресурсы и сейвы не изменены; новая NRO не создавалась, commit/push нет.
Дополнительно сверены NRO/ELF frozen E2E-5.10 и корневая cumulative NRO с их
документированными хэшами. Для production NRO записан текущий fingerprint;
сравнение с отсутствующим в этом шаге предварительным fingerprint не заявляется.
Старый недоступный `G:\pack.zip` остаётся UNAVAILABLE.

P0 основной игры остаётся OPEN. Проверка удержания texture resource несколькими
владельцами здесь не воспроизводит дефект общего GI-кэша на ресурсе проблемного
экрана и не устанавливает причину повторяющегося текста. `manual-publish ENOENT`
и зависания Switch остаются OPEN. Новых аппаратных проверок нет.

Адресные команды: `tools/matrixgame_residency_checks.py host|arm64|sdk`,
`tools/matrixgame_residency_windows.py`, `tools/matrixgame_residency_audit.py`.
Артефакты изолированы в `build/matrixgame/e2e521/residency`; exact commands и
ограничения сохранены в evidence JSON.
