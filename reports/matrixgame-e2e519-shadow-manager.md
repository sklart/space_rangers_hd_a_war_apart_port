# E2E-5.19 — полный shadow manager и оригинальный расчёт shadow basis

24.10.2026, продолжение `0ee3ae9`. В существующий native-cache target включён
полный `MatrixShadowManager.cpp`. Устранены настоящие compile blockers:
header теперь включает полные ShadowProj-типы, world transform и texture bind
используют существующие typed device backends, normal/look-at — существующие
DirectXMath adapters, stack allocation — compiler intrinsic в вызывающем frame.
`CACHE_CREATE_TEXTURE()` заменён настоящей фабрикой `CTexture`, без приведения
portable CacheTexture к оригинальному типу.

Для закрытия зависимостей ShadowProjBuildGeom подключены оба оригинальных
`CVectorObject::CalcShadowProjMatrix` в generated OriginalShadowProjection.cpp.
Это методы существующего оригинального класса, используемые настоящим
`CVectorObjectAnim`; другого loader, shadow algorithm или fake map нет.
Все13 тел manager совпали с original после только указанных adapter substitutions.
Оба projection bodies совпали после обратной замены LookAtLH adapter и написания
matrix multiply через уже принятый настоящий matrix operator. Geometry clipping,
strip construction, выбор texture branches и результаты боя не подменялись.

## Реально исполненный путь

В общий bootstrap добавлен адресный test. Он использует уже загруженную настоящую
Terron.vo, frame0 и последний frame, два направления света. Оригинальный animator
вызывает одиночный CalcShadowProjMatrix; static overload обрабатывает два настоящих
animators с nonuniform scale/translation и rotation/shear/translation world matrices.
Все четыре выходных vectors сверены с независимым scalar-double oracle:
camera axes, восемь углов каждого исходного frame bounds и их affine transforms.
Oracle не вызывает production GetBound, inverse matrix или DirectXMath.
Допуск на компонент: absolute5e-4 + relative2e-5. Исходный light direction не меняется.

Настоящий CMatrixShadowCliper создаётся на оригинальном CVOShadowProj. Исполнены
constructor, BeforeRender, Render, destructor и повторный вход. Методы доходят
до настоящих layout/world-transform операций и получают Unsupported без SDK device;
GPU success или submission не синтезируются. Общий parent cleanup завершается.

Host ASan/UBSan/leak-check и ARM64/QEMU — PASS, по два сеанса с активными assertions.
Standalone VO lifecycle, CList, BigVB/BigIB и RoadNetwork suites не повторялись;
новые вызовы включены в существующий интеграционный bootstrap. Новых sanitizer
exclusions нет, прежние ограничения bootstrap сохраняются.

devkitA64 prefix compile/link PASS, strong undefined=0; runtime NOT RUN.
ELF SHA-256: `aecc1f033c611d3145a68664c5e4f3151479b904b770791a2c3048b614bab5bb`.
MSVC: полный manager и оба selected projection methods COMPILE PASS, runtime NOT RUN.
CMake configure/generate PASS; полной повторной CMake-сборки не было.
Два engine views246 совпадают. Изменены только manager CPP/header и добавленный
OriginalShadowProjection.cpp;66 core outputs в трёх views остались прежними.

Точные команды, baseline errors, source/body checks, runtime logs и hashes:
[evidence](matrixgame-e2e519-shadow-manager-evidence.json).

## Точная оставшаяся граница

Force-link оригинального map owner — **27 unresolved вместо29**. Закрыты:

- CMatrixShadowProj destructor;
- ShadowProjBuildGeom;
- оба используемых overloads ShadowProjBuildTexture.

Открылись две следующие настоящие транзитивные зависимости:
`CVectorObject::CalcShadowTexture` и `CalcShadowTextureWOMat`.
Остальные блокеры: effect/weapon owners, side/robot/building/cannon/flyer owners,
interface hint cleanup, VO Pick/PickFull, map Trace и explosion definitions.
Полный список есть в evidence; отрицательный map link не считается запуском мира.

Terrain clipping/strip generation, ReadGeometry и map-dependent destructor
скомпилированы, но не исполнялись: настоящая карта ещё не создана.
GPU shadow texture generation, draw, CGame::Init, RunGameLoop, CFormMatrixGame
и бой NOT EXECUTED / HARDWARE PENDING. Нового NRO нет.

Frozen основная NRO/ELF, оба аппаратных ZIP,13 доступных protected inputs и107
аппаратных файлов сверены без изменений. G:/pack.zip недоступен и не считается
проверенным. Сейвы/ресурсы и настройки Switch не менялись. Push не выполнялся.
Причины raster, исторических ENOENT и freezes основной игры остаются OPEN.
