# E2E-5.47 — второй аппаратный журнал и nonblocking applet poll

Из `G:/logs1.zip` получен один полный журнал run-1793013902080446-0,
64 последовательных records, journal_close присутствует. Build ID и SHA-256
NRO совпадают с переданным run2 manifest. Архив и переданные NRO не изменены.
Неизменная копия: `build/matrixgame/e2e547-applet-poll/run2.jsonl`.

## Реальная аппаратная граница

`A.window_attachment` завершился ok. Device/Queue, shader pair, command backing,
color/depth allocation, swapchain attach и BC1 upload снова завершились успешно.
Первый отказ теперь находится внутри `A.frame.acquire_clear`:
`appletGetMessage failed: 59905` (`0xEA01`, module 1, description 117).
`window.attachment.detach` вернул 0. Поэтому прежний отсутствующий attachment
guard снят на аппаратуре, но передача оригинальному Init ещё не достигнута.

Цепочка по исходникам:
`RunSmoke → BeginNativeFrame → AcquireNativeFrame → Impl::StartRecording
→ SwitchWindow::WaitForDrawable → Lifecycle::Update → NxAppletBackend::Next
→ appletGetMessage → eventWait(message_event, 0)`.

BC1 draw, debug-font draw, readback, Present и CGame::Init не выполнялись.
Error screen повторил тот же отказ в acquire/clear. Abort/restore и тело
Device teardown завершились с Result=0/ok, process_exit=10, журнал закрыт
примерно через 0.575 секунды. Возврат в hbmenu самим архивом не подтверждается.

## Причина и исправление

По [исходному libnx v4.12.0](https://github.com/switchbrew/libnx/blob/v4.12.0/nx/source/services/applet.c),
appletGetMessage сначала выполняет eventWait с timeout=0 и возвращает его
Result при отсутствии сигнала. В установленном SDK KernelError_TimedOut=117,
то есть 0xEA01. Прежний backend признавал пустым только AM module 128 / desc 3,
а штатный timeout ошибочно превращал в исключение.

Production PollMessage теперь считает оба документированных пустых результата
отсутствием сообщения. При этом output message не обрабатывается, вызов не
объявляется успешным receive. Любые остальные ошибки сохраняются с исходным
Result. При успешном receive вызывается настоящий appletProcessMessage;
focus/resume/exit hooks и ограничения lifecycle pump сохранены. Константы и
result mask сверены с установленными SDK macros через static_assert/тесты.

## Программные проверки и пределы

- HOST ASan/UBSan и ARM64/QEMU: исполняется точное тело appletGetMessage из
  официального закреплённого libnx. EventWait и receive IPC endpoints заменены
  только в тесте детерминированными fixtures; HOS или GPU не эмулируются.
- Baseline возвращает точный 59905 до receive и воспроизводит старое исключение.
- Candidate: timeout, AM empty-race, успешное сообщение, сохранение реальных
  ошибок, неизменная SDK fatal policy при receive failure, focus/resume/exit
  и 1000 idle Lifecycle::Update. Все проверки адресные.
- COMPILE/LINK: production diagnostic ELF devkitA64 и strong-symbol audit.
- E546 handoff CPU-проверки используются по совпадающим хешам исходников;
  больших Windows/host регрессий без изменения общих контрактов не повторяли.
- WINDOWS HAL здесь не запускался; изменение касается только native poll.
- SWITCH HARDWARE исправления: NOT RUN / HARDWARE_PENDING. Прохождение
  исправленного acquire, GPU smoke и оригинального Init не доказано.

Итоговые exits/hashes/status: `matrixgame-e2e547-applet-poll-evidence.json`.
Доказательства второго запуска: `matrixgame-e2e547-run2-hardware-evidence.json`.
Официальный source и извлечённая функция сохранены с SHA-256 в build; тест
проверяет точный hash функции, не подменяет её похожей реализацией.

Третья NRO не создаётся: пользователь временно недоступен для аппаратных тестов.
Оба предыдущих диагностических комплекта, основная NRO, upstream, frozen,
ресурсы и сейвы сохраняются. Commit/push нет. Работы продолжаются офлайн;
следующая аппаратная граница после устранённого empty-poll отказа пока неизвестна.
