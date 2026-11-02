# E2E-5.61 — native CTexture::LoadFromBitmap

26.10.2026. **SOFTWARE DEPENDENCY PASS; FULL NATIVE INIT NOT VERIFIED.**

Оригинальный одноаргументный `CTexture::LoadFromBitmap(CBitmap&)` теперь имеет
настоящий native decoder вместо `unmanaged bitmap SDK decode unavailable`.
Используются исходные `Unload`, `CBuf`, `CBitmap::SaveInDDSUncompressed`,
оригинальный вызов backend и штатный native upload. Игровые алгоритмы и данные
не заменялись. [Evidence](matrixgame-e2e561-unmanaged-bitmap-evidence.json).

## Реализация и владение

`unmanaged_bitmap_filter.hpp` принимает конкретный DDS-протокол исходного
serializer: RGB24 или RGBA32, tight BGR/BGRA payload. Заголовок, размеры,
каналы и полный размер проверяются до декодирования. Native TextureAsset
выполняет настоящее чтение пикселей; при необходимости размер увеличивается
до степени двойки integrated triangle-фильтром с wrap и ordered 4×4 dither.
При совпадающем POT-размере действует копирование без фильтрации. Затем
существующий BOX backend строит mip-цепочку. RGB24 получает X8R8G8B8 (22),
RGBA32 — A8R8G8B8 (21); pool DEFAULT (0), usage 0.

Это отдельная политика от managed FILTER_NONE loader. Общая публикация
подготовленного TextureAsset вынесена из прежнего Decode в private Publish;
операции конверсии, RAII rollback и upload сохранены. Кандидат сначала создаёт
CPU mip storage, затем получает окончательные данные и единственный upload.
Выходной handle присваивается после успеха; исключение освобождает кандидата.
`ResidencyExhausted` и `bad_alloc` переводятся в соответствующие DeviceResult.
Проверяются предел 8192 и бюджет временных данных 64 MiB.

`CTexture::Unload` в оригинале сбрасывает TF_NOMIPMAP до выбора числа уровней.
Поведение сохранено и явно проверено; исправление этой upstream-семантики в
данный этап не входит. Дополнительно backend проверен с requested_levels=1.

Формула коэффициентов адаптирована из Microsoft DirectXTex, Git blob
`c41af51d49c728c9d04e4db15e3f80803a4ba30e`; лицензия и attribution сохранены
в `port/matrixgame/native-engine/unmanaged_bitmap_filter.NOTICE`.
[Исходный алгоритм](https://github.com/microsoft/DirectXTex/blob/main/DirectXTex/filters.h).
[Документация фильтров D3DX](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dx-filter).

## Проверки

| Gate | Результат и точная область |
| --- | --- |
| Windows HAL reference | PASS. Реальные D3DXCreateTextureFromFileInMemoryEx с исходными TRIANGLE+DITHER, BOX, UNKNOWN format. DEFAULT allocation/descriptors сопоставлены с SYSTEMMEM-запросом; пиксели экспортированы именно из SYSTEMMEM. GPU-пиксели DEFAULT не читались, Draw/Present не выполнялись. |
| HOST ASan/UBSan | PASS. 156 случаев, 390 mip-уровней, 85 104 channel bytes; настоящие CBitmap/CBuf/CTexture и native decoder. Конечная upload-функция — явно обозначенный CPU recorder, GPU caps не имитируются. |
| ARM64/QEMU | PASS. Те же 156 случаев/390 уровней; итоговые пиксели побайтно совпали с host. Это Linux ARM64, не libnx runtime. |
| Windows pixel differential | Дескрипторы, форматы, размеры и число mip-уровней совпали строго. Полностью совпали 148 случаев. Остальные содержат суммарно 12 channel differences, максимум 1/255; это предел проверенного набора, не доказанная глобальная оценка. POT copy и constant cases проверяются строго без допуска. Побитовая D3DX-совместимость resampling НЕ заявляется. |
| Ownership/failure | PASS. Реальный serializer совпал с DDS fixture побайтно; проверены unload/destructor, повторная загрузка, upload OOM → null handle → retry, registry cleanup, malformed header/truncated payload без upload. |
| devkitA64/libnx/deko3d | PASS. Отдельный полный CMake build штатной цели NRO, strong undefined = 0. Новый decoder и оригинальный одноаргументный метод определены в native-cache archive. |

В итоговом ELF новый одноаргументный decoder удалён linker GC: из текущей
диагностической Init-цепочки он пока недостижим. Следовательно, его реализация
закрывает программную зависимость, но не является доказательством продвижения
фактического native Init. Другие reachable overloads сохранены.

Общий SHA-256 CPU observations:
`93f1124f45029bf97c719a65ae1e579963ce3ca2653c1888c1ddc2f6c66e0419`.

Первый host build выявил ошибку объявления типов в новом тесте; исправлена,
первый log сохранён как `host/build-attempt1.log`. Финальные host/ARM build и
runtime прошли. Предупреждения deprecated wstring_convert относятся к прежнему
config_unicode.hpp; sanitizer errors не обнаружены.

## Отдельная воспроизводимая сборка

`build/matrixgame/e2e561-unmanaged-bitmap/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro`

Размер: **6 965 445 байт**. SHA-256:
`c7217b96fa50bc2d2b18d595e2d0f778852ee9a2bafad20c3111fb506d9b2c4f`.

Структура NRO, три сегмента, встроенные icon/NACP и отсутствие обязательного
RomFS проверены. Configure/build commands, hashes входов и ELF записаны в
`build/matrixgame/e2e561-unmanaged-bitmap/sdk/result.json`.

Из корня текущего подготовленного workspace:

```powershell
C:/Users/Sklyarov/AppData/Local/Programs/Python/Python313/python.exe tools/matrixgame_unmanaged_bitmap_reference.py --output build/matrixgame/e2e561-unmanaged-bitmap/expanded
C:/Users/Sklyarov/AppData/Local/Programs/Python/Python313/python.exe tools/matrixgame_unmanaged_bitmap_checks.py host
C:/Users/Sklyarov/AppData/Local/Programs/Python/Python313/python.exe tools/matrixgame_unmanaged_bitmap_checks.py arm64
C:/Users/Sklyarov/AppData/Local/Programs/Python/Python313/python.exe tools/matrixgame_unmanaged_bitmap_sdk.py
```

SDK script вызывает стандартную цель `matrixgame-native-diagnostic-nro`
(`nacptool`, `elf2nro`) в отдельном build-каталоге. CPU checks переиспользуют
проверенные библиотеки E559 и оригинальные owner dependencies E519; все
переиспользованные archives перечислены с SHA. Это воспроизводимость текущего
локального состояния с указанными prerequisites, а не чистого upstream checkout.

## Достигнутая Init-цепочка и оставшиеся компоненты

* Последний реальный Switch run из `logs2.7z`: package/config/cache/platform
  attachment → `CGame::Init → g_Config.ReadParams → ApplyGammaRamp`, затем
  `backend calibrated gamma submission unsupported`. Карта ещё не создана.
* E558 реализовал gamma storage и настоящий presentation LUT pass; CPU/GL/SDK
  checks сохранены. Аппаратного подтверждения gamma candidate пока нет.
* E559 отдельно исполнил полный оригинальный ReadParams с настоящими parser,
  heaps и data.txt, включая labels/descriptions и repeated Clear, на host,
  ARM64/QEMU и Win32. Это компонентное доказательство, не полный native Init.
* Следующие исходные шаги: ApplySettings → sampler ApplySettings (scalar cap
  clamp) → camera distance → CMatrixMapLogic constructor → LoadSide/interface
  owner → RobotPreload → stor.Load → PrepareMap → interface/minimap/final
  resources → original loop. Их полное исполнение на native GPU не подтверждено.
* StoredStateBackend уже подключён: 103 render registers, 13 sampler registers
  на каждом из 8 slots, lights/enables/material. E529 host/ARM64/Windows/SDK
  сохранены и проверены по saved results; большие тесты не повторялись. Scalar
  storage не означает поддержку произвольной GPU-семантики каждого значения.
* Ещё не реализованы некоторые режимы renderer: ненулевой MSAA/resolve,
  scissor, point rasterization, wireframe, отдельные vertex blend/clip/WRAP/
  sRGB/advanced sampler modes, autogen/depth texture creation и отдельные lock
  режимы. Они явно отвергаются; их обязательность именно для первого Init
  этим этапом не установлена. Нельзя объявлять любой из них следующим
  воспроизведённым блокером без фактического вызова.
* Открыты native PrepareMap/первый map frame, аппаратные gamma/image/VRAM,
  устройство после queue loss, полноценная игровая сцена и дальнейшая игра.
  Следующий фактический runtime failure пока неизвестен. Отсутствие Switch
  ограничивает аппаратную проверку, а не дальнейшую офлайн-разработку.

E527 output-folder provider остаётся с реальным CFile, отдельным output root и
cleanup. Его четыре прежних profile results, как и E529, прочитаны без повторного
большого прогона; source drift относительно исторических запусков указан в
evidence. Upstream pin/clean status, frozen/resource/save fingerprints, основная
NRO, E556/E558 NRO, прежние dist NRO и пользовательский logs2.7z сверены.
Изменений защищённых данных, commit/push и взаимодействия с устройством нет.
