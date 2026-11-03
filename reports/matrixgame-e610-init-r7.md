# MatrixGame E610 r7: Init GPU handoff

Основание: [два реальных запуска r6](matrixgame-e610-r6-hardware-20261103.md).
Настоящий Init завершился, затем diagnostic CaptureState отказал при active frame.
Удаление status screen недостаточно: original loop AcquireNativeFrame также
требует idle. Защитный guard CaptureState корректен и сохранён.

## Изменение

Добавлен DekoDevice::CompleteNativeInitializationWork. Diagnostic entry вызывает
его сразу после успешного Init, до следующего status snapshot/loop.
FlushRecording исполняет pending commands, ждёт queue completion и лишь затем
освобождает packet owners. Если Init приобрёл display image, nwindowCancelBuffer
возвращает его без Present; state/frame переводятся в idle только после успеха.
Открытая сцена, lost queue, pending readback отвергаются. Submit/wait failure
сохраняет packet owners; cancel failure после завершения GPU сохраняет image и
active frame, дальнейшая CaptureState не допускается. Текстуры/состояние Init
сохраняются, обязательные draw calls не отбрасываются. Main NRO не пересобирается.

## Адресная проверка

CPU harness исполняет извлечённые без замены тел FlushRecording и нового метода,
реальные NativeFrameScope/NativeStateStore/NativeDrawBatch. Window IPC/queue —
явные CPU endpoints; это не эмуляция или доказательство GPU/HOS.
Проверены: исходный guard failure; offscreen и acquired-image completion;
submit/wait → release → cancel; состояние до/после snapshot restore; next scene;
idle; open scene/lost queue/pending readback; submit и cancel failures; отсутствие
Present и начисления игрового кадра. Windows MSVC, host ASan/UBSan/LSan и
ARM64/QEMU UBSan (с vptr) — PASS: build/matrixgame/e610-init-r7-checks/*/2/.
ARM64 сохраняет локальную visibility std::type_info::operator== для известной
g++15/libubsan interposition проблемы; sanitizer checks не отключаются.
Первый compile тестового CPU endpoint имел неверный тип NativeWindow; он
исправлен до повторных PASS, результаты первой попытки сохранены отдельно.
Большие Windows бои/регрессии не перезапускались.

## Граница

r6 доказал Init и GPU smoke/orientation readback. Четыре игровых кадра/loop,
физические HID, FPS и робот ещё не проверены; Campaign/Gov entry и естественная
победа не входят в standalone NRO. Новый r7 HARDWARE PENDING.
[Инструкция](../docs/matrixgame-e610-init-r7-readme.ru.md).
