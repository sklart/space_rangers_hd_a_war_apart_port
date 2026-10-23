# E2E-5.19 — original bottom texture, DXT1 и material ownership

24.10.2026, продолжение `78a3943`, локальный master, без push.

## Две воспроизведённые ошибки владения

Первый compile baseline полного MatrixMapTexture.cpp останавливался на
D3DFMT_DXT1. Typed overload был объявлен, но не входил в selected managed
implementation. После подключения этого пути общий bootstrap выявил
более ранний дефект: CACHE_CREATE_TEXTUREMANAGED создаёт portable **CacheTexture**,
а bottom owner обращается к нему как к оригинальному **CTextureManaged**.
UBSan зафиксировал invalid downcast с обоими реальными runtime class names.
Sanitizer не отключался. Bottom path теперь вызывает существующую фабрику
настоящего оригинального CTextureManaged; глобальная portable фабрика не менялась.

После исправления типа следующий baseline исполнил два настоящих отказа GPU:
registry1 после первого MakeFromBitmap, registry2 после второго, **registry1
после Clear**. Исходная повторная публикация m_Texture теряла прежний объект.
Теперь новая текстура имеет временного владельца и публикуется только после
успешной загрузки; исключение вызывает исходный CCache::Destroy. Прежний материал
заменяется и освобождается после готовности нового. В финальном общем bootstrap
оба отказа оставляют nullptr и registry0, повторный Clear и Init проходят.

## Настоящий исходный data path

В общий native-cache включён **полный MatrixMapTexture.cpp**: original Init,
Clear, ctor/dtor и MakeFromBitmap. Последний получил проверенный factory/owner
adapter и явный TexturePixelFormat с FourCC DXT1, ровно6 mip levels.
Он не попадает в bool-overload преобразования16bit; защита от случайного
SDK enum → bool по-прежнему действует.

Selected OriginalManagedTexture.cpp теперь содержит исходный formatted
LoadFromBitmap overload: **оригинальный CBitmap::SaveInDDSUncompressed →
настоящий D3DXCreateTextureFromFileInMemoryEx**, исходные width/height0, usage0,
запрошенные format/levels, managed pool, FILTER_NONE/FILTER_BOX и color-key0.
Изменены checking HRESULT и момент публикации: SDK candidate и descriptor
должны быть готовы до освобождения прежнего m_Tex/изменения размеров.
Та же адаптированная функция остаётся в полном Texture.cpp; остальная полная
Texture TU этим шагом не объявляется перенесённой. Поддержан production
USE_DX_MANAGED_TEXTURES путь, отдельная system-memory ветка не подтверждается.

Windows backend использует настоящую Microsoft D3DX9 DLL, уже подключённую
для шрифта. Отказ освобождает возможный частичный SDK output. Без SDK backend
выдаётся явный Unsupported. CPU-имитации DXT1 или подмены формата RGBA нет.

## Что реально проверено

- Общий host ASan/UBSan/leak-check и ARM64/QEMU bootstrap: exit0, два сеанса,
  по две original material попытки, отсутствие публикации при отказе, registry0
  после каждого отказа и Clear, repeated Clear/Init. Реальны game/cache heap,
  CBottomTextureUnion, CTextureManaged ctor/dtor, bitmap/DDS serialization и
  отказ настоящего platform boundary; успешная GPU-загрузка здесь не заявляется.
- **Windows D3D9 NULLREF runtime PASS:** SDK-free caller создаёт контрольный
  original CBitmap RGB32x32, сериализует его original методом; настоящий D3DX
  создаёт DXT1 texture. Все6 mip descriptors сверены по формату и размерам32→1.
  Проверены malformed DDS отказ с null output, отказ непустого output,
  сохранение предыдущего SDK texture и два Release/recreate цикла.
  Device ready получен настоящим InitAsDLL; borrowed state lease восстановлен.
  Использована app-local официальная D3DX DLL, системные DLL не заменялись.
- Это контролируемый bitmap fixture, **не коммерческая текстура и не проверка
  rendered pixels/физического GPU**. Сквозное исполнение Windows CBottom →
  CTextureManaged formatted overload → SDK ещё NOT RUN: полный Windows
  bootstrap остаётся отдельным незавершённым target. Доказательства CPU frontend
  и реального Windows SDK backend разделены.
- devkitA64: отдельный ELF COMPILE/LINK PASS, strong undefined=0,
  original bottom и formatted symbols сохранены. SHA-256
  `5be0a262991429fffd7e54f2fd6bb9aa1c4f600558d7523b19741efbdf96e51d`.
  Runtime на Switch HARDWARE PENDING, NRO не собиралась.
- MSVC x86: полный bottom TU, selected managed методы и реальный formatted
  backend COMPILE PASS. Fresh CMake configure/generate и фактические bottom
  recipes PASS, полной повторной сборки CMake не было.

Новых sanitizer исключений для bottom/managed не добавлялось; прежние Console
ASan globals и Logic/Map vptr ограничения сохраняются. Standalone CList,
BigVB/BigIB и RoadNetwork suites не повторялись.

## Оставшаяся граница

Настоящий CMatrixMapLogic ctor/dtor link теперь требует **17** уникальных symbols
вместо18: CBottomTextureUnion::Clear снят. Full map owner LINK FAIL; CGame::Init,
RunGameLoop, CFormMatrixGame и бой NOT EXECUTED. Failing списка и baselines
не скрыты. Другие original material consumers с g_Cache->Get/portable texture
casts ещё не унифицированы; исправлен именно bottom factory path. Территория,
объекты, effects/skins/hints и полная GPU/device lifecycle остаются незавершёнными.

Сверены все242 engine-manifest файла двух views и отдельно66 core outputs.
От принятого terrain view изменились только MatrixMapTexture.cpp,
OriginalManagedTexture.cpp и Texture.cpp. Источники pinned upstream не менялись.
[Evidence](matrixgame-e2e519-bottom-texture-evidence.json) содержит commands,
source/object/binary hashes, UBSan type failure, потерю владельца и финальные gates.
build_dirty=true — проверки выполнены до локального коммита.

Повторно совпали13 доступных protected inputs,107 аппаратных файлов, frozen
main NRO/ELF и оба ZIP. G:/pack.zip недоступен. Сейвы и ресурсы не менялись.
Эти MatrixGame дефекты не установлены как причина аппаратных проблем основной
игры: intro raster owner, два historical manual-publish ENOENT и причины
freeze карты/консоли при выходе остаются OPEN.
