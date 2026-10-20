# MatrixGame E2E-5.15: состояние нативного порта

Дата: 20.10.2026. Исходный HEAD этого пакета:
`58bd45765b07180bf0ad94432f4c1129dfe595d0`.
Upstream MatrixGame: `218115a72fa9c5d43269f4ca84a8596a0bcb82f1`, подмодуль чистый.

Полный движок, native CGame Init/Run, настоящий планетарный бой и интеграция в
основной NRO пока отсутствуют. Новая подтверждённая часть — полный release CPU-модуль
MatrixRoadNetwork, скомпилированный и скомпонованный с существующим native-core.
Это не выполнение боя и не аппаратная проверка Switch.

## Новый модуль и граница переносимости

`tools/matrixgame_navigation_overrides.py` проверяет SHA исходных cpp/hpp и меняет
только платформенные конструкции в disposable effective source: DWORD/BYTE на
фиксированные типы, CopyMemory/MoveMemory/ZeroMemory на стандартные операции,
удаляет пустой в release DTRACE и Windows-зависимый include Tracer.hpp.
Оригинальные функции поиска пути, загрузки/сохранения и структуры графа сохранены.
GPU debug helpers остаются под исходными условиями; `_RELDEBUG` выбирает release-путь.
Поддельные d3d9.h, реализация фиктивного CGame и результаты боя не добавлялись.

CMake выделяет `matrixgame-native-navigation`, а полный engine зависит от него:
RoadNetwork не компилируется повторно в основной библиотеке. Тестовое executable
включается через BUILD_TESTING или явный NAVIGATION_BUILD_LINK_PROBE; в NRO модуль
этим изменением не включён. Прежние native-core CPU/resource и callback проверки
описаны в отдельных отчётах; они не заменяют полный engine.

## Фактический host LINK/RUNTIME

GNU C++ 15.2, Release, C++20, `-ffp-contract=off`.
`matrixgame-native-navigation` STATIC и `matrixgame-native-navigation-test`
скомпилированы и скомпонованы успешно.
SHA-256 executable: `f96ff95c01ea17c5d2616aafdcc2e937057e5d6de901675656d525994a7fdbcc`.

Тест выполнил десять циклов трёхзонного графа: путь 0→1→2, расстояние 20,
ограничение движения, Save/Load v27 и точное равенство повторной сериализации.
Синтетический fixture: 310 bytes, FNV32 `6b16a8e6`.

Ресурсная цепочка использует копию настоящего `Matrix/Map/Boarding.cmap` из
read-only robots.pkg: CStorage → roads/Data → original Load(v27) → region queries
→ original Save → Load/Save. Игровые bytes в Git не добавлены.
Копия cmap: 1112260 bytes, SHA-256
`8def488cb899a1c0f2cc76978954478cfff32e9866dc336275cf091f094e51da`.
Граф: 149 зон, 35 дорог, 27 узлов, 13 регионов; roads buffer 18894 bytes,
FNV32 `2cbab80a`. Выполнены 33 соседних запроса с масками 0/1/31, все достижимы.
Это ограниченная CPU-проверка одной карты, не полный AI/update loop.

Raw resource Save не равен входным bytes: original Load принудительно меняет
movement mask мест с пушками; на карте 10 таких мест. Нормализованная повторная
Save/Load/Save стабильна, FNV32 `e18e7b8d`. Полная побайтная атрибуция всех
отличий от исходного буфера ещё не выполнена; raw parity не заявляется.

Первый fixture FAIL вызван неинициализированными полями искусственно добавленного
региона: pinned POSIX ReAllocClear не очищает расширение памяти. Тест теперь явно
инициализирует свои регионы. Алгоритм движка не менялся. Второй FAIL был неверным
ожиданием raw byte equality после штатного Load; он сохранён как ограничение выше.
Новый модуль ещё не прошёл ASan/UBSan, Windows x86 reference, ARM64/QEMU и SDK link.

## До первого настоящего боя

| Граница | Состояние |
|---|---|
| Полный engine COMPILE | FAIL: Windows/D3D9 зависимости остаются |
| Полный engine LINK | BLOCKED компиляцией; проверенных LINK_FAIL нет |
| Native CGame Init/Run | RUNTIME UNSUPPORTED, не вызывался |
| Загрузка навигации настоящей карты | HOST PASS в описанной CPU-цепочке |
| Полный ресурсный поток карты/моделей/материалов | OPEN |
| D3D9 lifecycle/state/resources → deko3d | PARTIAL экспериментальный backend, не полный engine renderer |
| AI/physics/battle update | OPEN; путь региона не доказывает выполнение боя |
| Кампания → бой → возврат/результат | OPEN, production NRO не подключён |
| Switch execution/FPS/input | HARDWARE PENDING |

Следующая граница — отделение D3D9 ownership/types от реальных VectorObject,
map/object/material и lifecycle consumers с сохранением семантики. Затем полный
compile/link, настоящий entry и ресурсная инициализация; только после этого можно
проверять реальный update/render и возврат в кампанию. Процент готовности по числу
собранных единиц был бы недостоверным.

## Воспроизведение

```
cmake -S port/matrixgame/native-engine -B build/e2e515/mg-engine-after \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DBUILD_TESTING=OFF \
  -DNAVIGATION_BUILD_LINK_PROBE=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/e2e515/mg-engine-after --target matrixgame-native-navigation-test --parallel 2
build/e2e515/mg-engine-after/matrixgame-native-navigation-test build/e2e515/mg-map-private/Boarding.cmap
```

Пути build локальные ignored; копию лицензируемой карты надо подготовить из своей
установки. Полный поединичный compile audit приведён ниже.

## Поединичный compile audit

До: 1 COMPILE_PASS / 79 COMPILE_FAIL. После: 2 / 78. Новая единица — MatrixRoadNetwork.cpp.

Исходные логи: build/e2e515/mg-compile-before/result.json и mg-compile-after/result.json.
Это 80 игровых cpp: 79 engine + 1 navigation после выделения. MatrixGameDll.cpp
не выбран как Windows launcher. Всего pinned tree содержит 81 игровой cpp и 27
MatrixLib cpp; таблица не означает аудит всех 108 полных единиц MatrixLib/Game.
Native-core собирает свой подготовленный CPU/resource subset отдельно.

До CMAKE_BUILD_TYPE пустой, после Release (-O3 -DNDEBUG). Это сравнение
доступности компиляции, не производительности или идентичности object bytes.

| Единица | До | После | Первый блокер после |
|---|---|---|---|
| MatrixGame/src/DevConsole.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/Effects/MatrixEffect.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/Effects/MatrixEffectBigBoom.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/Effects/MatrixEffectBillboard.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/Effects/MatrixEffectDust.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/Effects/MatrixEffectElevatorField.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/Effects/MatrixEffectExplosion.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/Effects/MatrixEffectFirePlasma.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/Effects/MatrixEffectFlame.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/Effects/MatrixEffectKonus.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/Effects/MatrixEffectLandscapeSpot.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/Effects/MatrixEffectLightening.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/Effects/MatrixEffectMoveTo.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/Effects/MatrixEffectMovingObject.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/Effects/MatrixEffectPath.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/Effects/MatrixEffectPointLight.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/Effects/MatrixEffectRepair.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/Effects/MatrixEffectSelection.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/Effects/MatrixEffectShleif.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/Effects/MatrixEffectSmokeAndFire.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/Effects/MatrixEffectWeapon.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/Effects/MatrixEffectZahvat.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/Interface/CAnimation.cpp | COMPILE_FAIL | COMPILE_FAIL | windows.h: No such file or directory |
| MatrixGame/src/Interface/CConstructor.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/Interface/CCounter.cpp | COMPILE_FAIL | COMPILE_FAIL | windows.h: No such file or directory |
| MatrixGame/src/Interface/CHistory.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/Interface/CIFaceButton.cpp | COMPILE_FAIL | COMPILE_FAIL | windows.h: No such file or directory |
| MatrixGame/src/Interface/CIFaceElement.cpp | COMPILE_FAIL | COMPILE_FAIL | windows.h: No such file or directory |
| MatrixGame/src/Interface/CIFaceImage.cpp | COMPILE_FAIL | COMPILE_FAIL | windows.h: No such file or directory |
| MatrixGame/src/Interface/CIFaceMenu.cpp | COMPILE_FAIL | COMPILE_FAIL | windows.h: No such file or directory |
| MatrixGame/src/Interface/CIFaceStatic.cpp | COMPILE_FAIL | COMPILE_FAIL | windows.h: No such file or directory |
| MatrixGame/src/Interface/CInterface.cpp | COMPILE_FAIL | COMPILE_FAIL | windows.h: No such file or directory |
| MatrixGame/src/Interface/MatrixHint.cpp | COMPILE_FAIL | COMPILE_FAIL | windows.h: No such file or directory |
| MatrixGame/src/Logic/MatrixAIGroup.cpp | COMPILE_FAIL | COMPILE_FAIL | windows.h: No such file or directory |
| MatrixGame/src/Logic/MatrixEnvironment.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/Logic/MatrixLogicSlot.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/Logic/MatrixRoadNetwork.cpp | COMPILE_FAIL | COMPILE_PASS |  |
| MatrixGame/src/Logic/MatrixRule.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/Logic/MatrixState.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/Logic/MatrixTactics.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/MatrixCamera.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/MatrixConfig.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/MatrixCursor.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/MatrixDebugInfo.cpp | COMPILE_FAIL | COMPILE_FAIL | d3dx9core.h: No such file or directory |
| MatrixGame/src/MatrixFlyer.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/MatrixFormGame.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/MatrixGame.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/MatrixGamePathUtils.cpp | COMPILE_FAIL | COMPILE_FAIL | windows.h: No such file or directory |
| MatrixGame/src/MatrixInstantDraw.cpp | COMPILE_FAIL | COMPILE_FAIL | windows.h: No such file or directory |
| MatrixGame/src/MatrixLoadProgress.cpp | COMPILE_PASS | COMPILE_PASS |  |
| MatrixGame/src/MatrixLogic.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/MatrixMap.cpp | COMPILE_FAIL | COMPILE_FAIL | windows.h: No such file or directory |
| MatrixGame/src/MatrixMapGroup.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/MatrixMapPrepare.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/MatrixMapStatic.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/MatrixMapTexture.cpp | COMPILE_FAIL | COMPILE_FAIL | windows.h: No such file or directory |
| MatrixGame/src/MatrixMapTrace.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/MatrixMinimap.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/MatrixMultiSelection.cpp | COMPILE_FAIL | COMPILE_FAIL | windows.h: No such file or directory |
| MatrixGame/src/MatrixObject.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/MatrixObjectBuilding.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/MatrixObjectCannon.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/MatrixObjectRobot.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/MatrixProgressBar.cpp | COMPILE_FAIL | COMPILE_FAIL | windows.h: No such file or directory |
| MatrixGame/src/MatrixRenderPipeline.cpp | COMPILE_FAIL | COMPILE_FAIL | windows.h: No such file or directory |
| MatrixGame/src/MatrixRobot.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/MatrixSampleStateManager.cpp | COMPILE_FAIL | COMPILE_FAIL | windows.h: No such file or directory |
| MatrixGame/src/MatrixShadowManager.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/MatrixSide.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/MatrixSkinManager.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/MatrixSoundManager.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/MatrixTerSurface.cpp | COMPILE_FAIL | COMPILE_FAIL | windows.h: No such file or directory |
| MatrixGame/src/MatrixTransition.cpp | COMPILE_FAIL | COMPILE_FAIL | windows.h: No such file or directory |
| MatrixGame/src/MatrixVisiCalc.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/MatrixWater.cpp | COMPILE_FAIL | COMPILE_FAIL | windows.h: No such file or directory |
| MatrixGame/src/Text/Font.cpp | COMPILE_FAIL | COMPILE_FAIL | d3dx9core.h: No such file or directory |
| MatrixGame/src/Text/Parser.cpp | COMPILE_FAIL | COMPILE_FAIL | d3dx9core.h: No such file or directory |
| MatrixGame/src/Text/Render.cpp | COMPILE_FAIL | COMPILE_FAIL | d3dx9core.h: No such file or directory |
| MatrixGame/src/cheats.cpp | COMPILE_FAIL | COMPILE_FAIL | d3d9.h: No such file or directory |
| MatrixGame/src/input.cpp | COMPILE_FAIL | COMPILE_FAIL | windows.h: No such file or directory |
