# Первый аппаратный запуск diagnostic E2E-5.27

Архив пользователя: `G:/logs.zip`, SHA-256
`4864aaeacc3432b5895360610496cee51e2bd7ccbc8dfc8f7c01a739b32cc2fa`.
Один полный журнал: 57 последовательных JSON records, без повреждённого хвоста.
Build ID и вычисленный на консоли SHA-256 NRO совпадают с переданным manifest.
Неизменная копия журнала: `build/matrixgame/e2e545-hardware-run1/run.jsonl`.

## Что исполнилось

Application mode (`applet_type=0`), libnx HID/visibility calls Result=0,
создание настоящих deko Device/Queue, инициализация shader pair и command backing,
allocation color/depth и swapchain attach завершились успешно.
Зафиксированы 1280×720, color/depth по 3932160 байт,
alignment color=512/depth=65536. Последний успешный scoped этап — BC1 upload,
8 байт. Это подтверждает возврат соответствующих SDK/backend операций;
правильность изображения не подтверждена: draw/readback/Present не достигнуты.

## Первый точный отказ

`std::logic_error("native applet window not attached")` выброшен в
`applet_window::Lifecycle::Update` (`applet_window_lifecycle.cpp:23`).
Цепочка по исходникам:

`diagnostic::RunSmoke → DekoDevice::BeginNativeFrame → AcquireNativeFrame
→ Impl::StartRecording → SwitchWindow::WaitForDrawable
→ Lifecycle::WaitForDrawable → Lifecycle::Update`.

Диагностический main вызывает RunSmoke до InitializeNativeGame. При этом
AttachNative находится в native ветке platform_3g.cpp и исполняется лишь
при последующем входе в оригинальный Init. Конструктор SwitchWindow сам
не создаёт attachment. Ранний smoke требует lifecycle, которого ещё нет.
Это дефект порядка владения диагностического входа, а не отказ оригинального
движка или доказательство отсутствующих игровых файлов.

Попытка показать error screen проходит через тот же BeginNativeFrame и тоже
заканчивается отказом. Поэтому экран не даёт дополнительного доказательства.

## Cleanup и незапущенные этапы

Smoke/status abort и restore вернули 0; D.DekoDevice.impl_body завершился ok.
Записаны process_exit=10 и journal_close, всё менее чем за 0.62 секунды
от начала отсчёта журнала. Это обработанный отказ с закрытым журналом;
возврат в hbmenu сам по себе архивом независимо не подтверждён.

Оригинальный CGame::Init, PrepareMap, карта, debug-font draw, GPU readback,
Present и управление боем не выполнялись. В архиве нет framebuffer файлов.

Следующее исправление должно дать ранним diagnostic smoke/status собственный
реальный window attachment и корректно передать/освободить его перед оригинальным
Init, который также создаёт attachment. Просто добавить второй Attach или
удалить проверку Lifecycle::Update нельзя: первый приводит к double-attach,
второе скрывает отсутствующий provider. Нужны адресные проверки порядка,
unwind/re-entry и SDK compile/link перед подготовкой второго бинарника.

Новой NRO в рамках этого анализа не создано, commit/push нет.
Исторические файлы передачи не переписываются; аппаратный статус уточняет
новый `matrixgame-e2e545-hardware-run1-evidence.json`.
