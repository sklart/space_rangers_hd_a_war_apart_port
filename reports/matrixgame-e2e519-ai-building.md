# E2E-5.19 — полные оригинальные RobotAI и Building

В общий native-cache добавлены **полные** `MatrixRobot.cpp` и
`MatrixObjectBuilding.cpp`. Compiler-generated RTTI и vtable CMatrixRobotAI и
CMatrixBuilding реально определены в devkitA64 archive; фиктивных определений нет.
Это закрывает два прежних RTTI-блокера и переносит настоящую AI/building логику
на уровень компилируемых production objects, но ещё не доказывает её исполнение.

## Изменения и сохранение семантики

Оригинальные алгоритмы, ветви, constructors/destructors, orders и resource
ownership сохранены. Весь effective TU сопоставлен с предыдущим prepared source
после обращения конкретных адаптаций: namespace/include prelude, ASSERT_DX,
cache texture accessor, RotationYawPitchRoll, Dot2, Translation, Scaling,
MoveMemory → memmove, sampler/render state и bit_cast float LOD bias.
Остаточных различий алгоритмов нет. Новых математических реализаций не добавлено:
использованы существующие object/actor/minimap geometry adapters.

Состояние клавиш RobotAI использует уже имеющийся оригинальный Input adapter.
Graphics ops сохраняют исходные аргументы и порядок вызовов. Matrix pin не менялся.
В прежних Robot/Flyer effective CPP добавились только include/alias для существующей
Translation; остальные headers/layouts не менялись.

## Проверки и точная граница

- **devkitA64:** полная свежая CMake/Ninja сборка, 129 jobs, compile/link PASS.
  ELF strong undefined = 0. Отдельная linkable часть bootstrap, не полный движок.
- **ARM64 Linux:** два полных новых CPP compile PASS; archive и общий bootstrap
  link PASS. Неизменённые объекты предыдущих этапов переиспользованы.
  Новые actor constructors/Takt не запускались; нового QEMU runtime нет.
- **Host ASan/UBSan:** оба полных CPP compile PASS. Bootstrap **LINK BLOCKED**,
  runtime NOT RUN. Старые Building/RobotAI RTTI закрыты; теперь требуются
  `typeinfo for CIFaceElement`, `CMatrixCannon`, `CMatrixEffectElevatorField`,
  `CMatrixEffectZahvat`. Новых sanitizer exclusions нет.
- **Настоящий map-owner link probe:** прежние 20 symbols → 61; закрыты четыре
  связи (Building destructor/GetFloorZ, RobotAI destructor/CreateTextures),
  раскрыты 45 транзитивных зависимостей. Это не runtime regression и не процент
  готовности: впервые удерживаются полные исходные виртуальные методы этих классов.

Основные открытые владельцы: Side/UI/Group, CInfo, Cannon, Billboard/Score/Dust,
ElevatorField/Zahvat и полный CVectorObjectGroup. Точный before/after symbol list
и команды находятся в evidence JSON. Следующая адресная работа — эти реальные
владельцы, без fake RTTI или урезания virtual methods ради успешной линковки.

Полный CGame::Init, создание настоящей карты, RunGameLoop, CFormMatrixGame и бой
ещё не выполняются. Building destructor обращается к реальной карте: запускать
его на искусственном объекте для lifecycle PASS нельзя. Switch/deko3d остаётся
**HARDWARE PENDING**. NRO не создавалась; Windows runtime этим этапом не проверен.

## Сохранность

13 доступных protected inputs, 107 аппаратных файлов, основная frozen NRO и
matching ELF сверены по SHA-256: изменений нет. G:/pack.zip недоступен.
Оба прежних hardware ZIP, сейвы, ресурсы и настройки консоли не менялись.
Push не выполнялся. Источник CPU-композиции повторяющегося текста, исторические
manual-publish ENOENT и причины freezes основной игры остаются OPEN.

[Команды, original-source audit, RTTI/vtable, SHA-256 и оставшиеся symbols](matrixgame-e2e519-ai-building-evidence.json).
