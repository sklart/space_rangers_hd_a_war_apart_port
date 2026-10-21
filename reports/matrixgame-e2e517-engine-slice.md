# E2E-5.17 — исполнение оригинального CVectorObject без GPU

21.10.2026. Работа выполнена офлайн в `master`, без push и без доступа к Switch.
База: `df48ae6eb77eff72cccedf9a23e2264fc0e28cff`.
Код среза: `b21d6eb1ad6d1160010a8ea3f1c9982dfaa68dab`.
MatrixGame pin неизменён: `218115a72fa9c5d43269f4ca84a8596a0bcb82f1`.
Машинные доказательства: [evidence JSON](matrixgame-e2e517-evidence.json).

## Новый реально работающий путь

До этого этапа native VectorGeometry загружал данные VO, а настоящий
CVectorObject не мог компилироваться из-за заголовков D3D9. Теперь выполнен
явно выделенный CPU-срез исходного класса с его собственным `m_Geometry`:

```text
real CHeap + buffers::Context/Scope
  → CVectorObject::StaticInit (исходный inline reset m_VB/m_IB)
  → исходный конструктор CVectorObject / CCacheData / SRemindCore
  → исходный virtual Load
  → LoadSpecial(OLF_NO_TEX)
      → настоящий CStorage::Load на read-only копии VO
      → проверка storage существующим VectorGeometry reader
      → исходные копирования/разбор geometry, frames, anims, matrices,
        properties, surfaces и регистрация источников в BigVB/BigIB
  → исходные EnumFrameVerts / GetBound / GetAnimById, GetAnimByName /
    GetMatrixById, GetMatrixByName
  → Unload → повторная Load того же объекта → Unload → деструктор
```

Это **не весь static_init() из MatrixGame.cpp**, не CGame::Init и не GPU draw.
StaticInit CVectorObject не имеет GPU prerequisite: он лишь сбрасывает два
статических указателя. Контекст пробы предоставляет реальный heap и существующий
CPU staging backend; полный CacheInit, skin/material и device не имитируются.

В подготовщике проверяются SHA закреплённых входов. `VectorObjectCpu.hpp`
содержит исходные поля класса и объявления; `VectorObjectCpu.cpp` извлекает
указанные исходные методы из VectorObject.cpp. D3D-зависимые Draw/Shadow/Pick/
VectorObjectAnim не извлекаются и не получают пустых реализаций. DrawBegin
остаётся объявлением без CPU-определения; запрос не-OLF_NO_TEX явно выдаёт
`UNSUPPORTED`. Полный engine продолжает видеть оригинальный VectorObject.hpp.
CPU target нельзя выдавать за весь VectorObject.cpp или смешивать с его полной
реализацией в одном link. Fake d3d9.h, CGame и результаты боя не добавлялись.

## Адресные изменения и найденный дефект

- DWORD/WORD/BYTE в CPU-срезе заменены фиксированными типами. Layout selector
  XYZ/NORMAL/TEX1 сохранён как `0x112`; это метаданные существующего BigVB.
- Отделены Windows/D3D includes и пустой release DTRACE; heap, CStorage,
  CBlockPar, SRemindCore и BigVB/BigIB остаются настоящими реализациями.
- Добавлен настоящий CPU `D3DXVec3TransformCoordArray` через DirectXMath:
  strided memcpy и XMVector3TransformCoord. Исходный GetBound по-прежнему
  преобразует восемь углов и выбирает min/max.
- Перед raw-копированием проверяется тот же CStorage существующим checked
  reader, дополнительно ограничены имена anim/matrix до 31 UTF-16 units.
  Он не заполняет `m_Geometry` вместо исходного LoadSpecial. Ошибки отсутствия
  файла и short resource отвергаются до allocations геометрии.
- **Воспроизведено UBSan:** после Unload последнего источника `m_VB/m_IB`
  уничтожены, а повторная Load того же объекта обращается к нулевому m_VB.
  Исходный код создаёт буферы только в конструкторе. В CPU overlay тот же блок
  создания перенесён в LoadSpecial после проверки ресурса. Повторная загрузка
  и деструктор после неудачной первой загрузки теперь проходят. Это изменение
  времени владения ресурсом, не алгоритма геометрии. Arbitrary OOM/fault
  recovery после начала raw allocations в этом шаге не обещается.

Существующий дефект внутреннего Insert CList не менялся; логика RNG, боя,
сериализация, таймер, сохранения и основной renderer не редактировались.

## Инвентаризация и точная compile delta

Один baseline audit GCC 15.2, C++20, RelWithDebInfo, ASan/UBSan выполнен по
реальным CMake recipes:

```sh
cmake -S port/matrixgame/native-engine -B build/matrixgame/e2e517/host \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
python3 tools/matrixgame_engine_compile_audit.py \
  --build-root build/matrixgame/e2e517/host \
  --output build/matrixgame/e2e517/baseline \
  --target matrixgame-native-engine --target matrixgame-native-matrixlib-audit --jobs 4
```

**13 COMPILE_PASS / 93 COMPILE_FAIL из 106 выбранных units.** Отдельный
MatrixRoadNetwork исключён нынешним CMake из full-engine target и здесь не
перетестировался. Старый audit `build/e2e515/mg-compile-big-buffers/result.json`
содержал 107 units: единственный отсутствующий теперь unit — RoadNetwork;
из общих units CList.cpp перешёл FAIL → PASS ещё благодаря E2E-5.16.1.
Остальные compile-статусы общих units не изменились. Старое 13/107 не является
нынешней метрикой; эти числа не оценивают процент готовности игры.

Добавлен отдельный target `matrixgame-native-vector-object`: подготовленный
**CPU-поднабор исходного VectorObject.cpp** действительно скомпилирован,
связан с native-core и вызван resource probe. Его compile/link recipe и
object/executable inputs сохранены. **Новый полный inventory не запускался;
14/106 не заявляется.** Полный VectorObject.cpp остаётся D3D BLOCKED.

Baseline recipes/логи и снимок исходников сохранены отдельно в
`build/matrixgame/e2e517/baseline/` и `baseline-source/`; candidate inputs —
в `host/core/source/`, compile_commands — в `host/compile_commands.json`.
SHA effective CPU cpp/hpp совпадают между host, ARM64/QEMU и SDK views.

## Runtime и проверки

Read-only копии ранее извлечённых легальных ресурсов: dagger00.vo (53 вершины),
wood05.vo (103), Terron.vo (108008). Оригинальные SHA и имена внутри robots.pkg
в evidence, bytes не добавлены в Git. Два ограниченных lifecycle-цикла:
по 104 кадра, всего **208 проверенных кадров**. Сравниваются все перечисленные
вершины кадра, frame metadata, анимации/тайминги и матрицы с их исходными
storage-данными. GetBound проверен со scale/translation, tolerance 0.002.
Это не произвольная битовая эквивалентность всех float операций D3DX9.

| Gate | Фактический результат |
|---|---|
| Host GCC 15.2, весь связанный native-core + новый срез ASan/UBSan | HOST_RUNTIME_PASS, leak detection, 2 цикла / 3 модели / 208 кадров |
| Missing resource, 47-byte short VO, повторная загрузка после отказа | PASS, управляемый отказ и очистка; записи только в fresh build runtime |
| Linux ARM64 GCC 15.2 / QEMU | CPU_RUNTIME_PASS, та же ресурсная последовательность |
| devkitA64 16.1 + libnx, отдельный CPU ELF | COMPILE_LINK_PASS, undefined strong symbols: 0; не запускался |
| CGame::Init → RunGameLoop → CFormMatrixGame | BLOCKED / NOT CALLED |
| Switch GPU / основной аппаратный прогон | HARDWARE PENDING |

У probe `-UNDEBUG` стоит после `-DNDEBUG`; ASan/UBSan ошибки не подавлялись.
Не повторялись прежние CList, BigVB/BigIB, navigation и CMain/Form runtime
контракты. Windows x86 reference в этом шаге не запускался: новый GPU-less
срез не равен исходному Windows device lifecycle.

Сохранены неуспешные попытки: отсутствовавший math-array helper
(`compile-vo.log`), null shared VB (`runtime-candidate-1/runtime.log`), затем
ошибочное ожидание теста об уникальности matrix names (`runtime-final/runtime.log`).
Последнее исправлено в проверке: исходный GetMatrixByName возвращает первое
совпадение имени, GetMatrixById — первое совпадение ID; алгоритм не изменён.
Итоговые PASS: `runtime-verified/runtime.log`, `arm64/runtime/runtime.log`,
`sdk-final/result.json`. Скриптовая ошибка кавычек пути Git исправлена до
ARM64-компиляции; исходники SDK/Linux от неё не менялись.

### Воспроизведение нового среза

Из корня checkout в WSL, после configure выше:

```sh
cmake --build build/matrixgame/e2e517/host --target matrixgame-native-vector-object-probe --parallel 4
python3 tests/probe_matrixgame_vector_object.py \
  --binary build/matrixgame/e2e517/host/matrixgame-native-vector-object-probe \
  --model build/e2e515/big-buffer-models/model-0.vo \
  --model build/e2e515/big-buffer-models/model-1.vo \
  --model build/e2e515/big-buffer-models/model-2.vo \
  --output build/matrixgame/e2e517/fresh-host-runtime
bash tools/matrixgame_vector_object_arm64.sh build/matrixgame/e2e517/fresh-arm64 \
  build/e2e515/big-buffer-models/model-0.vo \
  build/e2e515/big-buffer-models/model-1.vo \
  build/e2e515/big-buffer-models/model-2.vo
```

Output должен быть свежим. Вместо локальных моделей допустимы свои read-only
копии тех же ресурсов. ARM64 helper проверяет pin/чистоту ранее подготовленного
zlib и использует существующий ARM64 libz.a, без сетевого скачивания/пересборки.
В Windows: `python tools/matrixgame_navigation_sdk.py --contract vector-object
--output build/matrixgame/e2e517/fresh-sdk`; это только compile/link.

SHA-256 host executable:
`814d9611fe6416a8226cc0986559bfe1ed98638e864312e20c8a73a3862d8055`.
Linux ARM64: `6b131b5524d3800d5d9fdf04cc56917350c7b9d31585866b425013e6050d91b0`.
SDK ELF: `e9c0613568db9864efe6b9c3bef2de11225e006db1071b552a312802213da924`,
Build ID `8fdd0215544bb0acc62fc99f924f07bbe41d4114`.

## Дальнейший оригинальный call graph и первый blocker

В DLL entry (`MatrixGame/src/MatrixGameDll.cpp:79`) порядок такой:
`CGame::Init → CFormMatrixGame → RunGameLoop → SaveResult/SafeFree`.
Внутри Init (`MatrixGame.cpp:172`) сначала RNG seed и `static_init()`
(BaseInit, VectorObject, ShadowProj/Stencil, Form и игровые statics), затем
heap/robots.pkg/config → CacheInit → L3GInitAsDLL → render pipeline/settings →
CMatrixMapLogic/LoadSide/RobotPreload → CStorage карты → PrepareMap.
Этот полный путь данным probe не выполнялся.

**Один следующий инженерный blocker:** native cache/material и device lifecycle
на переходе `CacheInit → L3GInitAsDLL`, нужные для настоящего CGame::Init.
Его нельзя закрыть успешным возвратом из headless-функции.

| Файлы | Конкретная причина |
|---|---|
| `MatrixLib/3G/VectorObject.hpp:8`, `VectorObject.cpp` | Полный класс/TU всё ещё включает D3D9; DrawBegin/Draw/BeforeDraw, skin passes и shadows требуют device/state. CPU-поднабор не закрывает эту часть |
| `MatrixLib/3G/Cache.cpp:201–327` | Полная фабрика/CacheInit связана с CTexture/CTextureManaged/3g; CPU probe задаёт heap явно, не заменяет эту фабрику |
| `MatrixLib/3G/3g.cpp:346`, `D3DControl.hpp` | L3GInitAsDLL заимствует IDirect3D9/Device9, Win32 HWND/WndProc, caps/gamma; для этого нет законченного native ownership/lifecycle contract |
| `MatrixGame/src/MatrixGame.cpp:293–365` | Init требует этот lifecycle, CRenderPipeline и полный PrepareMap до допустимого Run. Сейчас первая compile-ошибка — VectorObject.hpp/d3d9.h |
| `MatrixLib/3G/3g.cpp:439`, `MatrixGame.cpp:778` | RunGameLoop вызывает FormChange и настоящий L3GRun; нужны platform events/timing/device и активная игровая карта |
| `MatrixGame/src/MatrixFormGame.cpp:196,238,333` | Enter задаёт материал/device state; Draw и Takt требуют реальный map/objects/AI. Сейчас тот же первый compile blocker d3d9.h |

Существующий deko backend умеет отдельный mesh draw/readback, но не весь этот
state/lifecycle. Никакие Init/Run/победа в текущем этапе не симулировались.
Дальнейшее разветвление на десятки utility-переносов здесь остановлено.

## Основная игра, данные и вечер

13 важных входов (пользовательские saves/config/log, untracked provenance,
G:/pack.zip, три исходные VO и неизменный E2E-5.16 ZIP) имеют одинаковые
SHA-256 до/после. Подмодуль MatrixGame чист, gitlink прежний. В этом шаге не
редактировались main-game effective generators, port/switch, save/Unicode/
transaction-recovery, ввод, аркада/Timer, renderer или игровые ресурсы.
Все runtime-записи — в отдельных ignored build dirs; модели пробы read-only.

NRO не пересобиралась. Основной baseline остаётся `03651ce`, ZIP SHA
`e1a1dec5f410b983d629ae538ad9b4ba063f89a8101af43d27ea00c6ce6a08a4`.
Существующий отдельный GPU smoke также проверен по ZIP/NRO/ELF hash; новый
CVectorObject CPU-срез в него не включён. Основной README не переписывался.
Повторяющиеся фоны и задержки карты остаются без причины/HARDWARE PENDING.

[Короткая вечерняя инструкция](../docs/e2e517-evening-switch-check.md)
разделяет основной baseline и отдельный GPU smoke с точными именами логов.
Установленные OC/Atmosphère/Horizon параметры не менялись.
