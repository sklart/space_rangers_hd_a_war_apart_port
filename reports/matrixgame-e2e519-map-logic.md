# E2E-5.19 — полная MatrixLogic.cpp, граница карты ещё открыта

23.10.2026, локальный `master`, без push/fetch. Основная игра и аппаратные
комплекты не пересобирались. Это продолжение интеграции оригинального движка,
не новый отдельный CPU-прототип и не принятие полного `CGame::Init`.

## Реальное изменение

Полная `MatrixGame/src/MatrixLogic.cpp` (около3500 строк) переведена из
COMPILE_FAIL в COMPILE_PASS и включена в существующий `matrixgame-native-cache`.
Тело CMatrixMapLogic constructor/destructor и все исходные методы сохранены;
выборка одного метода или подмена класса не создавалась. В общем bootstrap
реально вызван оригинальный `OptimizeMovePath_Delete`, включая перекрывающиеся
участки массива пути. Конструктор карты при этом **не вызывается**.

Сняты следующие платформенные зависимости:

- UnionRect/LPRECT заменены типизированным UnionBounds для исходного CRect.
  Сохранены empty/inverted bounds и alias destination. Три случая сверены с
  настоящим User32 UnionRect на Windows и с helper в host/QEMU.
  [Контракт Microsoft](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-unionrect).
- Три настоящих MoveMemory вызова используют std::memmove, сохраняя overlap;
  закомментированный старый код не переписывался.
- Два _alloca сохраняют память в кадре **вызывающего** метода: macro выбирает
  MSVC _alloca или GCC/Clang builtin. Функция, возвращающая чужой stack pointer,
  не вводилась. Полный MSVC compile новой TU в этом шаге NOT RUN.
- D3DMATERIAL9 заменён существующим VideoMaterial; прежние нули и значения
  Diffuse/Ambient/Specular сохранены. SetMaterial перенаправлен в прежний
  Device::SetVideoMaterial, без изменения порядка и игнорирования результата.
  Сейчас этот provider сохраняет явный Unsupported: настоящая material
  submission не реализована и в данном шаге не исполнялась.

Навигационные алгоритмы, RNG, Takt, условия победы, объекты/ресурсы не заменены.
Из240 prepared engine files поменялся только MatrixLogic.cpp, SHA256
`49aba32589a010abcb79a84d0ed2418e6743a57cae2b87ffa8038380524328b1`.
Добавленный backend header находится вне generated дерева. Сверены все
остальные prepared bytes host/ARM64/SDK перед повторным использованием объектов.

## Проверки и их пределы

Один compile baseline проверил две следующие полные TU: MatrixLogic и
MatrixMap. У первой были Win32 geometry/memory/material/stack ошибки; вторая
останавливается на `MatrixMap.cpp:9 windows.h` (prepared строка13). Candidate
compile выполнялся только для изменённой MatrixLogic, PASS.
Две ошибки строгих marker checks при подготовке (предварительно заменённый
ZeroMemory и закомментированные MoveMemory) устранены до запуска compiler.

| Gate | Фактический результат |
|---|---|
| Полная MatrixLogic TU | COMPILE PASS, все исходные методы в source |
| Host ASan/UBSan/leak | Общий bootstrap PASS, оригинальный path helper и union проверены; vptr исключён только для MatrixLogic |
| ARM64/QEMU | Тот же общий bootstrap и новый оригинальный метод PASS, exit0; без sanitizer |
| devkitA64 | Два новых объекта + ELF link PASS, strong undefined=0, OptimizeMovePath_Delete retained; runtime NOT RUN |
| Windows | Настоящий User32 UnionRect:3 случая PASS; полный compile/link нового engine кандидата NOT RUN |
| CMatrixMap/Logic construction, CGame::Init, form/loop/battle | NOT EXECUTED |
| Switch/GPU | HARDWARE_PENDING; NRO не создавалась |

Первый host link остановился на UBSan vptr metadata, удерживающих RTTI
неперенесённых CMatrixMapStatic/Robot/Building. Подставные typeinfo не введены.
В CMake для полной MatrixLogic исключён **только vptr**; ASan и остальные
проверки UBSan сохранены. После этого перекомпилирован один Logic object и
перелинкован существующий bootstrap, а не все принятые компоненты. Успешный
host binary — адресная перелинковка; первоначальный CMake link log с ошибкой
сохранён и не объявляется успешной полной сборкой.

При host configure сохранены mtime373 generated files, чьи байты не менялись.
Make всё же пересобрал native-cache из-за изменившегося flags.make; core и
отдельные старые suites не перезапускались. ARM64 и SDK проверены двумя новыми
объектами и отдельными копиями архивов по прежним CMake/Ninja recipes. Ошибка
DEVKITPRO при первом прямом SDK link исправлена повтором **только link**.

Артефакты в `build/matrixgame/e2e519/`:

- `map-lifecycle-baseline`, `map-logic-candidate-audit`: actual full-TU recipes,
  compiler outputs и source hashes.
- `map-logic-integration/{host,arm64}`: compile/link/runtime logs общего driver.
- `map-logic-sdk`: новые объекты, отдельный архив и ELF, symbol table и link log.
- `map-logic-union-win32.json`: реальные результаты User32, destination alias.

Точные hashes и input provenance: `matrixgame-e2e519-map-logic-evidence.json`.
Сборки сделаны до локального коммита, с dirty исходниками; embedded Git SHA
не заявляется. Прежний SDK ELF остался совпадающим с его старым result.json.

## Следующая граница

Полный CGame.cpp уже компилировался до этого шага; его ApplyVideoParams в общем
CPU owner по-прежнему достигает явного device refusal. Добавление полной
MatrixLogic **не означает прохождения Init**. g_MatrixMap не публикуется.

Следующая настоящая зависимость — полная MatrixMap.cpp и создание CMatrixMap:
windows.h, затем её DevConsole/Camera/Minimap/Cursor/Transition/Sound owners,
cleanup и object factories. Logic destructor вызывает реальный Clear и не
подменён пустым освобождением. Потребуется замкнуть эти зависимости до
безопасного конструирования/освобождения CMatrixMapLogic и вызова из CGame.
Material provider также остаётся явной будущей GPU-границей.
Исторические32 Windows link dependencies после этого шага не пересчитывались.

13 доступных protected inputs, frozen основная NRO и matching ELF совпали
с прежними hashes; оба ZIP входят в эти protected inputs. Pinned VO/PNG/data.txt
после исполнения совпали с baseline. `G:\pack.zip` недоступен,14/14 PASS нет.
Upstream pin `218115a72fa9c5d43269f4ca84a8596a0bcb82f1` чистый.
