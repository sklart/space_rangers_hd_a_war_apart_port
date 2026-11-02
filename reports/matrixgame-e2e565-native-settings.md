# E2E-5.65 — native settings и анизотропная minification

Статус: **программные проверки и отдельная SDK/NRO PASS; Switch NOT RUN**.
Полный порт и полный native CGame::Init остаются незавершёнными.

## Изменения

`NativeGameOptions::settings` принимает необязательный указатель на настоящий
`SRobotsSettings`. Entry копирует структуру до привязки output provider и вызова
исходного Init; память вызывающего кода не изменяется. Null сохраняет оригинальные
defaults. Нулевые обе координаты разрешения заполняются размером owned drawable;
явное разрешение должно ему соответствовать. Для другого разрешения presentation
нужно заранее изменить средствами её владельца. Чужие D3D identities, частично
заданное разрешение и нечисловые brightness/contrast/distance отклоняются до Init.
Оригинальные BPP/FSAA/AF/quality requests сохраняются: caps negotiation,
ApplySettings и ограничение AF остаются работой оригинального движка.

Обнаружена зависимость AF: оригинальный sampler manager повышает LINEAR до
ANISOTROPIC для MIN, MAG и MIP. Native backend поддерживает anisotropic
minification. Генератор теперь повышает только MIN на native; запрошенные MAG/MIP
сохраняются. Windows branch оставлена исходной. По [документации D3D9](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dtexturefiltertype)
ANISOTROPIC для MIPFILTER имеет неопределённое поведение. Сырые неподдерживаемые
MAG/MIP значения по-прежнему явно отклоняются. Заглушек обязательных операций нет.

Создание SDK sampler вынесено из настоящего Record в общий `MakeDekoSampler`.
Тест использует именно этот helper и проверяет реальные поля dk::Sampler,
включая max anisotropy и восстановление point filtering; совпадение всех байтов
с прежним inline SDK construction проверено. GPU исполнение этим не доказано.

## Проверки

| Проверка | Результат и граница |
| --- | --- |
| Settings preparation | host ASan/UBSan, ARM64/QEMU, Win32 MSVC PASS: 4 defaults, 64 provided, 19 invalid, 18 scalar fields |
| Оригинальные полные TU ApplySettings | те же три платформы PASS: 128 config и sampler случаев, точные gamma RGB и AF clamp; scalar caps fixture, без Init/GPU |
| Cross ABI observations | settings.bin и observed.bin соответственно побайтно одинаковы на трёх платформах |
| Настоящий Init prefix | host ASan/UBSan и ARM64/QEMU PASS: default/provided entry, 7 отказов до Init, два cleanup/unbind цикла; намеренный отказ ReadCaps `NATIVE_MATERIAL_CAPS_PENDING` |
| AF baseline | старый manager воспроизвёл неподдерживаемое повышение MAG/MIP |
| AF candidate | host ASan/UBSan и ARM64/QEMU PASS: 160 случаев, actual SDK fields, point restore, invalid filters, propagation ошибок; без GPU |
| devkitA64 / libnx / deko3d | отдельные compile, link, standard NRO packaging PASS, strong undefined = 0 |

Полные команды и хеши находятся в [evidence](matrixgame-e2e565-native-settings-evidence.json).
Сохранённые первоначальные ошибки fixture/link не выдаются за финальные PASS:
они устранены до итоговых запусков, логи оставлены в каталоге этапа.
Большие пройденные ранее map/HAL suites не повторялись.

SDK компилирует именно `source/MatrixGame/src/MatrixSampleStateManager.cpp`.
Из 255 generated original files относительно E564 изменён только этот TU;
прочие хеши совпадают. Upstream pin
`218115a72fa9c5d43269f4ca84a8596a0bcb82f1` clean. Frozen, resources,
сохранения и основные/предыдущие NRO не изменялись. Commit/push не выполнялись.

## Воспроизведение и артефакт

Из корня checkout, с установленным SDK и теми же исходными зависимостями:

```powershell
python tools/matrixgame_native_settings_sdk.py
```

Wrapper конфигурирует отдельный каталог и вызывает стандартную CMake target
`matrixgame-native-diagnostic-nro`; точная SDK команда записана в
`build/matrixgame/e2e565-native-settings/sdk/result.json`. Основная NRO не затрагивается.

NRO: `build/matrixgame/e2e565-native-settings/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro`

Размер: 6969541 байт. SHA256:
`d0d760b08039d0071f49ab10244a1bdd362506dfa44b1eb0fb56364a7d8b2bd3`.

Аудит без повторного запуска тестов, с новым именем выходного файла:

```powershell
python tools/matrixgame_native_settings_audit.py --output build/matrixgame/e2e565-native-settings/audit-repeat.json
```

## Достигнутая цепочка и оставшиеся компоненты

Последнее аппаратное свидетельство из logs2 остаётся
ReadParams → ApplyGammaRamp до map constructor. E558 реализовал gamma pass;
E559 проверил исходный ReadParams на CPU. E565 проверяет копирование настроек,
их настоящие config/sampler consumers и ранний Init rollback, но нового
аппаратного прохода ReadCaps, gamma, PrepareMap или game loop не заявляет.
Ранее проверенная интеграция StoredStateBackend и отдельного minimap output
provider сохраняется; этот этап её не заменяет.

Открыты реальные Rangers resource/audio callbacks standalone приложения:
оригинальный CSound использует символические Sound.* names, существование bridge
и отдельного PCM output ещё не означает подключения host services. Необходимы
последующее выполнение native Init/map, проверка изображения/VRAM/HID/focus,
presentation и recovery на устройстве. Новые quality settings не означают
поддержку всех FSAA/BPP режимов; действуют реальные backend capabilities.
Перенос и аппаратный запуск выполняет пользователь. Отсутствие устройства
ограничивает runtime evidence, но не запрещает дальнейшую офлайн-разработку.
