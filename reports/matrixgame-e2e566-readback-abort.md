# E2E-5.66 — отмена обоих readback callbacks

Статус: **host ASan/UBSan, ARM64/QEMU, отдельная SDK/NRO PASS**.
Switch и физическое GPU recovery не запускались. Общий порт не завершён.

При рассмотрении обязательного продолжения native F9 screenshot обнаружен
конкретный дефект владения в существующей диагностике. `AbortNativeFrame`
очищал только `presentation_readback`, оставляя `primary_readback` даже при
отсутствующей или неисправной queue. Primary callback, например в настоящем
`BoundedOriginalLoop::Draw`, захватывает указатель на журнал сессии. После abort
он мог оставаться queued для другого кадра/сессии. Аппаратный вызов устаревшего
указателя или GPU UAF не воспроизведён и не заявляется.

Исправление очищает **оба** callback до проверки queue/Healthy и всех остальных
ветвей выхода. Освобождаются также объекты, удерживаемые захватами. Результаты
queue cleanup и SDK recovery не подменяются успехом. Обычные точки доставки
readback до Present и после gamma не изменены.

## Проверка и её пределы

Сохранена копия production `platform_switch.cpp` до изменения; её SHA совпадает
с E565 build metadata. Harness компилирует **реальный извлечённый префикс**
`DekoDevice::AbortNativeFrame` вплоть до queue-health guard. Остаток, исполняющий
SDK операции, в этот CPU тест не входит. Проверены отсутствующая queue,
unhealthy queue и healthy queue, каждая с четырьмя комбинациями запросов:
нет, primary, presentation, оба. Итого 12 случаев на профиль.

| Профиль | Baseline | Candidate |
| --- | --- | --- |
| Host ASan/UBSan | primary retained = 6; simulated late deliveries = 6 | retained = 0; late deliveries = 0 |
| ARM64/QEMU | primary retained = 6; simulated late deliveries = 6 | retained = 0; late deliveries = 0 |

Дополнительно проверены освобождение weak ownership, отсутствие вызова
callbacks во время отмены, повторный abort и приём нового one-shot запроса.
Simulated late delivery — вызов сохранённого callback harness-ом, не настоящий
Present/GPU run. CPU PASS не закрывает реальные readback/recovery gates.

Стандартная `matrixgame-native-diagnostic-nro` target собрана в новом каталоге:
devkitA64 compile/link, strong undefined = 0, NACP/icon/NRO segments PASS.
Generated original inputs полностью совпадают с E565; единственное изменение
его существующих production inputs — `platform_switch.cpp`. Прежние большие
Init/map/HAL suites не повторялись.

[Машиночитаемое evidence с командами и хешами](matrixgame-e2e566-readback-abort-evidence.json).

## Воспроизведение

Из корня checkout с установленными зависимостями:

```powershell
python tools/matrixgame_readback_abort_checks.py host
python tools/matrixgame_readback_abort_checks.py arm64
python tools/matrixgame_readback_abort_sdk.py
python tools/matrixgame_readback_abort_audit.py --output build/matrixgame/e2e566-readback-abort/audit-repeat.json
```

Последний файл создаётся исключительно новым: используйте другое имя при
повторном аудите. Сохранённый baseline нужен для воспроизведения регрессии.

Артефакт:
`build/matrixgame/e2e566-readback-abort/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro`

Размер: 6969541 байт. SHA256:
`9e25cb5673e106e4d7fd78849d77d7cb11f1027348f9ffbf9541d46a262d1f0b`.

Основная NRO, предыдущая E565 NRO, frozen, игровые ресурсы и сохранения
не изменялись. Protected fingerprints проверены. Upstream clean на pin
`218115a72fa9c5d43269f4ca84a8596a0bcb82f1`; commit/push не выполнялись.

## Продолжение

Native F9 пока остаётся явным Unsupported: этот этап исправляет необходимую
для безопасного request ownership зависимость, но не реализует screenshot.
Для завершения нужны сохранение настоящего выбранного кадра в PNG через
оригинальный CBitmap, private output scope, корректные lifetime/cancel и
проверка ошибок записи. После Present image_slot сбрасывается, поэтому
синхронно читать primary без выбора правильного кадра нельзя: это может дать
не тот swapchain image. Диагностический pre-Present request уже имеет верную
точку захвата; предстоит согласовать её с семантикой оригинального F9.

Последний фактический Switch Init остаётся ReadParams/ApplyGammaRamp до карты;
новых аппаратных сведений нет. Полный native Init/map, реальные Rangers
resource/audio services и аппаратные image/VRAM/HID/focus/recovery gates открыты.
Отсутствие устройства не останавливает дальнейшую программную интеграцию.
