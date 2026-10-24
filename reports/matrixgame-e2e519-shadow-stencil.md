# E2E-5.19 — оригинальный ShadowStencil на реальной VO

24.10.2026, продолжение `f7abfaa`. Подключён полный MatrixLib/3G/ShadowStencil.cpp.
Алгоритм silhouette/extrusion и DX_Prepare сохранён: нормализованный текст
Build совпадает с предыдущим original prepared source после обратной замены
только sign-bit std::bit_cast и написания compiler stack-allocation intrinsic.
Constructor/destructor, MarkAllBuffersNoNeed и DX_Prepare совпали точно.
Единственные statics остаются в существующем GameStaticResources.cpp.

Исходные render calls направлены в настоящие typed device/map backends.
Stack lifetime сохранён через compiler alloca intrinsic, не через возвращённый
из helper указатель. В ShadowStencil.hpp добавлен read-only diagnostic friend
и static reader исходной геометрии через уже существующую дружбу CVectorObject.
Поля и virtual entries не менялись; CVectorObject/core не менялись.

## Реальное выполнение

Первый runtime input wood05.vo не имеет edge section. Его нулевой edge count
нарушил предусловие теста: это не принято за подтверждение shadow geometry.
Использован уже сохранённый настоящий `Matrix/Obj/detail/Terron.vo`, SHA-256
`b7cba8a78faf0c8cf32cf942288d2fdb0d5d21b047d0e9abaa212092c11d9ccd`.
Payload остаётся приватным и не входит в Git. Дополнительная VO suite не запускалась;
на том же общем bootstrap host binary изменён только resource argument.

Host ASan/UBSan/leak-check и ARM64/QEMU прошли по два сеанса:

- original Build frame0: **1470 vertices,4572 indices**, оба направления invert;
- независимый скалярный signed-dot oracle на настоящих edge normals проверил
  отбор рёбер, позиции extrusion и все индексы/winding;
- original DX_Prepare создал настоящие dynamic CPU staging VB/IB; каждый byte
  совпал с геометрией, диапазоны помечены initialized;
- повтор того же Build сохранил buffer identity/version, исходный cache работает;
- MarkAllBuffersNoNeed, повторные DX_Free, создание буферов заново и destructor
  вернули staging allocation и исходный linked-list count к baseline;
- original Render вошёл в настоящий device boundary и получил Unsupported
  до draw. Fake device, fake map и фиктивная GPU submission не использовались.

Геометрия и upload в CPU staging исполнены. GPU upload/draw, мир и бой
**NOT EXECUTED / HARDWARE PENDING**. Проверен один настоящий ресурс/frame0,
не все модели/кадры/направления освещения. OOM injection не выполнялась.

MSVC: полный ShadowStencil.cpp COMPILE PASS, runtime NOT RUN.
devkitA64: общий prefix COMPILE/LINK PASS, strong undefined=0. ELF SHA-256:
`e9d69ea4313adb0fb099063b0a354ac8316da6679e482b7b2f0e1147c463c3da`.
Новой NRO нет. Новых sanitizer exclusions нет; прежние Console globals и восемь
world/effect vptr ограничений сохраняются, ShadowStencil TU проверяется полностью.

Fresh CMake configure/generate PASS; обе ShadowStencil recipes сохраняют
function/data sections и vptr. Полной повторной CMake-сборки не было. Все245
engine files совпали в двух views; изменились только ShadowStencil.cpp/.hpp.
Отдельно66 core outputs в трёх views неизменны. Новый bootstrap driver собран
для host/ARM64/devkitA64; прежний driver не выдаётся за проверку нового вызова.

## Следующая граница и сохранность

Map-owner LINK FAIL33 вместо38: закрыты все пять отсутствовавших методов
CVOShadowStencil. Остаются ShadowProj/ShadowManager, progress bars, effect/side/
robot methods, original VO Pick/PickFull и map Trace. Точный список, команды,
source hashes и пределы приведены в [evidence](matrixgame-e2e519-shadow-stencil-evidence.json).
CGame::Init, RunGameLoop, CFormMatrixGame и полный map owner не запускались.

Совпали13 доступных protected hashes,107 аппаратных файлов, frozen основная
NRO/ELF и оба ZIP. G:/pack.zip недоступен;14/14 не заявляется. Сейвы/ресурсы и
настройки Switch не менялись. Evidence собран до коммита, build_dirty=true.
Причины raster/исторических ENOENT/freeze основной игры остаются OPEN.
