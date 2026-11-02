# E2E-5.46 — window attachment и вторая диагностическая NRO

Исправлен дефект первого аппаратного запуска: `RunSmoke` начинал кадр до
подключения native window lifecycle. Прежняя сборка и аппаратный журнал
сохранены; исходный guard `native applet window not attached` не удалялся.

## Владение и исходная цепочка

`diagnostic::WindowPhase` держит настоящий `Window::AttachNative` lease:

1. Раннее подключение перед Device/smoke/status. Attachment живёт дольше Device.
2. После smoke и экрана B lease освобождается. Result восстановления visibility
   проверяется до оригинального Init.
3. `InitializeNativeGame → CGame::Init → EnterNativePlatform → InitAsNative`
   создаёт обычный оригинальный attachment. В момент входа раннего владельца нет.
4. Runtime владеет attachment во время оригинального Init, form/loop и кадров.
5. При отказе Init/Draw сначала AbortNativeFrame и runtime.Deinit, затем
   отдельный диагностический attachment для error UI, если он нужен.
6. При ранней ошибке smoke тот же ранний владелец остаётся доступен error UI.
   Исключение/отмена и обычное завершение освобождают его через RAII.

Сохраняются строгие проверки повторного Acquire и Lifecycle::Attach.
Никакой вложенный второй hook, refcount, пропуск исходного Init или fake success
не добавлен. Production InitAsNative/Deinit и original generated sources не
менялись. NativeAttachment пишет результат Detach в durable journal, включая
аварийное освобождение. Добавлены before/after scopes для acquire/clear,
BC1 draw и original debug-font upload/draw; алгоритмы отрисовки сохранены.

## Доказательства

| Gate | Фактический результат |
| --- | --- |
| HOST ASan/UBSan | PASS. Реальный Lifecycle и production InitAsNative/Deinit с явной IPC-моделью: точный старый guard воспроизведён; обычный handoff, smoke error, ранний/поздний Init error, start/read failure, visibility restore failure, отмена, повторный вход. Максимум один активный backend hook, сбалансированные Start/Stop. |
| ARM64/QEMU | PASS того же CPU ownership-контракта. Это не libnx IPC и не полный CGame::Init. |
| COMPILE/LINK | PASS диагностического devkitA64 ELF с настоящими deko3dd, smoke, fonts/shaders и original Init/Draw. Strong undefined = 0. |
| Smoke pixel checker / journal | Повторно проверены хеши неизменных адресных E544 HOST/ARM64 результатов; большие прежние тесты не перезапускались. GPU smoke на host не эмулировался. |
| WINDOWS HAL | Полный исторический original Init/Draw reference остаётся отдельным доказательством. Не повторён: общие Init/Draw/resource контракты не менялись. |
| SWITCH HARDWARE первой сборки | Реально пройдены Device/Queue/shaders/allocations/swapchain/BC1 upload, затем lifecycle guard. Нет карты, debug-font draw, framebuffer или Present. |
| SWITCH HARDWARE кандидата | NOT RUN / HARDWARE_PENDING. Исправленный smoke, полный Init, карта, Present и штатный выход ещё требуют второго ручного запуска. |

Тест не заменяет настоящий renderer обработчиками. IPC-модель используется
только в CPU-тесте владения, отдельно от связанного production SDK бинарника.
В первой попытке новой SDK-компиляции поймана ошибка типа владельца Font при
добавлении trace scope; исправлена на исходный shared_ptr, итоговая сборка PASS.

## Готовый комплект для второго запуска

Папка: `dist/matrixgame-e2e527-diag-run2/`.
Файл: `matrixgame-e2e527-diag-run2.nro`, 6 932 677 байт.
Название в hbmenu: **MatrixGame diagnostic run2**.
Build ID: `82034333146d-bef33ef232ece20af853`.
SHA-256:
`2ef94558387af0e719ff577110bbf0825ba06a674356ae9b0ed43230451e5916`.

NRO, NACP/icon, пять оригинальных font assets и обе DKSH shader программы
проверяются в самом бинарнике. Manifest, build-inputs, SHA256SUMS, список
личных ресурсов и инструкция находятся рядом. Второй NRO не заменяет ни
основную игру, ни первую диагностическую сборку.

На SD копируется только новая NRO в `switch/matrixgame-diag/`;
три прежних ресурса остаются в `switch/matrixgame-diag/game/`.
Новый журнал и readbacks появятся в отдельном `logs/run-…/` рядом с первым
каталогом. [Инструкция второго запуска](../docs/matrixgame-e2e527-second-run.md).

Перед упаковкой и после неё проверяются fingerprints 15 защищённых объектов
и 107 аппаратных файлов; первая NRO остаётся с SHA-256
`54fa67f6351eaadc52cfc838bd9b501929116346a92e6759c3ae92972fd68867`.
Upstream, основная NRO, frozen, ресурсы и сейвы не изменялись.
Commit/push и самостоятельных аппаратных запусков нет.

Точный следующий аппаратный отказ кандидата пока неизвестен. Цель второго
запуска — пройти исправленную границу smoke и установить фактический результат
оригинального Init/первого кадра. Queue loss recovery, управление боем, P0/GI,
historical manual-publish ENOENT и старые Switch hangs здесь не закрываются.

Программный аудит: `matrixgame-e2e546-window-handoff-evidence.json`.
Финальный аудит пакета: `matrixgame-e2e546-run2-delivery-evidence.json`.
