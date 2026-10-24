# E2E-5.19 — полный исходный модуль миникарты

24.10.2026. Продолжение `d7a993a`, локальный master, без push.

## Подключённый код

В native-cache подключён **полный MatrixMinimap.cpp**, включая Init/Clear,
StoreTexture/RestoreTexture, геометрию, события, Draw/DrawRadar, чтение и запись
PNG-кэша, RenderBackground и RenderObjectToBackground. Это полнота компиляции,
а не утверждение, что все эти пути исполнялись.

Исходные алгоритмы и ветви отказов сохранены. Два CACHE_CREATE_TEXTURE
направлены в genuine CreateUnmanaged; SMMTex::Load использует typed GetManaged
вместо C-style cast portable cache entry. Layout CMinimap и Texture.hpp не
менялся. Глобальная portable factory не подменялась.

Прямые COM/API вызовы перенаправлены в существующие legacy_texture,
unmanaged_texture, offscreen, map_device и ReadyDevice contracts. Добавлены
два настоящих SDK вызова: GetRenderTargetData и DrawPrimitiveUP; вне Windows
они явно отказывают Unsupported. Никакой фиктивной поверхности или карты нет.
Настройка SPECULARMATERIALSOURCE=146 сверена с установленным официальным
Windows SDK d3d9types.h. D3D9Device пока не поддерживает все generic
material/light операции, поэтому полный рендер миникарты не объявляется рабочим.

Недостающая математика Translation/OrthoLH/Vec2TransformCoord и strided XY
использует настоящий DirectXMath. Только XY записываются в interleaved vertices.

## Проверенная граница выполнения

В общем host bootstrap ASan/UBSan/leak-check и ARM64/QEMU выполнены:

- настоящий CMinimap ctor, повторные Clear и пустые Store/Restore;
- SetOutParams, исходные SetAngle/PauseTakt и исходная формула масштабирования;
- Init создаёт настоящий caller-owned CTexture, затем original BlockGet
  отклоняет отсутствующую секцию в контролируемой пустой конфигурации;
- Clear освобождает созданную текстуру, повторная очистка безопасна, registry
  возвращается к исходному размеру; повторены два объекта в каждом из двух
  сеансов общего bootstrap;
- strided in-place XY transform проверен по независимым скалярным формулам,
  включая perspective divide и сохранность соседних color/UV.

Это проверка original Init prefix и cleanup после реального отказа. Успешный
Init на ресурсах мира, непустой StoreTexture/RestoreTexture, PNG-кэш и полный
Draw **NOT EXECUTED**. Мир и g_MatrixMap не подменялись.

На Windows MSVC полный модуль скомпилирован. На настоящем D3D9 HAL отдельный
существующий offscreen harness проверил **GetRenderTargetData**: GPU Clear →
system-memory texture → проверка всех пикселей с учётом pitch, два цикла.
Это настоящая backend-предпосылка миникарты, а не выполнение её полного GPU
пути. Полный Windows common game bootstrap не запускался.

devkitA64 common-prefix COMPILE/LINK PASS, strong undefined=0. ELF SHA-256:
`32ef344fecd4596453134ffd53f987dde5f111e52d451abf5a56b15fb3563a7c`.
Новая NRO не создавалась. Switch/deko3d runtime HARDWARE PENDING.

## Ограничение sanitizer и ошибки сборочной пробы

Первая линковка показала retention UBSan vptr metadata с отсутствующими
typeinfo CMatrixBuilding/CMatrixCannon/CMatrixRobot/CMatrixRobotAI. Для полного
MatrixMinimap.cpp добавлено такое же ограничение **только vptr**, как у шести
ранее подключённых world/effect TUs. ASan и остальные UBSan проверки сохранены;
прежнее ограничение console globals остаётся. Fake RTTI не вводилась.

В той же первой пробе скрипт добавил новый offscreen_backend.cpp.o рядом со
старым offscreen.o; duplicate definitions устранены точной заменой archive
member. Эта ошибка private runner не была ошибкой runtime. Первые compile/link
логи сохранены. В официальном CMake backend входит один раз.

## Оставшаяся граница

Принудительное подключение полного владельца карты теперь даёт LINK FAIL10
вместо FAIL11: CMinimap::Clear закрыт оригинальным методом. Остаются:

- CIFaceList::HideHintButtons;
- CMatrixBuilding, CMatrixCannon, CMatrixFlyer, CMatrixRobotAI,
  CMatrixSideUnit — destructors;
- CMatrixMapObject::ClearTextures и destructor;
- CMatrixHint::SoundOut и destructor.

CGame::Init, RunGameLoop, CFormMatrixGame, загруженный мир и планетарный бой
по-прежнему NOT EXECUTED. Основные P0 причины raster/исторических ENOENT/freeze
не установлены этим шагом. Никаких предположительных правок основной игры нет.

Команды, реальные результаты, source manifests, ограничения и hashes находятся
в [evidence](matrixgame-e2e519-minimap-evidence.json). Оба runtime завершились
с exit0, по два сеанса. Compile/link не считается аппаратным runtime.

Fresh CMake configure/generate PASS; обе рецептуры полного MatrixMinimap.cpp
проверены, повторная полная CMake-сборка не выполнялась. Все245 engine files
совпали в двух views, отдельно66 core outputs в трёх views; относительно
принятого шага изменился только MatrixMinimap.cpp. Host/ARM64/devkitA64/MSVC
использовали этот engine view, MSVC include также направлен в его portable-core.
Evidence собран до локального коммита, build_dirty=true, hashes repo helpers
и тестов указаны отдельно. Старые SDK helper objects переиспользованы только
в ранее принятом standalone HAL harness; это не full-engine Windows link.

Снова совпали13 доступных protected hashes,107 аппаратных файлов, основная
NRO/ELF и оба аппаратных ZIP. G:/pack.zip недоступен;14/14 не заявляется.
Пользовательские сейвы, ресурсы и настройки Switch не менялись.
