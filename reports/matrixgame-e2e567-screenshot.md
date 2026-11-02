# E2E-5.67 — программная интеграция native F9 screenshot

Статус: **host ASan/UBSan, ARM64/QEMU, Win32 PNG/reference и SDK/NRO PASS**.
Физический GPU capture, нажатие F9 в native игре и Switch Init/map — **NOT RUN**.
Полный порт остаётся незавершённым.

## Реализация

Убрана безусловная заглушка-отказ native `make_screenshot`. Теперь полный
generated `CFormMatrixGame` вызывает `native_form::Screenshot`, после успешной
записи показывает оригинальное сообщение с именем файла. Обязательная операция
захвата не заменена фиктивным изображением: production `DekoDevice` читает
настоящий scene image через существующий SDK image-to-buffer readback.

Снимок берётся с последнего завершённого успешным Present scene image до gamma
pass. Это соответствует render-target варианту оригинального fullscreen F9;
Windows GDI/desktop branch на Switch не переносится. Software cursor,
нарисованный перед Present, входит в этот scene. Цвета post-gamma presentation
снимком не заявляются. Чтение синхронное, без отложенного callback.

`ScreenshotFrame` публикует фактический slot после Present/queue completion.
Recording нового кадра, Detach/Reset и Abort инвалидируют его. Пока завершённого
кадра нет, идёт frame/recording или queue неисправна, capture явно отказывается.
Нет fallback на slot 0 и нет чтения freed/перерисовываемого image. Проверка этих
точек подключения — source audit плюс SDK compile/link; исполнение самих GPU
ветвей без устройства не доказано.

`SaveScreenshot` проверяет формат/extent/pitch/размер данных, преобразует
RGBA8/RGBX8 или RGB565 в оригинальный RGB CBitmap и вызывает настоящий
`CBitmap::SaveInPNG` → `CFile::Create/Write/Close`. Native capture наследует
ограничения существующего ReadImage: размер до 8192 по стороне и 64 MiB данных.
Сам сериализатор допускает валидированный буфер до 256 MiB, без заявления
большего GPU capture. Alpha не записывается, как в оригинальном RGB screenshot.

Требуется уже привязанный private output scope. Используется оригинальный
`PathToOutputFiles("Screenshots")`, получающий
`<private-root>/SpaceRangersHD/Screenshots`. Имя `Shot-<clock>-<sequence>.png`
не удаляет старый файл; существующее имя пропускается. Symlink/traversal guards
существующего output provider сохраняются. Ошибки захвата/пути/записи не
возвращают успешное имя и не показывают сообщение об успешном сохранении.
Операция исполняется в том же engine thread, что и остальные Deko calls.

## Проверки

| Проверка | Результат | Предел |
| --- | --- | --- |
| Host ASan/UBSan | 36 PNG round trips, 3024 точных RGB channel, 8 отказов, 4 slot sequences PASS | Synthetic capture provider, без GPU/Init |
| ARM64/QEMU | Те же случаи PASS | CPU execution, не Switch |
| Win32 MSVC | Те же случаи PASS с настоящими native-core PNG/CFile dependencies | Компонентный reference, не D3D HAL screenshot |
| Cross ABI | pixels.bin побайтно совпадает на всех трёх | RGB observations этих fixtures |
| Независимый PNG oracle | CRC chunks, zlib, PNG filters, независимая RGB/RGB565 формула PASS для 36 PNG каждого профиля | Проверяет сохранённые файлы, не источник GPU pixels |
| Readback cancellation regression | Host ASan/UBSan и ARM64/QEMU PASS | Actual abort prefix, callback ownership CPU only |
| devkitA64/libnx/deko3d | Compile/link/NRO PASS, strong undefined = 0 | GPU и аппаратный F9 NOT RUN |

Fixtures покрывают неплотный pitch, несимметричные цвета/строки, RGB565
расширение битов, игнорируемую alpha, неизменность входного буфера, отсутствие
output binding, некорректные layout и injected capture failure. Ошибка I/O
проверена детерминированно: обязательный parent directory занят существующим
обычным файлом; он сохранён. Это не инъекция короткой записи внутри CFile.
Старый sentinel screenshot сохраняется; корректные вызовы создают отдельные PNG.

Первоначальные host/ARM результаты оставлены как `result-first.json`, их fixtures
не удалены. После добавления Windows branch небольшая адресная проверка
повторена окончательным runner; прежние большие map/HAL suites не повторялись.
Первый SDK configure отказал в чтении submodule git metadata в sandbox;
лог `configure-first-sandbox-failure.log` сохранён. Разрешённая SDK сборка прошла.
Предварительная успешная NRO сохранена отдельно от окончательной fingerprint
сборки. Уточнения source-аудита не меняли production алгоритм.

## Артефакт и воспроизведение

Отдельная стандартная target `matrixgame-native-diagnostic-nro`:

`build/matrixgame/e2e567-screenshot/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro`

Размер: 6973637 байт. SHA256:
`10fb5e0507d8aa6b5c1ce2d4b8f50b47b5f87e1ec3816d7d29f5854efb7d882f`.

Точные размер/SHA256 NRO и ELF, команды, исходные hashes, SDK link inputs,
cross ABI observations и независимый PNG oracle записаны в
[evidence](matrixgame-e2e567-screenshot-evidence.json).

Из корня checkout, с установленными SDK/зависимостями:

```powershell
python tools/matrixgame_screenshot_checks.py host
python tools/matrixgame_screenshot_checks.py arm64
python tools/matrixgame_screenshot_checks.py windows
python tools/matrixgame_screenshot_readback_checks.py host
python tools/matrixgame_screenshot_readback_checks.py arm64
python tools/matrixgame_screenshot_sdk.py
python tools/matrixgame_screenshot_audit.py --output build/matrixgame/e2e567-screenshot/audit-repeat.json
```

Для повторного аудита выбирается новое имя выходного файла. Wrapper SDK
конфигурирует отдельный build directory; основная NRO не меняется.

## Состояние цепочки Init и оставшиеся компоненты

Единственное изменение generated original files относительно E566 —
`OriginalNativeForm.cpp` с маршрутом F9. Остальные generated hashes сохранены.
Upstream clean на `218115a72fa9c5d43269f4ca84a8596a0bcb82f1`. Protected inputs,
frozen, ресурсы/сохранения, пользовательский logs2.7z и предыдущие NRO сохранены.
Commit/push нет.

Последнее фактическое аппаратное свидетельство Init остаётся
ReadParams → ApplyGammaRamp до map constructor. Программные компоненты gamma,
ReadParams, settings/sampler, StoredStateBackend и minimap output продолжают
существовать; этот этап не является новым полным native Init run.

Открыты последующее native Init/PrepareMap, реальные Rangers resource/audio
services и hardware image/VRAM/HID/focus/presentation/recovery gates. Новый F9
требует пользовательской проверки после завершения хотя бы одного кадра:
правильный кадр/цвета/ориентация, путь PNG, повторные снимки, поведение после
resize/reset/ошибки кадра. Агент не переносил файлы и не взаимодействовал с Switch.
