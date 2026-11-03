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

## Проверенный пакет r7

Source commit: `9c097a23021adc11692bf634d9b0f793245b9281`; build ID `9c097a23021a-4294303d9d29695f9fa3`.
ZIP: `dist\matrixgame-e610-switch-test-r7-init.zip` (10068814 байт).
SHA256 ZIP: `b0467bc456bd0fa0e07ebdf5bb7068856157bd62b0f8aa10a07eca9dceb973bf`.

| NRO | SHA256 | ELF build ID |
| --- | --- | --- |
| matrixgame-e610-quick-smoke.nro | `b158868eb70023bd0b368ec91f18588c5f066a261f610aec833a4ce5deb73f20` | `205f69c3dab5f3118a0f3b925d5d3b0d887f7bc3` |
| matrixgame-e610-interactive.nro | `5651d4cbcba36d40cf952b48c6926da55096441ebe2b4c45a9c100993dd5cf4e` | `3959ae031a3ef51298121f5c7f38e2829d6a5b58` |

Итог: **SOFTWARE PASS / R7 HARDWARE PENDING**. Обе devkitA64 compile/link
и NRO structure checks — PASS, strong undefined отсутствуют. В каждом map
23 object members deko3dd, 0 deko3d; debug error symbols подтверждены.
Сверены 691 SHA исходников, все generated original файлы совпали с r6/r5/r4.
185 защищённых локальных файлов/старых пакетов не изменены; frozen, игровые
ресурсы, основная NRO и upstream проверены существующим SDK gate.
[Полное evidence](matrixgame-e610-init-r7-evidence.json).

Копирование: две NRO из ZIP в `/switch/matrixgame-e610-r7-test/`.
В hbmenu: MatrixGame r7 Quick Smoke, затем MatrixGame r7 Interactive.
DATA и три MatrixGame файла повторно не копировать, если они уже установлены.
Interactive: до 120 секунд после Init, выход Minus+Plus. Передать два новых
каталога `/switch/matrixgame-diag/logs/<run-id>/` и фото/описание экрана.
До пользовательских запусков r7 первый игровой кадр/loop и физические HID/FPS
остаются NOT RUN; Campaign/Gov и победа не охватываются. Разработка остановлена
до новых аппаратных результатов.
