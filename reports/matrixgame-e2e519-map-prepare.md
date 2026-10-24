# E2E-5.19: полная оригинальная подготовка карты

В native target включён полный `MatrixMapPrepare.cpp` (OriginalMapPrepare.cpp),
вместо трёх выбранных методов очистки групп. Подключены оригинальные PrepareMap,
Restart, InitObjectsLights, Create/ReleasePoolDefaultResources, CheckLostDevice
и остальные методы этого TU. Нормали, чтение CStorage, объекты, AI, загрузка
ландшафта, миникарта и перезапуск не заменялись альтернативными алгоритмами.

## Новая граница интеграции

Forced full CGame::Init link на ARM64: четыре прежние связи CMatrixMap закрыты.
Осталась **одна** отсутствующая реализация:
`PathToOutputFiles(wchar_t const*)`, потребитель — оригинальный
`CMinimap::RenderBackground`, MatrixMinimap.cpp:1005. Производитель находится в
`MatrixGamePathUtils.cpp`: Windows personal folder / SpaceRangersHD / dest,
с исходным fallback при отказе Shell API. Подставной каталог не добавлен.

**CGame::Init NOT CALLED; карта и бой NOT RUN.** Следующий исполнительный
блокер виден уже в начале оригинального MatrixGame.cpp: после static_init
он присваивает `g_MatrixHeap = HNew(NULL) CHeap`, тогда как текущая integrated
сессия уже принадлежит NativeInitRuntime. Перед реальным вызовом нужно
согласовать создание и освобождение владельцев, а не молча перезаписать
указатель. Доказательства работоспособной загрузки карты из нового TU нет.

## Адаптации и проверки

- Полные оригинальные тела GroupClear/ClearGroupVis/SGroupVisibility::Release
  сохранены, отдельный lifecycle object удалён из новой копии архива. Общий
  bootstrap исполняет их реальные cleanup/reentry проверки из прежнего этапа.
- Исходные MakeSolidTexture, native VB staging, CMatrixEffect, WaterInit,
  расчёт света, объектная загрузка и обработка флагов сохранены. Новый TU
  не выполнялся на вымышленной карте и не получает фиктивных объектов.
- INT32 заменён на std::int32_t. Stack allocation использует прежний
  caller-stack macro. RotationX/TransformNormal используют существующий
  object_geometry. Новых математических алгоритмов не вводилось.
- Материал передаётся через существующий VideoMaterial/SetVideoMaterial;
  значения specular-material register146 и MATERIAL0 сверены с Windows SDK.
- Windows cooperative-level backend вызывает настоящий TestCooperativeLevel.
  StoredStateBackend получен от настоящего ReadyDevice через dynamic_cast;
  отсутствие нужного backend отклоняется явно. На CPU/Switch без SDK нет
  фиктивного результата D3D_OK.
- Reset использует тот же Current().presentation, куда прежний video adapter
  отображает оригинальный g_D3Dpp. ResetPresentation по-прежнему отклоняется
  до реализации безопасного device lifecycle. Успешный Reset не заявляется.

Host ASan/UBSan/LSan: полный новый TU, backend, link и общий runtime **PASS**,
без новых исключений санитайзера. ARM64/QEMU: compile/link/bootstrap runtime
**PASS**, forced CGame::Init link **FAIL: одна указанная связь**. Новые
PrepareMap/Restart/создание GPU-ресурсов этим runtime не исполнялись.

devkitA64 и Windows MSVC: оба затронутых полных TU **COMPILE PASS**. Нового
devkitA64 link/ELF/NRO на этом этапе нет; прежний transition ELF не выдаётся
за сборку с MapPrepare. Это адресная интеграция с переиспользованием
неизменённых native объектов, не повторная полная сборка.

Артефакты: `build/matrixgame/e2e519/map-prepare-{source,sdk,msvc,host,arm64}`.
[Команды, source/object hashes, runtime и unresolved symbol](matrixgame-e2e519-map-prepare-evidence.json).
Основная NRO, frozen комплекты, сейвы, ресурсы и аппаратные журналы не
изменялись. Push не выполнялся. Основные аппаратные P0 остаются OPEN.
