# Аппаратный r6 — 03.11.2026

Архив пользователя `C:/logs2.zip`, SHA256 `ed7f80723fc667a55c8e1ca5c8ca870bce320fca31c449a56a44cacdd0840620`.
Оба self NRO SHA совпали с r6 package, build ID `7036820f4c81-2be9a052b1f566893f12`.
В каждом журнале 5010 непрерывных записей, штатный journal_close, process exit 10.

Оба запуска завершили настоящий `CGame::Init`, включая миникарту и финальные
ресурсы. В этих запусках отсутствуют SDK fatal/error и warnings. Проверка
асимметричного presentation readback для ориентации прошла на реальном GPU;
это не заменяет пользовательскую оценку экрана. До сбоя: 8406 recorded draws,
689 batch drains, max 64 live packets, 6500352 max live upload bytes, healthy queue.
Четыре completed_presents относятся к smoke/status/loading, не игровым кадрам.
`original-first.bgra` отсутствует. Карта в original loop, HID, FPS, робот/бой,
победа и Campaign/Gov этими запусками не приняты.

Первый отказ обоих запусков: `diagnostic.status_screen` сразу после Init;
исключение `native state capture requires a quiescent frame`.
Оригинальный Init допускает служебные сцены без финального Present. Последующая
диагностическая CaptureState требует idle и отвергает оставшийся active frame.
Также вход loop AcquireNativeFrame требует idle; просто убрать status screen
недостаточно. NativeStateStore guard корректен и не должен отключаться.

Граница исправления r7: после успешного Init выполнить pending commands через
существующий FlushRecording (submit/wait, затем release); вернуть приобретённый
window buffer без Present; лишь после успеха перевести state/frame в idle.
Не имитировать игровой кадр, не отбрасывать команды, не изменять saves/resources.
[Evidence](matrixgame-e610-r6-hardware-20261103-evidence.json).
