# E2E-5.19 — Button/Billboard и восстановленная sanitizer-регрессия

В production native-cache включены полные оригинальные `Interface/CIFaceButton.cpp`
и `Effects/MatrixEffectBillboard.cpp` (Billboard, BillboardLine и BillboardScore).
Оригинальные compiler-generated RTTI/vtable четырёх классов реально определены
в devkitA64 archive. Прежние два host RTTI-блокера устранены без fake definitions
и без новых sanitizer exclusions.

## Реальный результат

- **Host ASan/UBSan + LeakSanitizer: compile/link/runtime PASS**, две сессии,
  exit 0, ASAN_OPTIONS=detect_leaks=1:halt_on_error=1,
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1.
  Впервые после интеграции новых actor/effect owners снова выполнена общая
  регрессия, включая high-address callbacks и typed Cannon unit reset с настоящим
  эффектом, отсоединением backlink, освобождением и повторным входом.
- **ARM64/QEMU: compile/link/runtime PASS**, две сессии, exit 0.
- **devkitA64:** полная свежая CMake/Ninja compile/link PASS, 136 jobs,
  strong undefined = 0. Runtime на Switch не выполнялся, NRO не создавалась.
- **Production ARM64 map-owner probe:** 57 → 49 unresolved, закрыты восемь
  Billboard/Line/Score связей, новых unresolved нет. Карта не создавалась.

Host/ARM64 — адресная компиляция двух новых полных CPP с переиспользованием
неизменённых production objects предыдущего этапа. Headers/layouts не менялись.
Сохраняются восемь прежних UBSan vptr exclusions в других TUs; общей полнотой
sanitizer-покрытия всего движка данный PASS не является.

## Сохранение оригинальной логики

Сравнение всех effective CPP/HPP/H с предыдущей SDK source view выявило ровно
два изменённых CPP. После обращения перечисленных typed device/cache adapters
и эквивалентных TRUE/FALSE spelling оба полных TU совпадают с предыдущими файлами.
Ветви UI actions, hints, popup menu, sound, constructors/destructors и resource
refcounts сохранены. Billboard draw использует прежние аргументы и порядок
операций через существующие texture, buffer и device adapters.
Изменения алгоритмов или новый математический код не добавлялись.

## Точная граница исполнения

Новые Button/Billboard constructors, mouse methods, Takt/Draw, refcount lifecycle
Billboard и его GPU drawing этим этапом не запускались. Runtime PASS относится
к общему существующему bootstrap и регрессиям предыдущих исправлений, а не к
работающему UI или бою. Нового изолированного CPU-прототипа нет.

Полный CGame::Init по-прежнему NOT CALLED; bootstrap заканчивается явными
Unsupported на отсутствующих platform/device/window boundaries.
Создание настоящего мира, RunGameLoop, CFormMatrixGame, GPU runtime на Switch
и планетарный бой не подтверждены. **HARDWARE PENDING** сохраняется.

Следующие реальные владельцы: полный CVectorObjectGroup, Side/UI, CInfo/Group,
Dust, SAutoFlyData и SSpecialBot. Точный оставшийся список из 49 symbols приведён
в evidence JSON. Число unresolved не является процентом готовности.

## Сохранность

13 доступных protected inputs, 107 аппаратных файлов, frozen main NRO/ELF и оба
hardware ZIP сверены с исходными SHA-256: изменений нет. G:/pack.zip недоступен.
Сейвы, ресурсы и настройки консоли не менялись. Push не выполнялся.
Источник повторяющегося текста, исторические manual-publish ENOENT и причины
зависаний основной игры остаются OPEN.

[Команды, полные source hashes, runtime logs и списки symbols](matrixgame-e2e519-button-billboard-evidence.json).
