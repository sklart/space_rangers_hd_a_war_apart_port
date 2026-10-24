# E2E-5.19: оригинальные Constructor, MultiSelection и Moveto

24.10.2026. Родитель: `2f93194`. Локальный этап в master, без push.

В native-cache включены полные исходные CConstructor.cpp (также
CConstructorPanel/SSpecialBot), MatrixMultiSelection.cpp и MatrixEffectMoveTo.cpp.
Это очередное устранение зависимостей настоящего владельца карты, не отдельная
имитация конструктора или боя. Методы рендера не вырезаны ради линковки.

## Изменения и проверка исходников

`tools/matrixgame_constructor_overrides.py` переводит SDK-вызовы на существующие
типизированные device/offscreen/math adapters. Сохранены аргументы, порядок
операций, ветви stencil, восстановление камеры/освещения, callbacks и ownership.
Эффект Moveto использует существующий GetManaged и Scaling. Статики
CMultiSelection остаются у прежнего владельца GameStaticResources.cpp;
дублирующие определения не добавлены. Заголовки, layout и ABI не менялись.

Проверены полные diff трёх effective CPP относительно ai-side-fixed-sdk/source,
включая Draw и cleanup, а не только список имён файлов. Различия ограничены
явными SDK/device/cache/math адаптациями и ASSERT_DX. Новые D3D9 значения
stencil/cull/fill/compare/clear/light/FVF/primitive, красный ARGB и значение PI
дополнительно проверены static_assert против настоящих D3D9/D3DX/DirectXMath
headers компилятором MSVC x86: compile exit 0. Это проверка констант, не GPU PASS.

## Реально выполненные проверки

- **devkitA64 compile/link PASS:** свежая сборка constructor-sdk, 146 jobs,
  bootstrap ELF, strong undefined 0. Новая NRO не выпускалась.
- **Host ASan/UBSan/LeakSanitizer PASS:** три новых полных CPP скомпилированы,
  включены в общий архив, bootstrap с cleanup/reentry завершён exit 0.
  Новых sanitizer exclusions нет.
- **ARM64/QEMU PASS:** те же три CPP compile/link и общий bootstrap exit 0.
  Для host/ARM64 использована актуальная база vector-group-fresh с исправленным
  uintptr ABI; неизменённые объекты переиспользованы, это не полная пересборка.
- **Map-owner link probe всё ещё FAIL:** 26 → 20 неразрешённых методов;
  закрыты семь связей, открыта CIFaceMenu::ResetMenu(bool). В обоих link logs
  нет duplicate/multiple-definition ошибок. Это проверка зависимостей,
  не создание карты и не успешная линковка полного мира.

Закрыты CConstructor destructor, CConstructorPanel::ResetGroupNClose,
Moveto constructor/Release, MultiSelection::FindItem/Remove и
SSpecialBot::GetRobot. Следующая группа — CIFaceList/CInterface/CIFaceMenu,
camera RestoreCameraParams/SAutoFlyData и cursor SetVisible. Полный перечень
20 методов с исходными link-командами находится в evidence JSON.

## Точная граница исполнения

Новые Constructor/MultiSelection/Moveto lifecycle, Takt, selection и Draw
**NOT EXECUTED**. CConstructor создаёт настоящий CMatrixRobotAI; деструктор
базового CMatrixMapStatic безусловно обращается к g_MatrixMap и player side.
Начало выделения требует map IsPaused, Moveto — map GetZ. Запускать их с
поддельной картой или добавлять ранние return ради теста не стали.

Установлен конкретный следующий rendering blocker: оригинальный
CMultiSelection::DrawPass1 вызывает Clear(1, &rect, ...), а
native-engine/offscreen_backend.cpp пока отвергает прямоугольники и поддерживает
только полный target. Вызов сохранён, не заменён очисткой всего экрана.
SET_SIGN_FLOAT в Moveto остаётся оригинальным; runtime его корректность
этим этапом не подтверждена.

Общий bootstrap подтверждает прежние реальные CPU/resource lifecycle и явно
заканчивает platform/device/window paths отказом Unsupported. **CGame::Init
NOT CALLED; полный мир, RunGameLoop, CFormMatrixGame и бой NOT RUN.**
Switch/deko3d — **HARDWARE PENDING**.

## Сохранность и ограничения основной игры

По SHA-256 неизменны 13 доступных protected inputs, 107 файлов аппаратного
прогона 23.10, frozen main NRO/ELF и оба hardware ZIP. G:/pack.zip недоступен,
его сохранность повторно не проверена. Пользовательские сейвы и игровые ресурсы
не изменялись. Push не выполнялся.

Источник повторяющегося текста в CPU composition, точные syscall/пути двух
исторических manual-publish ENOENT и причины полного зависания карты/выхода
остаются OPEN. Новых предположительных исправлений основной игры нет.

[Результаты, команды, SHA-256 и списки link blockers](matrixgame-e2e519-constructor-evidence.json).
