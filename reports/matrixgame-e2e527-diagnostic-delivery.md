# E2E-5.27 — передача одной диагностической NRO

Подготовлена отдельная `dist/matrixgame-e2e527-diag/matrixgame-e2e527-diag.nro`,
6 924 485 байт. SHA-256:
`54fa67f6351eaadc52cfc838bd9b501929116346a92e6759c3ae92972fd68867`.
Build ID: `82034333146d-018b15457d85deaf526d`.
Native Switch execution: **NOT RUN / HARDWARE_PENDING**.

Подготовка комплекта не означает успешный native Init: настоящая цепочка на
Switch ещё не запускалась. Первый точный аппаратный отказ и исходный вызывающий
метод пока не наблюдались. Они будут определяться по последнему `begin`,
`last_ok`, Result/exception и SDK context первого ручного запуска.

## Аудит требований исходного ТЗ

| Требование | Доказательство и предел |
| --- | --- |
| Output provider, original CFile/minimap, Unicode, ошибки, повторный вход | E527 output и E528 minimap: адресные HOST/ARM64/Windows filesystem, настоящий cold-render/store/warm-cache original Windows Init. Изолированный run/output передаётся NativeInitRuntime и живёт до Deinit. На SD это ещё не исполнялось. |
| Original StoredState, capture/restore/release/reset/targets | E529 scalar state и E534 reset owners, затем E542/E543 full Windows sessions. Native backend сохраняет оригинальный DeviceState. Независимый raw Windows HAL CreateStateBlock retry с двумя mismatch и queue-loss recovery остаются OPEN. |
| A: libnx, HOS, lifecycle, Device/Queue/memory/shaders/color/depth/swapchain/BC1/submission/Present/release | Реальные production providers и E544 RunSmoke; SDK debug library, NRO font/shader bytes проверены. HOST checker проверяет только проверки пикселей/размеров; GPU runtime не выполнялся. |
| B: полный оригинальный Init, стадии и время | E543 original CGame::Init instrumentation; реальные Windows journals покрывают config/font/cursor/RobotPreload/stor.Load/BuildTexUnions/PrepareMap/minimap/interface/ApplyVideoParams. Диагностический native entry не пропускает обязательные вызовы. |
| C: original Draw после полного Init, дополнительные кадры | Original form/loop, четыре completed Present; отдельные smoke/status Present не считаются. Первый framebuffer читается из текущего acquired image до Present, сохраняются BGRA/SHA/draw/queue/Present counters. Только SDK/static proof до аппаратного запуска. |
| D: аварийное освобождение и hbmenu | RAII owners, AbortNativeFrame, original Deinit, queue wait/release и снятие window attachment. E542/E543 повторный Windows вход проверен, native in-process reentry отложен до подтверждения безопасного первого cleanup. SDK fatal не гарантирует unwind; это явно указано пользователю. |
| Ранний durable журнал | E543 + E544 HOST ASan/UBSan и ARM64/QEMU: настоящая filesystem, flush/fsync, семь fault cases, nested stages, 200 concurrent records, Unicode и unique runs. Журнал открывается до GPU. libnx fsync статически вызывает fsFileFlush; физическая SD durability hardware pending. |
| Environment/Result/exception/memory/alignment/upload/draw/Present/cleanup | Реальные метрики и SDK callback, отдельные Result и null для неизвестного exception Result. Пределы счётчиков зафиксированы в E544; raw PC/address журнал получает только если SDK предоставляет его в сообщении, отдельного crash unwinder нет. |
| Экран, B/+, ограничение ожидания | Реальный original debug-font status/error path, проверенная CPU RunControl policy, 120-second cooperative deadline. Блокирующий SDK/GPU вызов не прерывается таймером. |
| Один пакет, независимые данные, без игровых ресурсов | Ровно одна NRO, NACP/icon встроены, внешний RomFS не нужен. Личная копия трёх ресурсов перечислена с SHA в resources.json, в пакет не включена. Только отдельный diagnostic SD tree. |
| Application mode и максимум два запуска | Инструкция со ссылкой на официальный Atmosphère override; entry проверяет Application/SystemApplication до GPU. Первый запуск назначен диагностическим; второй только после анализа первого, заранее второй бинарник не создавался. |
| Compile/link, strong symbols, ASan/UBSan, ARM64 | Текущие E544 profiles PASS: CPU pixels/journal, devkitA64 ELF, 186 reachable link inputs, strong undefined = 0. Полная старая Windows pixel/text регрессия не повторялась без изменения контрактов. |
| Сохранность, no commit/push, отсутствие самостоятельного hardware | После упаковки повторно проверены защищённые fingerprints. Основная NRO/upstream/frozen/robots.pkg и аппаратные входы сохранены. Аппаратных запусков и новых commit/push нет. |

## Состав передачи

В папке `dist/matrixgame-e2e527-diag` находятся NRO, control.nacp,
manifest.json, build-inputs.json, resources.json, README.md и SHA256SUMS.txt.
Для hbmenu на SD нужна только NRO плюс три личных игровых файла по инструкции.

[Инструкция первого запуска](../docs/matrixgame-e2e527-first-run.md) описывает
ожидаемые экраны, возможные ошибки и извлечение всего уникального каталога
`sd:/switch/matrixgame-diag/logs/run-…/` после завершения приложения.
Он может содержать run.jsonl, smoke.bgra, original-first.bgra и output/.

Точные configure/build/package команды сохранены в manifest; команда упаковки
воспроизводит бинарник из текущего подготовленного workspace и отказывается
перезаписывать существующую NRO. Для чистого master потребуются незафиксированные
исходники и оригинальные font bakes, что явно указано в README.

Сводный machine-readable аудит: `matrixgame-e2e527-diagnostic-delivery-evidence.json`.
Исторические E541/E543 и E544 preparation-only записи не переписываются как будто
NRO существовала тогда. Новый отчёт фиксирует фактическую передачу сейчас.

Для первой карты остаётся ручной native hardware проход A → полный CGame::Init
→ original form Draw → submit/Present → cleanup. Пакет предназначен именно для
установления этой границы за один запуск. Управление боем, интеграция с основной
игрой, P0/GI repeating-text causality, manual-publish ENOENT, старые Switch hangs
и queue-loss recovery остаются открытыми независимыми задачами.
