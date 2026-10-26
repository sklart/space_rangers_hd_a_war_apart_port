# E2E-5.21: alpha/null, ReadCaps и native draw

Исторический срез. [Продолжение: texture residency/backend и следующая граница](matrixgame-e2e521-residency.md)
заменяет сведения ниже об исходном отказе AvailableTextureBytes.

25.10.2026. **PARTIAL; Switch runtime NOT RUN.** Прежние 209 alpha/null отказов
закрыты в проверенной матрице оригинальных material callbacks. Реализованы
FVF lighting/TCI, настоящая пара deko shaders и native map/VO submission.
Безусловный исходный отказ `ReadCaps` снят. Первая загрузка карты на Switch
ещё не выполнена. [Машинные результаты, команды и SHA-256](matrixgame-e2e521-cascade-evidence.json).

## Сохранённая семантика каскада

Оригинальные callback bodies и Init/Draw algorithms сохранены. Подготовка
создаёт отдельный effective uniform packet; сырые registers и texture bindings
не меняются, `nullptr` остаётся `nullptr`.

- Null texture прекращает cascade, только когда активный явный RGB argument
  читает TEXTURE. Alpha-only чтение и implicit texture-alpha factor каскад
  сами по себе не прекращают.
- Для alpha-only null reads shader вычисляет texel `(1,1,1,1)` без обращения
  к sampler. Подставной white texture/resource не создаётся.
- Disabled alpha сохраняет alpha регистра назначения CURRENT либо TEMP.
  Последняя эффективная стадия пишет CURRENT независимо от сырого RESULTARG;
  предыдущая запись в TEMP не уничтожает CURRENT.
- Для оригинального gloss prefix с начальным disabled alpha и без diffuse RGB
  input используется начальная opaque alpha. Учитываются live DIFFUSE arguments
  и CURRENT первой стадии. Это проверенная совместимость с выбранным Windows
  reference, а не обещание одинакового поведения неопределённого D3D9 случая
  на всех драйверах: сочетание active RGB/disabled alpha имеет такую оговорку
  в [документации D3D9](https://learn.microsoft.com/en-us/windows/win32/direct3d9/texture-blending-operations-and-arguments).

## Что действительно исполнилось

| Проверка | Результат и граница доказательства |
| --- | --- |
| Windows D3D9 HAL material reference | 1476 сравнений; ноль отказов alpha/null и расхождений. Четыре набора texture/diffuse/factor, включая alpha 0/255 и black/white |
| Тот же fragment GLSL на Windows OpenGL GPU | 1476 сравнений с D3D9, ноль расхождений; настоящий std140 packet и восемь bindings |
| Vertex shader transform feedback | 84 CPU/GLSL случая; 80 из них независимо сверены с D3D9 ProcessVertices, включая TCI; четыре XYZRHW проверены CPU/GLSL |
| Host ASan/UBSan/leaks | CPU material/vertex и negative native routing fixtures PASS |
| ARM64/QEMU | Те же CPU/routing проверки PASS; GPU не исполнялся |
| devkitA64/UAM | Vertex/fragment DKSH, providers, buffers и tests compile/link PASS, strong undefined=0; runtime NOT RUN |
| CMake Windows/SDK material targets | Configure/build PASS |
| Полный оригинальный native Init/frame closure | Адресная SDK линковка PASS с прежними original archives и новыми providers/map dispatch; strong undefined=0; runtime NOT RUN |
| Windows оригинальные Init/frame/Deinit | Два полных цикла PASS после адресной пересборки map dispatch; по 1008 draws, 1006 indexed, 111812 primitives |

Windows reference: NVIDIA GeForce RTX 5060, D3D9 `nvldumd.dll`, OpenGL NVIDIA
617.42. Это controlled material/vertex comparisons; полноценная картинка всей
игры на Switch ими не проверена. Material и vertex reference cases исполнялись
отдельно. Negative routing fixture подтверждает достижение native методов
с отказом-сентинелом, не успешную GPU-отрисовку.

Windows регрессия использовала прежние оригинальные библиотеки, а не новую
чистую пересборку всего движка. Карта 276×285, группы 28×29, кадр 800×600,
154038 RGB-цветов. Оба снимка побитово совпали с E2E-5.20:
`f6f0d25c1e7b2a194f6523f8c96b9a74f7b7385525e5bb5cf7696de7f09a52bb`.
Reset refusal/release/retry и восстановление window/gamma/leases прошли.
Большие прежние наборы проверок повторно не запускались.

## Подключённый native путь

FVF decoder сохраняет position/normal/diffuse/specular и до восьми UV sets
с размерностью 1–4; поддерживает исходные XYZ/XYZRHW layouts. Матрицы сохраняют
row-vector соглашение оригинала. Реализованы directional lighting, ambient/
emissive/diffuse material sources, normal matrix/normalization, TCI passthrough,
camera position/normal/reflection и texture transforms COUNT2. Unsupported
blend positions/PSIZE, point/spot lights, specular lighting и projected texture
flags явно отклоняются; полного D3D9 lighting контракта это не доказывает.

`NativeGraphics` связывает исходные map primitive/UP/indexed и VO Emit
с retained CPU staging, проверкой индексов и настоящими deko upload/bind/draw
commands. Реализованы acquire/targets/depth/clear и submit/present/wait.
Per-draw uploads живут до queue completion; allocation arithmetic проверяется.
Фактическое исполнение этих новых GPU commands **NOT RUN**.

Настоящие vertex/fragment DKSH собираются UAM, проверяются и встраиваются;
device construction загружает оба shader objects. `ReadCaps` требует
`VertexReady()` и возвращает 8 stages/8 textures, anisotropy 16, один настоящий
swapchain, без calibrated gamma. Его исходный shader-gate закрыт и SDK-связан;
вызов на Switch не проверен. Эти caps не означают готовность texture residency
или полной отрисовки. Non-null borrowed texture явно отклоняется до реализации
оригинального texture upload/residency backend; COM identity не считается GPU image.

## Где остановилась настоящая интеграция

После успешного создания device/queue/images/swapchain и shader pair следующая
известная по исходникам цепочка:

```text
CGame::Init
  → original_game::EnterPlatform
  → NativeInitRuntime::EnterNativePlatform
  → platform3g::InitAsNative
  → DekoDevice::AvailableTextureBytes
      Unsupported("deko texture residency budget not negotiated")
```

Это исходная граница, не наблюдавшаяся аппаратно. На Switch возможен более
ранний отказ allocation; прежний alignment fix аппаратно ещё не принят.
Подмена доступной texture memory общим process RAM либо upload лимитом не внесена.

Для первой карты остаются настоящий residency/allocation budget и original
texture resource backend, capture/restore состояния и lifecycle, video/window/
font/cursor/input providers; далее — offscreen/raster/stencil/shadow integration
и фактический native frame. Сохранён оригинальный Init, отказ не обойдён.

## Сохранность и открытые дефекты

Итоговый аудит проверил 15 защищённых входов, 107 файлов hardware-комплекта,
исходные robots.pkg/CFG и frozen GI cache source; SHA-256 сохранены в evidence.
Основная NRO, frozen-комплекты, ресурсы и сейвы не изменялись. Новая NRO
не создавалась; commit и push не выполнялись.

P0 основной игры остаётся приоритетом и OPEN. Этот шаг не добавляет
воспроизведения GI-дефекта на ресурсе дефектного экрана и не устанавливает его
причинность для повторяющегося текста. Исторические `manual-publish ENOENT`
и зависания Switch остаются OPEN. Новых аппаратных проверок нет.

Воспроизведение адресных проверок: `tools/matrixgame_material_checks.py`
с профилями host/arm64/windows/sdk/cmake-windows/cmake-sdk и output
`build/matrixgame/e2e521/cascade`; SDK closure — `tools/matrixgame_cascade_link.py`,
Windows regression — `tools/matrixgame_cascade_windows.py`, итоговая сверка —
`tools/matrixgame_cascade_audit.py`. Точные исполненные команды находятся в JSON.
