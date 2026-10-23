# E2E-5.19 — полный модуль групп и оригинальная очистка владельца

24.10.2026. Продолжение `09ba09d`, локальный master, без push.

## Интеграция

Полный MatrixMapGroup.cpp включён в native-cache: исходные BuildBottom,
BuildWater, picking, сортировка/поиск объектов, волны и cleanup сохранены.
Добавлен настоящий ShadowProj.hpp для базовых классов, вместо неполных forward
declarations. SDK draw/transform/state вызовы используют существующие typed
adapters; две stack allocations сохраняют исходную семантику через intrinsic.

BuildWater создаёт настоящий original CTextureManaged через CreateManaged,
а не объект portable cache под несовместимым указателем. Сохранена исходная
цепочка CreateLock A8→A8L8→A8R3G3B2→A8R8G8B8, с явным TexturePixelFormat.
Это пока **compile proof**, не создание водной маски и не GPU runtime.

Из MatrixMapPrepare.cpp в OriginalGroupLifecycle.cpp включены три исходных
метода: SGroupVisibility::Release, CMatrixMap::ClearGroupVis и GroupClear.
Их тела сравнены с подготовленным оригиналом целиком, без изменений алгоритма.
Они закрывают реальную зависимость CMatrixMap::Clear, не заменяют мир заглушкой.
Полный MapPrepare в этом шаге **не компилировался и не интегрировался**.
При его подключении selected lifecycle TU нужно исключить из того же link,
чтобы оригинальные определения не дублировались.
В нём остаются отдельные SDK пути render-target/CACHE_CREATE_TEXTURE и
CheckLostDevice с TestCooperativeLevel/Reset; готовность этих путей не заявляется.

## Выполнение и проверки

В каждом из двух общих сеансов host и ARM64/QEMU настоящий
SGroupVisibility::Release освобождает непустые vis/levels массивы, выделенные
на настоящем game heap; проверены повторный Release, повторное выделение без
StaticInit, нулевые pointers/counts и сохранность z_from. Это **ownership
fixture**, не результат расчёта видимости. Ни настоящий map, ни terrain group
не подставлялись. GroupClear/ClearGroupVis и конструктор/BuildBottom/BuildWater
группы **не исполнялись**: для этого нужен ещё не запущенный настоящий мир.

Host ASan/UBSan/leak-check и ARM64/QEMU: exit0. Новый full group TU требует
`-fno-sanitize=vptr`: failed link показывает metadata references к ещё не
перенесённым CMatrixMapStatic/CMatrixRobot RTTI. Address и остальные UBSan checks
сохранены; исключение не распространяется на OriginalGroupLifecycle.cpp.
Прежние ограничения Console globals и Logic/Map/SkinManager/MatrixEffect vptr
остаются. Полный sanitizer coverage движка не заявляется.

devkitA64: полный group и selected lifecycle COMPILE PASS; общий prefix ELF
LINK PASS, strong undefined=0. SHA-256:
`d7474183b2d391543a6a5ae5d180e5bb7fda1d57f8aaa6d03fa286bd8e63ecb6`.
ELF удерживает выполняемый visibility Release; полный map-owner path ещё
не слинкован. Switch runtime **HARDWARE PENDING**, NRO не создавалась.
Windows compile/runtime новых модулей в этом шаге NOT RUN.
Fresh CMake configure/generate PASS, четыре новых compile recipes проверены;
повторная полная сборка не выполнялась.

В private runner пришлось явно взять существующий link template: предыдущий
resumed result не содержал common link command. Скомпилированные host objects
повторно использованы; тест не был объявлен успешным до реального запуска.
Failed vptr link и версии сценариев сохранены в evidence.

## Точная оставшаяся граница

Настоящий CMatrixMapLogic ctor/dtor **LINK FAIL12**, ранее13: GroupClear
разрешён оригинальной реализацией. Остаются object/side destructors,
RemoveFromSorted/ClearTextures, minimap и hint/interface cleanup; точные symbols
в [evidence](matrixgame-e2e519-map-group-evidence.json).
CGame::Init, RunGameLoop, CFormMatrixGame, загруженный мир и бой **NOT EXECUTED**.

244 engine source files в двух views совпали; отдельно совпали66 core outputs
в трёх views. От effects stage изменились только MatrixMapGroup.cpp и новый
OriginalGroupLifecycle.cpp. Upstream pin и core не менялись. Evidence записан
до коммита, build_dirty=true, содержит реальные source/object/log/binary hashes.

Совпали13 доступных protected inputs,107 аппаратных файлов, frozen main NRO/ELF
и оба аппаратных ZIP. G:/pack.zip недоступен. Сейвы/ресурсы не менялись.
Причины intro raster, двух historical manual-publish ENOENT и зависаний
карты/консоли по-прежнему OPEN; новых аппаратных оснований для их закрытия нет.
