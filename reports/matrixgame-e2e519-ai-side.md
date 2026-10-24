# E2E-5.19 — полные Environment/AIGroup/Side/Dust и CInfo FIFO

В production native-cache добавлены полные оригинальные `Logic/MatrixEnvironment.cpp`,
`Logic/MatrixAIGroup.cpp`, `MatrixSide.cpp` и `Effects/MatrixEffectDust.cpp`.
Не выделялись только удобные методы; fake world/AI/battle results не добавлялись.

## Доказанная ошибка CInfo

Оригинальный AddBadCoord при заполненном массиве из 16 CPoint сдвигал данные
на `(16-1)*sizeof(int)`, то есть 60 вместо 120 байт. Оба FIFO также использовали
CopyMemory для перекрывающихся областей. В исходном AddBadPlace размер элементов
верен, но для overlap требуется memmove.

ARM64/QEMU baseline на оригинальном CInfo воспроизвёл повреждение после 17-й
координаты: слот 7 expected `(108,-208)`, actual `(108,-207)`, sizeof(CPoint)=8.
Контролируемый exit 1, завершено 0 сессий. Baseline не является sanitizer PASS.

Исправлены только два сдвига: std::memmove и sizeof(actual_element).
Оригинальные порядок элементов, ограничение длины и подавление дубликатов сохранены.
Тот же test source включён в общий bootstrap. Проверяются 40 координат, 24 места,
FIFO order, удаление вытесненного элемента, duplicate suppression и reentry.

**После исправления host ASan/UBSan с LeakSanitizer и ARM64/QEMU — PASS**, exit 0,
по две сессии. На каждой сессии выполняются два свежих lifecycle CInfo.
Enemy/world paths не подменялись и не исполнялись; Reset/Clear проверены без врагов.
Связь этого MatrixGame дефекта с зависаниями основной игры не установлена.

## Реальная интеграция и зависимости

- **devkitA64:** полная compile/link PASS, strong undefined 0. Первоначальный
  compile blocker Side — timeGetTime и TRUE/FALSE — устранён адаптерами ниже.
- **Host:** четыре полных новых CPP compile/link PASS; общий bootstrap runtime
  PASS под ASan/UBSan, detect_leaks=1, halt_on_error=1. Восемь прежних vptr
  exclusions остаются, новых нет.
- **Linux ARM64:** четыре полных CPP compile/link PASS, общий QEMU runtime PASS.
  Host/ARM64 адресно компилируют новые модули и изменённый/new driver/test;
  неизменённые objects предыдущей свежей сборки переиспользованы. Headers/layouts
  существующего engine ABI не менялись.
- **Production map-owner:** 34 → 26 unresolved. Закрыты 23 прежние связи,
  раскрыты 15 новых транзитивных. Это link probe, карта не создавалась.

Следующие владельцы: CConstructor/CConstructorPanel, CIFaceList/CInterface,
cursor visibility, Moveto, CMultiSelection, camera restore/SAutoFlyData и SSpecialBot.
Точные before/after lists находятся в evidence JSON.

## Адаптер статистического времени

Side вычисляет STAT_TIME из timeGetTime и map start timestamp. Добавлен один
GameClockMilliseconds: настоящий steady_clock, миллисекунды modulo uint32.
Один исполняемый call site Side и два setter sites MatrixMapPrepare используют
один домен. Шесть закомментированных измерений Side сохранены.
Планирование loop, sleep, игровое продвижение времени и таймер основной игры не менялись.

Стартовые функции полного MatrixMapPrepare ещё не входят в production compiled
subset: их адаптация проверена по source, не по runtime. Side statistics/world
runtime также не проверен. Это не доказательство завершённого таймерного порта.
Обычные MoveMemory операции Side используют std::memmove с теми же аргументами;
TRUE/FALSE заменены на эквивалентные bool literals.

Полные effective TU сопоставлены с предыдущими исходниками после обращения
конкретных adapters и двух объявленных FIFO corrections. Остаточных изменений
AI algorithms нет; сохранены исходные решения, переходы и владение ресурсами.

## Границы и сохранность

Side constructor/gameplay, AIGroup nonempty world, Dust simulation/rendering,
полные CGame::Init, RunGameLoop, CFormMatrixGame и бой не исполнялись.
Switch/deko3d — HARDWARE PENDING, новой NRO нет.

13 доступных protected inputs, 107 аппаратных файлов, frozen main NRO/ELF и оба
hardware ZIP сверены по исходным SHA-256: изменений нет. G:/pack.zip недоступен.
Сейвы, ресурсы и настройки консоли не менялись. Push не выполнялся.
Источник повторяющегося текста в CPU-композиции, исторические manual-publish ENOENT
и причины зависаний основной игры остаются OPEN.

[Команды, source fidelity, baseline/candidate runtime и SHA-256](matrixgame-e2e519-ai-side-evidence.json).
