# E2E-5.21: native applet/window lifecycle

Уточнение следующей итерации: ExchangeCursor реализован. Прежний порядок
блокеров ниже был указан неверно: `m_DI` объявлен раньше `m_Cursor`, поэтому
раньше требуется debug font. См. [актуальную границу](matrixgame-e2e521-cursor.md).

25.10.2026. **PARTIAL / HARDWARE_PENDING.** `SwitchWindow::Show/Update`
реализованы и включены в отдельную полную SDK ELF оригинального Init/frame.
Новая NRO не создана. На Switch этот код не исполнялся; первая карта не загружена.

## Реализация

- Show вызывает настоящий `appletSetAppletWindowVisibility` при изменении
  visibility. Normal/Restored/Maximized означают показ fullscreen applet layer,
  Minimized — скрытие. Это не эмуляция Win32 styles/resize.
- Getter visibility отсутствует: владелец явно передаёт исходную visibility.
  Отдельный native-entry probe сначала устанавливает её настоящим вызовом IPC.
  Встроенный хозяин обязан передать своё известное состояние. API требует HOS 7+;
  ошибка реального IPC не превращается в PASS.
- Attach проверяет живые applet services, регистрирует только собственный hook
  и исключает одновременных владельцев applet visibility. NWindow и applet
  services заимствованы; их закрытие/повторная инициализация не выполняются.
- Update обрабатывает applet messages через `appletGetMessage` и
  `appletProcessMessage`, сохраняет порядок focus/exit notifications. Pump
  ограничен 256 событиями за вызов, overflow и ошибки IPC явные. C hook не бросает
  исключения и не выделяет память. Все операции выполняются на pump thread.
- Focus использует исходные `GFLAG_APPACTIVE`, `GFLAG_KEEPALIVE` и
  `CForm::SystemEvent(SYSEV_ACTIVATED/SYSEV_DEACTIVATING)` в исходном порядке.
  Exit ставит `GFLAG_APPCLOSE`, остаётся sticky и разворачивает RAII через
  `ExitRequested`. Win32 WndProc не вызывается с синтетическими сообщениями.
- Перед новым acquire `BeginNativeFrame` ждёт foreground через настоящий
  applet message event, обрабатывает resume/exit; скрытое окно нужно явно показать.
  Suspend/focus policy и RequestToDisplay policy хозяина не изменяются. Размеры
  читаются у NWindow; изменение extent требует настоящего swapchain recreation
  и вызывает явную ошибку до acquire. Recreate ещё не реализован.
- Detach снимает собственный hook и восстанавливает visibility владельца.
  Ошибка noexcept restore записывается, печатается и доступна через
  `LastVisibilityRestoreResult`; повторное присоединение после неё запрещено.

Оригинальные алгоритмы Init, Draw и source generators этого этапа не менялись.
Минимальный bootstrap и полная original Init/frame цель сохраняются раздельными.
Основная игра не получает нового native frame loop от этой реализации окна.

Контракт сверялся с установленными libnx headers и исходниками
[applet.c](https://github.com/switchbrew/libnx/blob/master/nx/source/services/applet.c),
[native_window.c](https://github.com/switchbrew/libnx/blob/master/nx/source/display/native_window.c).
Хэши SDK headers и inputs линковки сохранены в evidence.

## Что действительно исполнилось

| Профиль | Результат и граница |
| --- | --- |
| Linux host | ASan/UBSan/leaks PASS: production Lifecycle/engine events и оригинальный CForm; IPC-model fixture |
| ARM64/QEMU | Те же настоящие CPU методы и CForm PASS; IPC-model fixture |
| Windows x86 MSVC | Те же CPU проверки PASS; IPC-model fixture, не D3D9 окно |
| devkitA64 | Новые lifecycle/events/Window/probe objects compile PASS; полный original Init/frame ELF link PASS, strong undefined=0 |
| Switch | libnx IPC, attach/visibility/focus/resume/exit, deko allocation и original Init/PrepareMap/frame — NOT RUN |

Проверены: первичная активация, повторные сообщения, unfocused/background/resume,
KEEPALIVE, null form, исходные флаги и SystemEvent, показ/скрытие, оба visibility
baseline, повторный attach/detach, ошибки start/read/show/pump/observer/restore,
ограничение pump, ожидание resume и exit во время ожидания. После exit показ
и кадр запрещены. Ошибка Show не публикует новое состояние; ошибка attach
убирает hook; ошибка restore остаётся наблюдаемой.

Эти тесты не подменяют системный backend в игровой цели и не заявляют успешный
native Init. Новая CMake test target добавлена; полный CMake rebuild не выполнялся.
Большие material/residency/state suites и Windows Init/frame не повторялись:
изменены только Switch provider и новый lifecycle/events код.

## Где остановилась настоящая цепочка

В этой итерации исполнились CPU lifecycle и SDK compile/link; новая полная
цепочка CGame на Switch **не запускалась**. Show/Update перестали быть исходными
throw-заглушками. Следующая установленная по исходникам граница:

`CGame::Init → создание CMatrixMapLogic → CMatrixMap::m_Cursor →
CMatrixCursor::CMatrixCursor → ReadyWindow().ExchangeCursor({}) →
Window::ExchangeCursor → Unsupported("backend system cursor unsupported")`.

Конструктор cursor срабатывает до `RobotPreload`, `stor.Load` и `PrepareMap`.
Соседние конструкторы Transition/Minimap/DevConsole не вызывают backend.
Это исходная граница при успешных предыдущих операциях и подходящей версии HOS,
а не наблюдённая на консоли остановка. Успешная линковка её не закрывает.

Для первой карты нужны настоящий native cursor/input provider с сохранением
исходного поведения, затем используемые картой stored-state/offscreen/shadow/
font/video/presentation пути. После этого требуется аппаратный запуск
original Init/RobotPreload/storage/PrepareMap с настоящими ресурсами,
первый кадр и teardown. Полный native Takt/Draw loop и интеграция с хозяином
также остаются открытыми.

## Сохранность и открытые P0

Аудит подтвердил прежние хэши 15 доступных защищённых входов, дополнительных
frozen ELF/NRO, основной NRO, root cumulative NRO, 107 аппаратных файлов,
robots.pkg, CFG и frozen cache source. `G:\pack.zip` по-прежнему недоступен;
его сохранность этой средой не проверяется. Ресурсы и сейвы не изменялись.
Коммита и push нет.

P0 основной игры, `manual-publish ENOENT` и зависания Switch остаются OPEN.
Общий GI-cache UAF не объявляется причиной повторяющегося текста: применимость
к `DATA/FormRuinsBG/2RCbg.gi` и frozen source проверялась ранее, но occurrence
нужного освобождения на дефектном экране и причинное воспроизведение отсутствуют.

Команды: `tools/matrixgame_lifecycle_checks.py host|arm64|windows|sdk`, затем
`tools/matrixgame_lifecycle_audit.py`. Результаты и хэши:
[evidence](matrixgame-e2e521-lifecycle-evidence.json),
логи — `build/matrixgame/e2e521/lifecycle/`.
