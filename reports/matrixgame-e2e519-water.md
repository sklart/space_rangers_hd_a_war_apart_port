# E2E-5.19 — полный water TU и типизированный именованный SDK-кэш

24.10.2026, продолжение `569f9b9`, локальный master, без push.

## Новый исполняемый исходный код

Полный MatrixWater.cpp включён в общий native-cache. Сохранены original
CMatrixWater ctor/Clear/dtor/Init/BeforeDraw/Draw/Takt и SInshorewave
Create/PrepareVB/Release/DrawBegin/DrawEnd/Draw. Волновая математика, порядок
вершин и RNG constructor не переписаны. SDK границы направлены в существующие
typed render/sampler/texture/buffer/map adapters. В header добавлен только
отсутствующий forward declaration CTextureManaged, layout не менялся.

На host и ARM64/QEMU общий bootstrap реально исполняет:

- original CMatrixWater ctor, повторный Clear и dtor. В публичные owned slots
  помещаются настоящие staging VB/IB из production Context; после Clear/dtor
  AllocatedBytes возвращается к исходному значению в двух циклах;
- **original SInshorewave::PrepareVB**: создаёт и заполняет настоящий staging
  VB четырьмя вершинами. Все XYZ/UV сверены с исходным квадом; initialized range,
  unlock, повтор без перевыделения, двойной MarkAllBuffersNoNeed и новое создание
  проверены. Это исполнение новой original geometry routine в общем bootstrap,
  не повторение standalone BigVB/BigIB suites.

Water VB/IB здесь — явно заданный ownership fixture, **не исполнение Init и
не GPU allocations**. Water map-dependent Init/Takt/Draw, создание/Release
активных inshore waves и GPU upload/render NOT RUN. Настоящий g_MatrixMap
остаётся nullptr. Оригинальные constructor RNG calls исполняются; не заявляется
эквивалентность состояния запущенного мира, которого в тесте ещё нет.

## Настоящий typed named texture path

Water раньше предполагал, что любой CacheClass::TextureManaged — оригинальный
CTextureManaged. Portable фабрика создаёт другой CacheTexture, как уже доказано
предыдущим UBSan baseline. Новый legacy_texture::GetManaged сохраняет алгоритм
original CCache::Get: **Find → create → exact Name → Add**, без eager Load.

Создаётся настоящий original CTextureManaged; он принадлежит реальному g_Cache.
Повторный lookup возвращает тот же объект. Если существующая запись — portable
тип, запрос явно отклоняется с сохранением прежней записи. Unsafe downcast,
вымышленный SDK object и изменение глобальной portable фабрики не применялись.
Совмещение двух representations под одним ключом ещё не реализовано.

В том же общем bootstrap проверены cold create, lazy state, Name/m_Cache,
повторный lookup, original Up, корректное удаление и отказ конфликта типов без
замены portable entry. Последний настоящий named SDK object оставлен родителю;
после original CacheDeinit общий cold checkpoint требует RegistrySize()==0.
Это проверено в обоих сеансах host и ARM64, без ручного удаления последнего
объекта тестом.

Первая попытка теста вызвала ASan use-after-free: attached entry был передан
в Destroy без Delete. Это **ошибка нового fixture**, а не найденный дефект
Cache::Find. Original Clear использует Delete→Destroy, базовый destructor
не удаляет membership. Исправлен тест и явно описан borrowed API contract;
производственные CacheNative.cpp/Cache.hpp не менялись. Failed log сохранён,
его нельзя выдавать за исправленную ошибку исходного движка.

## SDK и Windows границы

Water texture priority направлен в настоящий IDirect3DTexture9::SetPriority
с сохранением DWORD return. Windows NULLREF проверка наблюдает приоритет
0xffffffff и его возврат при сбросе0 на настоящем managed DXT1 texture.
Прежний original bitmap→DDS→D3DX/six-mip контракт также проходит. Это реальный
SDK resource runtime, **не water rendering или physical GPU pixel proof**.
Named cache/Water Windows runtime в полном game bootstrap ещё NOT RUN.

- Общий host ASan/UBSan/leak-check и ARM64/QEMU: exit0, два сеанса water/quad/
  named cache и parent cleanup. Новых sanitizer исключений не добавлялось;
  прежние Console globals и Logic/Map vptr ограничения сохранены.
- devkitA64: отдельный ELF COMPILE/LINK PASS, strong undefined=0;
  original water/GetManaged/PrepareVB symbols retained. SHA-256
  `c4447ff23609a814e68347e1a36455bf12ad4c6664a898801d623111c07b3abf`.
  Switch execution HARDWARE PENDING, NRO не создавалась.
- MSVC x86: полный Water, selected original managed и настоящий SDK texture
  backend COMPILE PASS. Windows priority/DXT1 resource runtime PASS отдельно.
- Fresh CMake configure/generate PASS, реальные water recipes проверены.
  Полная повторная сборка CMake не выполнялась. Завершённые CList, BigVB/IB,
  RoadNetwork suites не повторялись.

## Точная оставшаяся граница

Настоящий CMatrixMapLogic ctor/dtor link требует **16** unique missing symbols
вместо17: destructor CMatrixWater снят. World objects, minimap/effects/groups/
skins/hints/UI cleanup остаются. Full owner LINK FAIL; CGame::Init, RunGameLoop,
CFormMatrixGame и бой NOT EXECUTED. Staging upload/binding и другие SDK/device
ограничения не скрываются successful draw. Старые material consumers вне
water/bottom ещё не направлены в новый typed path.

Все242 engine source hashes в двух views совпали, отдельно проверены66 core
outputs трёх views. От bottom stage изменились только MatrixWater.cpp,
MatrixWater.hpp и selected OriginalManagedTexture.cpp. Upstream pin сохранён.
[Evidence](matrixgame-e2e519-water-evidence.json) содержит команды, object/log/
binary hashes, исправленный fixture failure и реальные gates. build_dirty=true:
проверки выполнены до локального коммита.

Повторно совпали13 доступных protected inputs,107 аппаратных файлов, frozen
main NRO/ELF и оба ZIP. G:/pack.zip недоступен. Сейвы и ресурсы не менялись.
Причины аппаратного intro raster, двух historical manual-publish ENOENT и
freeze карты/консоли при выходе остаются OPEN; новых аппаратных данных нет.
