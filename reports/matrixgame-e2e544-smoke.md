# E2E-5.44 — подготовка реального диагностического GPU smoke

Статус: программные проверки приведены в `matrixgame-e2e544-smoke-evidence.json`.
Отдельная NRO ещё не упакована. SWITCH HARDWARE: NOT RUN / HARDWARE_PENDING.

## Реализованный путь

Диагностический вход использует настоящий deko Device/Queue, загруженные
material shaders, swapchain/color/depth и реальный texture backend. Smoke
создаёт BC1 4×4 с красным блоком, выполняет upload, рисует прямоугольник и
оригинальный debug font, читает framebuffer, проверяет красный центр, пишет
BGRA и SHA-256, затем выполняет Present и освобождает ресурсы. Это отдельный
диагностический кадр; он не является кадром MatrixGame.

Экраны B/C и ошибки используют тот же оригинальный шрифт и GPU backend.
После успешного полного оригинального Init запускается оригинальный form/loop.
Перед первым настоящим Present callback читает текущий acquired image,
сохраняет `original-first.bgra` и возобновляет запись в том же кадре.
Остановка происходит после четырёх завершённых Present оригинального цикла.
Самостоятельные smoke/status Present в эти четыре кадра не входят.

Оба readback сохраняются рядом с `run.jsonl` в уникальном каталоге
`sdmc:/switch/matrixgame-diag/logs/run-…/`. Выход B/+ проверяется на безопасных
границах, общий кооперативный deadline — 120 секунд. Он не прерывает зависший
блокирующий вызов GPU/SDK. После кадров карта остаётся видимой две секунды.

## Исправления владения и ошибок

В texture store command buffer теперь уничтожается перед собственной памятью
команд. Прежний обратный порядок установлен анализом исходников; аппаратное
воспроизведение дефекта отсутствует. Очередь upload/readback синхронизируется
существующим backend перед освобождением временных ресурсов.

Ошибки libnx Result проверяются через `R_FAILED`, а не знак HRESULT.
SDK debug callback пишет отдельное событие `sdk_error`, учитываемое как отказ
диагностики; warning ограничен первыми 32 сообщениями и суммарным счётчиком.
Диагностическая сборка линкует настоящий `libdeko3dd.a`. Callback возвращает
управление штатной fatal-политике SDK и не бросает исключение через C ABI.
Fatal SDK завершение не гарантирует C++ unwind/cleanup; журнал до операции
и SDK context/Result/message сохраняются по возможности. Это отличается от
обычного исключения, при котором выполняется RAII cleanup.

Политика SDK проверена по [оригинальному deko3d v0.5.0](https://github.com/devkitPro/deko3d/blob/v0.5.0/source/dk_device.cpp).

## Границы доказательств

- HOST ASan/UBSan: журнал, ошибки записи/flush/close, конкуренция, Unicode,
  SDK callback event accounting; CPU checker bounds и отрицательные pixel cases.
- ARM64/QEMU: те же доступные CPU/файловые проверки.
- COMPILE/LINK: диагностический ELF devkitA64, реальные shader/font objects,
  debug deko SDK и отсутствие unresolved strong symbols.
- WINDOWS HAL: историческая E543 проверка полного оригинального Init/Draw и
  двух сессий остаётся отдельным доказательством. Здесь её не повторяли:
  Init/Draw/resource контракты не изменены; новое событие журнала проверено адресно.
- SWITCH HARDWARE: ни smoke upload/draw/readback/Present, ни native Init,
  ни четыре кадра, ни выход в hbmenu пока не выполнялись.

Счётчики buffer bytes являются накопленными, не полной оценкой VRAM.
Queue submission counter не включает внутренние transfer submissions texture
store; native texture session upload counter также не является всеми GPU upload.
Показания process memory на Switch получаются через `svcGetInfo` с Result.

Следующий обязательный шаг: упаковать одну отдельную NRO, проверить структуру
и встроенные assets, подготовить manifest/SHA-256, точный список личных ресурсов
и инструкцию первого аппаратного запуска. До этого подготовка E2E-5.27 не завершена.
