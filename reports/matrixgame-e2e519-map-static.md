# E2E-5.19 — полный static-object модуль и настоящий Windows offscreen backend

24.10.2026, продолжение `bfd122b`, локальный master, без push.

## Интеграция оригинального кода

Полный MatrixMapStatic.cpp подключён к native-cache, включая RenderToTexture,
list/sort/visibility, object loading и destructor. Алгоритмы выбора шасси/брони,
расчёта камеры и изменения изображения не заменялись. SortBegin,
RemoveFromSorted и EnumVertsHandler сравнены с предыдущим original source view:
тела совпадают целиком. Статические определения по-прежнему имеют единственного
владельца GameStaticResources.cpp; настоящий objects[] определён в полном TU.

Портретный путь использует настоящую original managed texture factory и typed
CreateLock overload. Добавленный LookAtLH делегирует DirectXMath. Новые SDK-free
offscreen value contracts направляют операции в настоящий IDirect3DDevice9 /
IDirect3DSurface9 на Windows: create target/depth, Get/Set target/depth,
describe, lock/unlock, scene, full-target Clear, viewport и Release.
Возвращаются настоящие SDK results; CPU поверхности под видом GPU не создаются.
Вне Windows отсутствующий backend явно отказывает, без fake render success.

## Что выполнено на host/ARM64

В каждом из двух сеансов общего bootstrap исполнены original SortBegin,
RemoveFromSorted на пустом списке и EnumVertsHandler для реальных vertex/matrix/
rectangle payloads: transformed point inside/outside и found flag.
Абстрактный CMatrixMapStatic и подставной мир не создавались. Через производный
abstract access type доступен только protected payload; объект этого типа не
инстанцируется. Непустой sorted list и object ctor/dtor **не проверены**.

LookAtLH проверен по преобразованию eye в origin и target на положительную
ось Z. Без SDK/device offscreen create/query/scene/clear вызовы действительно
отказывают, output surface остаётся nullptr. Это не выполнение RenderToTexture.

Host ASan/UBSan/leak-check и ARM64/QEMU: exit0. Новое vptr исключение только
для full MatrixMapStatic.cpp: failed link содержит RTTI references к ещё не
перенесённым robot/building/cannon/map-object классам. Address и остальные UBSan
checks сохранены. Offscreen backend не получил sanitizer исключений.
Прежние Console globals и Logic/Map/SkinManager/MatrixEffect/MatrixMapGroup vptr
ограничения остаются; full-engine sanitizer coverage не заявляется.

## Настоящий Windows SDK runtime

Полный static-object TU, backend и SDK-free caller скомпилированы MSVC x86.
Отдельный тест использует настоящий D3D9 HAL device, зарегистрированный через
production InitAsDLL/ReadyDevice. Сам игровой static-object TU не исполняется.

В двух циклах действительно выполнены:

- создание lockable A8R8G8B8 64×64 render target и D16 depth surface;
- описание ресурсов, bind target/depth, установка viewport, Begin/Clear/End;
- чтение всех пикселей после настоящего HAL Clear: ожидаемый ARGB подтверждён
  **до CPU записи**. Затем проверен pitch-aware CPU write/unlock/read roundtrip;
- точное восстановление исходных render target, отсутствовавшего depth и
  viewport, подтверждённое SDK readback; выход клиентских Surface owners и
  повторное создание. Это не полный GPU leak detector, но bindings проверены.

NULLREF здесь не выдаётся за PASS: GetRenderTarget вернул D3DERR_NOTFOUND.
Private HAL helper отличается от исходного SDK helper только device-type token
и подписью; это проверено текстовым сравнением. Исходный helper не менялся.
Первый HAL проход обнаружил ошибочную маршрутизацию нового viewport adapter в
неподдерживаемый generic video API. Исправлен именно offscreen boundary на
настоящий SDK SetViewport; затем весь сценарий прошёл. Это новый adapter defect,
не доказанная причина каких-либо дефектов основной игры. Failed logs сохранены.

Clear pixels подтверждены на Windows HAL. Рисование робота, его портрет,
полноценный scene draw и Switch/deko3d **не подтверждены**. RenderToTexture
целиком NOT RUN: настоящий мир/robot object отсутствует; generic
SetVideoLight/EnableVideoLight и прочие world/render зависимости ещё не закрыты.
Его исходные failure/cleanup paths не объявляются транзакционно исправленными.

## ARM64 сборка и точная граница

devkitA64: COMPILE/LINK PASS отдельного common-prefix ELF, strong undefined=0.
EnumVertsHandler/RemoveFromSorted retained. SHA-256:
`9b5d487dfeb57c61d41763cc665ff748463bf04bc5c256b36d91712253ff890f`.
Switch execution **HARDWARE PENDING**, NRO не создавалась. Full Windows common
bootstrap не слинкован; Windows SDK test является отдельной проверкой backend.

Настоящий CMatrixMapLogic ctor/dtor **LINK FAIL11**, ранее12:
RemoveFromSorted разрешён настоящим методом. Остаются derived object/side
destructors, ClearTextures, minimap и hint/interface cleanup. Точные symbols,
команды, source/object/log/binary hashes — в
[evidence](matrixgame-e2e519-map-static-evidence.json).
CGame::Init, RunGameLoop, CFormMatrixGame, загруженный мир и бой **NOT EXECUTED**.

Fresh CMake configure/generate PASS, три новых native/audit compile recipes
проверены; повторная полная сборка не выполнялась. 244 engine files совпали
в host, fresh CMake и MSVC source views; отдельно совпали66 core outputs.
От group stage изменился только MatrixMapStatic.cpp. Новые backend/math/test
файлы вне generated source manifest отдельно покрыты SHA-256. Upstream/core
не менялись. Evidence собран до коммита, build_dirty=true.

Совпали13 доступных protected inputs,107 аппаратных файлов, frozen main NRO/ELF
и оба аппаратных ZIP. G:/pack.zip недоступен. Сейвы, игровые ресурсы и настройки
Switch не менялись. Причины intro raster, двух historical manual-publish ENOENT
и зависаний карты/консоли остаются OPEN; новых аппаратных данных нет.
