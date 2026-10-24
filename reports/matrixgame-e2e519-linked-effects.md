# E2E-5.19 — оригинальные Lightening, Repair и Shleif в общем движке

Подключены полные исходные `MatrixEffectLightening.cpp` (включая Shorted),
`MatrixEffectRepair.cpp` и `MatrixEffectShleif.cpp` в production native-cache.
Три прежних RTTI-блокера закрыты настоящими compiler-generated RTTI/vtable
оригинальных классов. Фиктивные определения не добавлялись.

## Доказанное продвижение

На одном и том же ARM64 link command проверены зависимости настоящих
`CMatrixMapLogic` constructor/destructor до и после добавления трёх объектов:
**30 → 20 unresolved**, закрыты десять связей, новых нет. Закрыты constructors
и SetPos Lightening/Shorted, constructor/GetTarget Repair,
constructor/Release/AddSmoke/AddFire Shleif. Это link probe, карта не создавалась.

Сравнение полных effective CPP с предыдущим этапом: добавлены только include/
namespace prelude, ASSERT_DX adapter и замена названия функции в комментарии
Repair. После удаления этих перечисленных вставок файлы полностью совпадают:
все оригинальные тела методов, ветви, allocation/release и алгоритмы сохранены.

- **devkitA64 compile/link PASS:** полная свежая CMake/Ninja сборка, 127 jobs,
  bootstrap ELF без strong undefined. Конфигурация сначала упёрлась в sandbox
  access к метаданным закреплённого submodule; повторена с разрешённым локальным
  доступом. До успешного configure объекты в этой директории не создавались.
- **Linux ARM64 compile/link PASS:** адресно скомпилированы три полных CPP,
  добавлены в копию production archive предыдущего этапа. Остальные неизменённые
  objects переиспользованы; это не новая полная ARM64-сборка.
- **Host ASan/UBSan compile PASS, LINK BLOCKED:** все три CPP скомпилированы с
  прежними флагами sanitizer. Повторная линковка общего bootstrap теперь требует
  только `typeinfo for CMatrixBuilding` и `typeinfo for CMatrixRobotAI`, вместо
  пяти RTTI предыдущего этапа. Runtime NOT RUN. Новых exclusions нет.

Запуск constructors, Takt/Draw и factories новых эффектов этим этапом не
подтверждён. Lightening/Shleif factories требуют настоящий g_MatrixMap; он не
подменялся искусственным объектом. CGame::Init, RunGameLoop, CFormMatrixGame,
мир и бой остаются незапущенными. Switch — **HARDWARE PENDING**, новой NRO нет.

## Следующая граница

Host bootstrap требует оригинальных владельцев из `MatrixRobot.cpp`
(CMatrixRobotAI) и `MatrixObjectBuilding.cpp` (CMatrixBuilding).
Production map-owner также требует Cannon, Side/UI, Group, special robot,
SAutoFlyData и constructor/Release BillboardLine из `Effects/MatrixEffectBillboard.cpp`.
Полный точный список из 20 symbols приведён в evidence JSON.
Подключение этих владельцев может раскрыть следующие транзитивные зависимости;
число unresolved не является процентом готовности.

## Сохранность и открытые задачи основной игры

13 доступных protected inputs сверены с исходными hashes: без изменений.
G:/pack.zip недоступен. Основная NRO, matching ELF и аппаратные комплекты не
пересобирались; пользовательские данные, ресурсы и настройки консоли не менялись.
Push не выполнялся. Источник повторяющегося текста в CPU-композиции,
исторические manual-publish ENOENT и причины freezes основной игры остаются OPEN.

[Команды, полные списки symbols, результаты и hashes](matrixgame-e2e519-linked-effects-evidence.json).
