# E2E-5.19 — полный CVectorObjectGroup и сквозные pointer payloads

В общий native-cache включён **полный непрерывный оригинальный блок**
CVectorObjectGroupUnit/CVectorObjectGroup из VectorObject.cpp: создание и удаление
units, load/config, links, animation, matrices, bounds, picking, vertex enumeration,
rendering и stencil methods. Это отдельная иерархия от ранее перенесённых групп карты.
Алгоритмы не заменялись; добавлены существующие TransformNormal/world adapters.

## Доказанная ошибка и исправление

На реальной Terron VO новый общий тест воспроизвёл потерю старших 32 бит:
expected `72e241fce460`, vertices `41fce460`, anim_draw `41fce460`.
Оригинальная группа перечислила 3498 вершин; оригинальная Anim → VO draw chain
вызвала три SSkin callbacks и выдала пять настоящих mesh ranges.
Baseline завершился контролируемым exit 1, завершено 0 сессий. Неверный адрес
не разыменовывался. Один и тот же test source используется до и после исправления.

Причины:

- Group EnumFrameVerts принимал uint32_t и сохранял payload в uint32_t,
  хотя Building передавал указатель, а конечный handler уже принимал uintptr_t.
- Group Draw → inline Anim Draw → CVectorObject Draw принимали uint32_t,
  хотя producer передавал texture pointer, а SSkin callbacks уже принимали uintptr_t.

Вся цепочка приведена к uintptr_t: declarations, definitions и локальное
SEVH_data_group storage. SKIN_GET flags/gsp, colors и прочие 32-битные данные
не менялись. Размеры объектов не менялись; все потребители изменённых сигнатур
пересобраны в новых директориях, старые native objects не подмешивались.

## Реально работающая часть

В общий bootstrap включён оригинальный group lifecycle с настоящим уже загруженным
CVectorObject: Add, lookup по id/name, Init/SetAnimDefault, RNeed matrices,
BoundGetAllFrame с проверкой инварианта сдвига на 100, Takt, Delete/Clear и reentry.
Проверены полные реальные адреса через group vertex enumeration и Anim → VO →
SSkin callbacks. После совпадения адреса callback читает marker по нему.

- **Свежий host ASan/UBSan + LeakSanitizer: compile/link/runtime PASS**, exit 0,
  две сессии, четыре high-address round trips. Восемь прежних vptr exclusions
  остаются в других TUs, новых нет.
- **Свежий Linux ARM64/QEMU: compile/link/runtime PASS**, exit 0, две сессии,
  четыре high-address round trips. Все native objects собраны заново.
- **devkitA64: полная свежая compile/link PASS**, strong undefined 0.
  Switch runtime HARDWARE PENDING, NRO не создавалась.
- **Реальный MSVC Win32:** canonical member signatures до/после идентичны;
  sizeof VO=240, Anim=40, Group=52, Unit=276, pointer=4 — без изменений.
- **Production ARM64 map-owner probe:** 49 → 34 unresolved, закрыты 15 связей
  CVectorObjectGroup, новых нет. Карта не создавалась.

## Effective sources и ограничения

Полный group block побайтово совпадает с исходным prepared блоком после обращения
двух SDK adapters. Baseline → fixed source changes состоят только из типов
opaque parameters/storage. Два core implementation/header outputs изменены;
379 CPP/HPP/H engine+core inputs совпадают между SDK и обеими свежими Linux-сборками.

Проверены фактические recipes, наличие compiled objects и production archive members:
OriginalVectorGroup.cpp, OriginalVectorAnim.cpp и VectorObjectCpu.cpp.
Raw VectorObject.cpp имеет recipe отдельного EXCLUDE_FROM_ALL matrixlib-audit,
но его объект не собран и в production libraries не входит. Наличие такого recipe
в compile_commands не считается компиляцией полного raw VectorObject.cpp.

Group Load/config, linked units, picking и stencil/GPU branches этим тестом не
исполнялись. SSkin callbacks здесь наблюдают настоящий opaque payload и CPU mesh
emission; графические операции и успешное GPU drawing не имитируются.
Сам Group Draw требует настоящий device и не запускался; его forwarding signature
исправлена, а следующая Anim/VO/callback часть реально проверена.

Полные CGame::Init, RunGameLoop, CFormMatrixGame, загрузка мира и бой ещё не
выполнялись. Следующие владельцы: Side/UI, CInfo/Group, Dust, SAutoFlyData,
SSpecialBot и camera restore. Точные 34 symbols приведены в evidence JSON.

## Сохранность

13 доступных protected inputs, 107 аппаратных файлов, frozen main NRO/ELF и оба
hardware ZIP сверены по исходным SHA-256: изменений нет. G:/pack.zip недоступен.
Сейвы, ресурсы и настройки консоли не менялись. Push не выполнялся.
Источник CPU-композиции повторяющегося текста, исторические manual-publish ENOENT
и причины зависаний основной игры остаются OPEN.

[Команды, before/after runtime, source diff, manifests и SHA-256](matrixgame-e2e519-vector-group-evidence.json).
