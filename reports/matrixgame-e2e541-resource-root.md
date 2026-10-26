# E2E-5.41: explicit resource root — PARTIAL

Нативный standalone entry получает абсолютный resource root из argv[1], по
умолчанию `sdmc:/switch/space-rangers-hd-a-war-apart/game`, как основная игра.
Путь `CGame::Init → CFile::OpenPackFiles` больше не зависит от process cwd.
Baseline без provider воспроизвёл отказ открытия настоящего robots.pkg из
чужого пустого cwd. Файлы игры не копировались и не изменялись.

`resource_paths::NativeScope` публикует immutable root для всех CPU threads.
Relative read paths и package physical paths получают root; logical package
keys, disk-first fallback, case lookup, extension lookup и enumeration сохранены.
Absolute paths и write destinations сохраняются. Нет chdir, mkdir или фиктивного
успеха. Scope сохраняется до полного original/cache/package cleanup; при отказе
Init живёт до окончания rollback. Владельцы файлов и пакетов остаются в CFile.

## Доказательства и незакрытый отказ

- HOST ASan/UBSan: настоящий original CFG и CStorage.Load карты
  `desert1_3e.cmap`, map_id `389853e2`; CPU-only, без PrepareMap/GPU.
  Case/extensions/enumeration, package fallback, 16 worker reads, absolute cache,
  write destinations, invalid/nested roots и scope cleanup PASS.
- ARM64/QEMU: тот же настоящий CFG/map и контракты PASS.
- HOST и ARM64/QEMU файловая регрессия: 300 package reads, 256 concurrent reads,
  payload 1100003 bytes PASS. HOST использует ASan/UBSan.
- devkitA64 COMPILE/LINK: 184 reachable own link inputs, 0 strong undefined;
  255 generated original files совпадают с предыдущим SDK baseline. NRO нет.
- WINDOWS HAL: первый отрицательный Init с отсутствующим pkg корректно очистил
  original/runtime/root owners. Следующий настоящий Init/RobotPreload/stor.Load/
  PrepareMap, 8 form frames, focus release/recreate и Deinit PASS из чужого cwd.
  Во втором сеансе повторный отрицательный Init аварийно завершился: сначала
  access violation, после обновления package TU — heap corruption `0xc0000374`.
  Поэтому вся Windows-регрессия FAIL, **этап не завершён**.

Сбой находится на пути `InitializeOriginalGame → CGame::Init → CFile::OpenPackFiles`
с дальнейшим original rollback. Первый стек включал destructor shared Package.
Это ещё не установленная первопричина. Свежие file_native/package/input TUs
собраны вместе для исключения устаревших объектов; одной пересборки недостаточно.
Повторный failure assertion сохранён. Нельзя объявлять root cleanup доказанным
для этого сценария. PageHeap-попытка без обязательной union env не является
проверкой карты; следующая попытка остановилась раньше на HAL CreateDevice.

SWITCH HARDWARE: NOT RUN / HARDWARE_PENDING. Реальный native Init, PrepareMap,
первый кадр и Present на Switch ещё не исполнялись. Первый аппаратный блокер
не наблюдался. Следующая программная работа — локализовать Windows heap corruption
и повторить этот адресный test; затем продолжить настоящую native цепочку.

Сырые результаты: `build/matrixgame/e2e541-resource-root/`; проверяемый сводный
manifest — `matrixgame-e2e541-resource-root-evidence.json`. Результаты предыдущих
этапов остаются историческими, не превращаются в текущий Windows PASS.

P0, GI repeating-text causality, manual-publish ENOENT, Switch hangs,
E2E-5.34 raw HAL retry и queue-loss recovery остаются OPEN.
Commit/push разрешены последним запросом пользователя; защищённые входы не входят
в коммит. Общая интеграция игры остаётся незавершённой.
