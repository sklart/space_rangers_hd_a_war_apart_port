# E2E-5.19 — полный MatrixObject и shadow texture table

24.10.2026, продолжение `bcecf93`, локальный master, без push.

## Реально подключённый код

В native-cache включён **полный MatrixObject.cpp**: constructor/destructor,
resource demand, damage, animation state, logic, shadow, picking и draw methods.
Эти методы не заменены пустыми cleanup functions. Полнота компиляции не
означает, что объект или мир уже запущены.

Explicit include настоящего подготовленного ShadowProj.hpp восстанавливает
полный тип CVOShadowProj перед CMatrixShadowProj. Его layout не менялся;
существующие declarations и оригинальные inline методы сохранены. Нового
shadow SDK object или fake world не вводилось.

D3D sampler/render state и world/view/projection направлены в существующие
typed backends. LOD bias передаётся теми же битами float через std::bit_cast,
как уже принято для прочих map biases. Недостающие RotationX/RotationY/Scaling/
TransformNormal используют настоящий DirectXMath. LookAtLH и OrthoLH используют
принятые реализации. Исходные алгоритмы игрового объекта сохранены.

Shadow statics уже имели единственного владельца в GameStaticResources.cpp.
Он не менялся; дублирующих definitions в MatrixObject.cpp не добавлено.
Ни header CMatrixMapObject, ни core outputs не изменены.

## Что исполнилось

В общем host ASan/UBSan/leak-check и ARM64/QEMU bootstrap выполнены оригинальные
InitTextures(3), ClearTexture и повторные ClearTextures, по два цикла в каждом
из двух сеансов. Записи таблицы содержат настоящие caller-owned CTextureManaged.
ClearTexture обнуляет только совпадающие ссылки; ClearTextures освобождает
таблицу, **не уничтожая заимствованные текстуры**. Registry сохраняется до
отдельного настоящего Destroy текстур, затем возвращается к исходному размеру.

Численные проверки покрывают axis rotations, scaling и in-place TransformNormal:
translation игнорируется, перспективное деление и нормализация не добавлены.
Скалярные ожидаемые значения независимы от DirectXMath вызовов адаптера.

Настоящий CMatrixMapObject **не создавался**: destructor базового класса
обращается к живому g_MatrixMap/side, которого пока нет. Fake map не подставлялась.
Constructor/destructor, RNeed, Damage/LogicTakt, picking и shadow/GPU draw runtime
**NOT EXECUTED**. Полные методы лишь скомпилированы и участвуют в link closure.

MSVC: полный MatrixObject.cpp COMPILE PASS, runtime NOT RUN.
devkitA64: common-prefix COMPILE/LINK PASS, strong undefined=0. ELF SHA-256:
`6103a4c84e3dcb29d7e8e9bffe08370a87a4955a74c4eb284f46808e729fff7e`.
Switch/deko3d HARDWARE PENDING, новая NRO не создавалась.

Первая host-линковка подтвердила UBSan vptr retention для ещё не перенесённых
CMatrixRobot/CMatrixRobotAI/CMatrixEffectWeapon. Только для полного MatrixObject
добавлено ограничение vptr; ASan и остальные UBSan проверки сохранены. Прежние
Console globals и семь world/effect vptr исключений остаются. Fake RTTI нет.

## Настоящая следующая граница

Принудительная линковка полного map owner больше не требует отсутствующих
CMatrixMapObject::ClearTextures/destructor как внешних definitions. Теперь
исходный destructor удерживает настоящую vtable и все игровые callbacks.
Получено **38 уникальных unresolved symbols**: шесть прежних зависимостей и32
новые транзитивные. Уменьшение числа ошибок не подменяет перенос поведения;
полный объект раскрывает следующий реальный слой зависимостей.

Ближайшие lifecycle blockers:

- CVOShadowStencil constructor/destructor и DX_Prepare/Build/Render —
  MatrixLib/3G/ShadowStencil.cpp;
- CVOShadowProj constructor/DestroyTexture — MatrixLib/3G/ShadowProj.cpp;
- CMatrixShadowProj destructor и ShadowProjBuild* — MatrixGame/src/MatrixShadowManager.cpp;
- CMatrixProgressBar constructor/destructor — MatrixGame/src/MatrixProgressBar.cpp.

Также callbacks удерживают настоящие effect constructors, weapon/side methods,
map Trace, original VO Pick/PickFull, SSpecialBot и explosion definitions.
Полный точный список находится в [evidence](matrixgame-e2e519-map-object-evidence.json).
CGame::Init, RunGameLoop, CFormMatrixGame, мир и планетарный бой NOT EXECUTED.

Fresh CMake configure/generate PASS, полной повторной сборки не было. Оба
runtime завершились exit0, по два сеанса. Для InitTextures, ClearTextures,
ClearTexture и constructor/destructor подтверждено точное равенство исходных
тел методов upstream и подготовленного модуля; проверки не ограничены именами
файлов. Constructor/destructor равенство не считается доказательством исполнения.

Все245 engine files совпали в двух views, отдельно66 core outputs в трёх views.
Единственное изменение generated engine — MatrixObject.cpp; GameStaticResources
и header layouts не менялись. MSVC source SHA совпал с этим view. Evidence
собран до коммита, build_dirty=true; новые repo helper/test hashes указаны
отдельно. SDK compile/link не считается аппаратным runtime.

Основные raster/исторические ENOENT/freeze причины этим шагом не установлены.
Основная NRO/ELF, frozen kits, пользовательские сейвы/ресурсы и настройки Switch
не менялись: совпали13 доступных protected hashes,107 аппаратных файлов,
основная NRO/ELF и оба ZIP. G:/pack.zip недоступен;14/14 не заявляется.
Работа остаётся локальной, без push.
