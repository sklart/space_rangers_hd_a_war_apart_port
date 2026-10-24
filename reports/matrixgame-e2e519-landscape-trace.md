# E2E-5.19 — полные LandscapeSpot и MatrixMapTrace

24.10.2026. База `ead0c5f6252c91942c61b90bca7c7be244c6f0a9`.

Подключены полные production modules
`MatrixGame/src/Effects/MatrixEffectLandscapeSpot.cpp` и
`MatrixGame/src/MatrixMapTrace.cpp`.
Сохранены исходные генерация полос/UV/геометрии, fade callbacks, списки пятен,
priority, cleanup и алгоритмы трассировки landscape/objects/land/catch point.
Все **21 тело исходных методов** сверено после только перечисленных адаптеров.

## Изменения переноса

LandscapeSpot использует существующие typed state/texture/buffer/draw adapters.
Единственный _alloca заменён на соответствующий compiler intrinsic.
Три исходные 32-битные записи пар индексов теперь используют memcpy в тот же
адрес с тем же uint32 value и размером 4 bytes. Это сохраняет упаковку и порядок
индексов и не требует выровненного uint32_t* на границе двух uint16_t.
BuildLand runtime не выполнялась; результат реальной terrain triangulation
этим срезом не подтверждён.

В полном Trace три чтения float через uint32_t* заменены на bit_cast.
Маска `0x7FFFFFFF`, условия выбора оси, порядок обхода групп и поиск попаданий
сохранены. Для проверки не создавалась фиктивная карта или упрощённый ray tracer.

## Фактические результаты

- devkitA64: оба полных CPP compile PASS; общий принятый bootstrap link PASS,
  strong UND = 0. ELF SHA-256:
  `532216b0a1c86acdf7ae6c15790b4c7f8dd6330ec497560534597128de217fbd`.
  Это ELF bootstrap с dead-code collection, не полный engine/NRO.
- Linux ARM64/QEMU: прежний общий bootstrap с новой native-cache library
  runtime PASS, две сессии. **Новые Trace/BuildLand/object lifecycle не исполнялись**;
  это regression gate, не runtime proof новых игровых алгоритмов.
- MSVC: compile PASS обоих полных production CPP; Windows runtime NOT RUN.
- CMake: configure/generate PASS, оба CPP есть в native-cache и full-engine
  compile recipes. Повторной полной сборки движка нет.
- Host GCC ASan/UBSan: оба CPP compile PASS, но **LINK BLOCKED**, runtime NOT RUN.

Host link остановлен ровно на:

```
typeinfo for CMatrixFlyer
typeinfo for CMatrixRobot
```

Ссылки исходят из UBSan vptr metadata полного MatrixMapTrace.cpp.
Настоящие key virtual owners этих классов ещё не перенесены целиком.
Новые sanitizer exclusions, фиктивные RTTI definitions или force-link overrides
не добавлены. В текущей экспериментальной конфигурации host bootstrap с
ASan/UBSan остаётся незамкнутым до переноса настоящих owners.
Следующий шаг — исходные Flyer/Robot lifecycle, а не подмена RTTI.
Конкретные исходные owners: `MatrixGame/src/MatrixFlyer.cpp`
(`CMatrixFlyer::~CMatrixFlyer`) и `MatrixGame/src/MatrixObjectRobot.cpp`
(`CMatrixRobot::~CMatrixRobot`); RobotAI lifecycle отдельно находится в
`MatrixGame/src/MatrixRobot.cpp`.

## Production-граница карты

Отдельный force-link map owner на **Linux ARM64 без санитайзеров**:
**14 unresolved вместо 17**, закрыты:

- CMatrixEffectLandscapeSpot constructor;
- CMatrixEffectLandscapeSpot::Priority;
- CMatrixMap::Trace.

Новых production missing symbols нет. Предыдущий baseline — host negative
probe с уже существовавшими ограничениями vptr для других owner modules;
архитектуры/инструментальные режимы в evidence указаны отдельно.
Число 14 относится к production ARM64, не к успешному host sanitizer link.

Остались interface hint cleanup, weapon effects, Flyer/RobotAI/Building/Cannon/
Side owners, side orders и special robot creation. Точный список в evidence.
Это замыкание зависимостей, не запуск карты и не процент готовности.

Constructor LandscapeSpot читает свойства текстуры и реальные размеры/высоты
карты. Его BuildLand, PrepareDX, Takt, DrawAll и destructor ещё не исполнены.
Trace/TraceLand/CatchPoint также требуют настоящей карты и объектов.
Полный CGame::Init, RunGameLoop, CFormMatrixGame и бой NOT EXECUTED.
Switch/deko3d — HARDWARE PENDING. Нового NRO нет.

## Сохранность и доказательства

Effective engine остаётся 248 файлов, 66 portable-core outputs прежние.
Из engine sources изменены только два указанных CPP; headers/layouts не менялись.
Основная NRO/ELF, два аппаратных ZIP, 13 доступных protected inputs и 107
аппаратных файлов сверены по исходным SHA. G:/pack.zip недоступен.
Пользовательские сейвы, ресурсы и настройки консоли не менялись. Push не выполнялся.
Причины CPU raster, исторических manual-publish ENOENT и freezes основной игры
остаются OPEN.

[Команды, hashes, negative link и точные оставшиеся symbols](matrixgame-e2e519-landscape-trace-evidence.json).
