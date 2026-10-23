# E2E-5.19 — вход в настоящий конструктор CMatrixMap

23.10.2026, продолжение `2de89de`, локальный master, без push.

Полная оригинальная MatrixDebugInfo.cpp включена в общий native-cache.
Алгоритмы списка текста, T/Takt/Draw сохранены. Windows API границы вынесены
в debug_font_backend: точный исходный запрос D3DXCreateFontW (25, normal,
один mip, MS Sans Serif), Release, lost/reset, DrawTextW и DrawPrimitiveUP.
Два Windows FLOAT cast заменены эквивалентным float. На остальных backend
создание/рисование шрифта явно Unsupported; фиктивного успешного font нет.

Из полного оригинального MatrixTransition.cpp выделена **точная исходная
CTransition::Clear** в MatrixTransitionLifecycle.cpp. Это зависимость unwind
конструктора, а не перенос GDI capture, BuildTexture, DXT1 upload или render.
Полный Transition TU не изменён и не объявляется COMPILE/RUNTIME PASS.
Сгенерированный lifecycle исключён из общего ENGINE_UNITS, как другие
выбранные lifecycle units. Pinned upstream не изменялся.

В существующем bootstrap после загрузки настоящего robots/data.txt дважды
в каждой из двух сессий выполняется HNew(g_MatrixHeap) CMatrixMap().
**Конструктор реально ENTERED**, но не завершается: CMatrixDebugInfo требует
ReadyDevice, отсутствующий в CPU-сессии. Проверяется конкретное исключение
с префиксом `original debug font Create:`, повторный вход и сохранность
настоящих heap/cache/data owners; g_MatrixMap не публикуется.
Исходный защищённый деструктор CMatrixMap не открывается и искусственный
наследник ради удаления не создаётся. Неожиданный успешный возврат на CPU
аварийно завершает отрицательный тест. При ожидаемом исключении работают
исходный member unwind и matching placement delete.

## Результаты и ограничения

- Host ASan/UBSan/leak-check: общий bootstrap exit0, четыре входа в ctor
  до реального отсутствующего font backend. Прежние исключения сохраняются:
  Console TU без ASan global redzones, Logic/Map без UBSan vptr.
- ARM64/QEMU: тот же общий bootstrap exit0, четыре ожидаемых отказа и повтора.
- devkitA64: отдельный ELF COMPILE/LINK PASS, strong undefined=0;
  удержаны настоящие CMatrixMap/CMatrixDebugInfo ctor и CTransition symbols.
  ELF SHA-256 `e592036e2fcacd0ad0e65dc5857bb66cdd9b95757375199785371f59f7c1d6e2`.
  Исполнение на Switch — HARDWARE PENDING. Новая NRO не создавалась.
- MSVC x86: полные DebugInfo, выбранный Transition lifecycle и настоящая
  Windows-ветка backend COMPILE PASS. Font Create/Release/Draw runtime NOT RUN.
- Свежий CMake configure/generate PASS; проверены реальные recipes и единственный
  selected Transition lifecycle. Полной повторной сборки CMake не было.
- Два source view сверены по содержимому всех 241 файлов. Относительно
  предыдущих 240 изменился только DebugInfo и добавился selected lifecycle.

Windows зависимость задаётся `MATRIXGAME_D3DX9_ROOT`. Использован официальный
[Microsoft.DXSDK.D3DX 9.29.952.8](https://www.nuget.org/packages/Microsoft.DXSDK.D3DX),
локально в отдельной build/deps папке; системные DLL не заменялись.
Package SHA-256 `ead0906ae8a26c18a7525da7490127a2110f7c58f18293738283e30e97c6ea4b`.
SDK/runtime D3DX не является реализацией Switch backend.

Три прежних отсутствующих link symbols закрыты. **Новая точная граница —
создание настоящего debug font при конструировании CMatrixMap.** Базовая карта
ещё не создана полностью; CMatrixMapLogic owner, CGame::Init, игровой цикл и
бой NOT EXECUTED. Не доказаны освобождение живого шрифта, reset/reentry устройства,
отрисовка текста и изоляция stream0 после DrawPrimitiveUP. Следующий шаг —
реальная Windows font lifecycle проверка с настоящим устройством и согласование
GPU state ownership; затем продвигать исходный игровой owner без fake map.

Commands, логи, hashes, manifest diff и точные gates:
[evidence](matrixgame-e2e519-debug-font-evidence.json). Сборки выполнены до
коммита, build_dirty=true. Повторно совпали 13 доступных protected baseline
файлов, 107 аппаратных входов, frozen main NRO/ELF и оба ZIP. G:/pack.zip
недоступен; его текущий hash не объявляется проверенным. Сейвы и ресурсы
не изменялись. Причины аппаратного повторного текста, двух исторических
ENOENT и полного freeze карты/консоли остаются OPEN.
