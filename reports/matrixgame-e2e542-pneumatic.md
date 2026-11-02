# E2E-5.42: original pneumatic static owner

Незакрытый Windows-сбой E2E-5.41 локализован в настоящем
`CGame::Deinit → CMatrixRobot::DestroyPneumaticData`. Оригинальный метод делал
`HFree(m_Pneumaic, g_MatrixHeap)`, но оставлял статический указатель и `g_fcnt`.
После успешного Init/PrepareMap/Draw/Deinit следующий Init мог упасть до
RobotPreload: его rollback повторно освобождал буфер предыдущей сессии.

В baseline PE под CDB ошибочный free связан с RVA `0xb2420` и символом
`CMatrixRobot::DestroyPneumaticData+0x10`. Это опровергает предварительную
локализацию только по destructor Package из первого стека E2E-5.41.
Оба generated заголовка CException побайтно совпадают после LF normalization;
гипотеза несовместимого exception ABI не подтвердилась.

Production generator `matrixgame_actor_overrides.py` сохраняет исходный release
и после него обнуляет `m_Pneumaic` и `g_fcnt`. Порядок Deinit не меняется:
карта и её robot owners уничтожаются до pneumatic buffer. Upstream не изменён.
Повторный ранний отказ не пропускается и не объявляется успехом.

## Проверки

- BASELINE: исходное тело метода, реальное объявление CMatrixRobot и настоящий
  HAlloc/HFree под ASan воспроизводят double-free на втором release.
- HOST ASan/UBSan: исправленное production тело, реальные class/allocator;
  null, нормальный release, повторный release, новая allocation/release PASS.
  Это адресный CPU test метода, без симуляции Init, движения робота или GPU.
- ARM64/QEMU: тот же адресный тест PASS.
- WINDOWS HAL: два полных сеанса из foreign cwd. В каждом настоящий ранний
  missing-package Init отклоняется с чистым rollback, затем проходят
  CGame::Init/RobotPreload/stor.Load/PrepareMap, 8 form frames, focus resource
  recreation и Deinit. Два framebuffer readback 800×600; 18838 и 14852 actual
  map draw calls, все проверки native state validator без отказов. Целиком PASS.
- devkitA64 COMPILE/LINK: полный native entry, 184 reachable own inputs,
  0 strong undefined. Среди 255 original generated файлов изменён только
  `MatrixGame/src/MatrixObjectRobot.cpp`; остальные 254 совпадают с E2E-5.35.
- SWITCH HARDWARE: NOT RUN / HARDWARE_PENDING. Ни native Init, ни первый native
  Draw/Present на Switch в этом этапе не исполнялись.

Новая Windows PE названа `matrixgame-map-state.exe`; исходная диагностическая
настройка PageHeap для `actual-draw.exe` к ней не применяется. Системная настройка
старого тестового имени пока не снята: GFlags требует UAC. Никакие игровые
assertions не отключены. NRO не создавалась и не менялась.

E2E-5.41 resource-root Windows gate теперь закрыт этим повторным сценарием.
Его первоначальный FAIL-отчёт остаётся историческим доказательством baseline.
Предыдущие большие текстовые/pixel suites не повторялись.

## Следующая граница аппаратной диагностической сборки

Текущий `tests/probe_matrixgame_native_entry.cpp::main` — compile/link entry,
но ещё не готовая диагностическая программа задания E2E-5.27. До создания
SwitchWindow/DekoDevice отсутствует файловый run journal. В
`DekoDevice::Impl::Impl` нет долговечных before/after записей device, queue,
shader/image allocation и их alignment; действующий main запускает неограниченный
original loop и не перехватывает Base::CException. Это конкретные программные
препятствия безопасной передаче одной NRO для ограниченных аппаратных запусков.

Далее нужны ранний отдельный журнал, именованные original Init milestones,
platform/GPU smoke, ограниченный настоящий Draw/Present и финальный cleanup
report; затем manifest/assets/команда NRO и инструкция первого запуска.
Ни линковка, ни эта Windows-регрессия не заменяют данные первого Switch запуска.
Первый фактический native runtime blocker пока UNKNOWN.

Основная NRO, frozen inputs, оригинальные пакеты и сейвы сохранены. P0/GI,
manual-publish ENOENT, Switch hangs, raw HAL state-block retry и queue loss
остаются независимыми открытыми вопросами. Commit/push в этом продолжении нет.
