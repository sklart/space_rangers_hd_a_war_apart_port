# E2E-5.19 — оригинальная очистка эффектов, billboard и point-light pool

24.10.2026, продолжение `0f5c581`, локальный master, без push.

## Что подключено

Полные оригинальные MatrixEffect.cpp, MatrixEffectPointLight.cpp и CBillboard.cpp
включены в общий native-cache. Сохранены фабрики эффектов, spawner, алгоритмы
геометрии и выбора ресурсов; SDK операции направлены в существующие typed
device/texture/staging adapters. Named texture requests используют GetManaged,
а не несовместимый cast portable CacheTexture. LOD bias передаёт те же float32
bits через bit_cast, `_alloca` сохраняет исходную stack-allocation семантику
через MSVC intrinsic или GCC builtin. Математика эффектов не заменялась.

Следующий настоящий link blocker — CVectorObjectAnim::~CVectorObjectAnim:
ClearEffects уничтожает CDebris, который наследуется от **полного оригинального
аниматора**. Ранее принятый NativeVectorObjectAnim — отдельный CPU-only тип.
В OriginalVectorAnim.cpp включён весь исходный блок методов CVectorObjectAnim,
включая InitLights/DrawLights, и исходные SColorInterval/SLightData. Поля/layout
сохранены; private payload definitions остаются в implementation TU. Native
адаптер и принятые core algorithms не менялись. CVectorObjectGroup не переносился.

Billboard требовал D3DXVec3TransformCoordArray. Новый geometry::TransformCoordArray
читает/пишет XYZ с исходными byte strides и вызывает уже используемую scalar
DirectXMath операцию. Между XYZ могут находиться цвет/UV; они не копируются
вместе с координатами. Это математическая операция, без GPU/device эмуляции.

## Что действительно исполнено

В **общем** bootstrap, два сеанса host и ARM64/QEMU:

- original CreatePoolDefaultResources → CBillboard::Init выделяет настоящие CPU
  staging VB/IB и заполняет исходную таблицу индексов. Повторный Init не меняет
  объём. Это не GPU allocation/upload и не повтор standalone BigVB/IB suites;
- original SPL_VBIB::GetCreate/LockVB/LockIB/NoNeed: реальные allocations,
  запись/Unlock, initialized ranges, reuse того же free-pool entry;
- original ClearEffects → CBillboard::Deinit и point-light ClearAll/ReleaseFree:
  непустые ресурсы освобождены, AllocatedBytes возвращается к исходному значению.
  Повторный ClearEffects безопасен, следующий цикл работает без StaticInit;
- original CBillboardLine ctor/UpdateVBSlot/Release/dtor: две ориентации линии,
  все четыре XYZ, цвет и UV сверены с аналитическими значениями. Draw не вызывается;
- настоящий пустой CDebris исполняет original CVectorObjectAnim ctor/Init/Clear/
  повторный Clear/dtor. VO/lights не загружались, успешный светящийся анимированный
  объект не заявляется.

## Доказательства и ограничения

Host ASan/UBSan/leak-check и ARM64/QEMU: exit0. На MatrixEffect.cpp только vptr
проверки исключены: метаданные фабрик удерживают RTTI ещё не перенесённых
конкретных эффектов, что подтверждено реальным failed link. Address и другие
UBSan проверки сохранены. Billboard, point-light и original animator проходят
без нового vptr исключения. Прежние Console globals и Logic/Map/SkinManager
vptr ограничения остаются. Это не полный sanitizer coverage движка.

devkitA64: COMPILE/LINK PASS отдельного ELF, strong undefined=0. SHA-256:
`48c87301223f7ff645d565380ee9e4940026b4d89bc30376b533411de6729bd7`.
Switch runtime **HARDWARE PENDING**, NRO не создавалась. Windows compile/runtime
этих новых модулей в этом шаге NOT RUN. Fresh CMake configure/generate PASS;
проверены восемь новых compile recipes, повторная полная сборка не выполнялась.

При первом host запуске сценарий выбрал link argv вместо runtime argv.
Исполняемый файл вернул2 до входа в RunBootstrap по проверке числа аргументов.
Исправлена команда; **тот же бинарник** прошёл настоящий bootstrap без пересборки.
Это ошибка проверочного сценария, не дефект движка. Failed log и версия сценария
сохранены. Отдельно сохранены build failures missing animator destructor и
incomplete light payload; они устранены подключением оригинальных определений.

Полный InitEffects на настоящем CFG/ресурсах, создание effect heap, загрузка
debris, spawner ctor/Takt, эффекты в мире, оригинальные светящиеся анимации,
GPU binding/upload/draw **NOT RUN**. Новые Cull/AlphaRef/subtract/complement
пути отрисовки пока подтверждены компиляцией, не исполнением на backend.

## Точная граница

Map owner **LINK FAIL13**, ранее15: CEffectSpawner destructor и ClearEffects
разрешены настоящими реализациями. Остаются world-object destructors,
RemoveFromSorted/ClearTextures, side owner, GroupClear, minimap и hint/interface
cleanup. Точные13 symbols и команды — в
[evidence](matrixgame-e2e519-effects-evidence.json).
Настоящий CMatrixMapLogic ctor/dtor, CGame::Init, RunGameLoop, CFormMatrixGame и
бой **NOT EXECUTED**. Успешный bootstrap не означает запущенный мир.

Сверены243 engine files в двух views (новый animator TU), отдельно66 core
outputs в трёх views. Изменились только три новых полных модуля и новый animator
TU. Реальные build inputs и object/log/binary hashes записаны в evidence;
`build_dirty=true`, проверки до локального коммита. Upstream pin сохранён.

Совпали13 доступных protected inputs,107 аппаратных файлов, frozen main
NRO/ELF и оба аппаратных ZIP. G:/pack.zip недоступен. Сейвы и ресурсы не менялись.
Новых оснований закрыть причины intro raster, двух historical manual-publish
ENOENT и freeze карты/консоли нет; эти расследования остаются OPEN.
