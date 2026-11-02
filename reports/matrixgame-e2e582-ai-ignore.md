# MatrixGame E582: original AI ignore FIFO — overlap и время новой цели

Статус: дефект воспроизведён MSVC ASan, исправлен в generated port source;
адресный real-map oracle и Windows HAL regression PASS. Host ASan/UBSan и ARM64
delta **compile-only PASS**. Новая отдельная SDK NRO compile/link/package PASS,
strong undefined = 0. Switch **NOT RUN**.

## Найденный дефект

Продолжение проверки pointer/FIFO операций после прежних 64-bit sorting fixes
обнаружило две перекрывающиеся `CopyMemory` в `CInfo::AddIgnore`. При заполнении
16-entry списка обе области сдвигаются на один элемент влево: 15 указателей и
15 временных меток. Прежний generator превращал эти вызовы в `std::memcpy`,
что остаётся undefined behavior. Это дефект и на x86, и на 64-bit ABI;
доказательство не следует называть pointer-truncation ошибкой.

На настоящем загруженном MatrixGame мире baseline probe вызвал оригинальный
CInfo с 17 настоящими world object identities. MSVC AddressSanitizer остановил
первое переполнение с **memcpy-param-overlap**, две области длиной 120 bytes
(15 x 8-byte pointers) перекрываются со сдвигом 8 bytes. Baseline source,
executable, link inputs, stdout и sanitizer report сохранены отдельно:
`build/matrixgame/e2e582-ai-ignore/baseline/` и `baseline-environment.cpp`.

По исходнику также установлено: после сдвига новая цель записывается в последний
слот, но временная метка этого слота не обновляется. Поэтому срок её
игнорирования мог считаться от прежней цели. Baseline ASan останавливается
раньше этой проверки; отдельный runtime baseline этой второй ошибки не заявляется.

## Исправление

`tools/matrixgame_ai_owners_overrides.py` теперь генерирует два `std::memmove`
с typed element sizes и записывает `g_MatrixMap->GetTime()` новой цели.
FIFO из 16 целей, duplicate refresh, выбор пустого/истёкшего слота и срок
5000 мс сохраняются. Upstream, игровые данные и форматы не изменены.
Сравнение E580/E582 generated manifests показывает ровно один изменённый файл:
`MatrixGame/src/Logic/MatrixEnvironment.cpp`.

## Проверки и их границы

| Проверка | Результат |
| --- | --- |
| Baseline MSVC partial ASan | Link и реальный original Init/map выполнились; первое ignore FIFO overflow завершилось ASan memcpy-param-overlap, exit 1. Это ожидаемое воспроизведение дефекта. |
| Исправленный CInfo | Два real-map цикла: 17 genuine identities, по 65 overflow shifts, FIFO pointer/time oracle, свежий timestamp, duplicate refresh, deleted slot, expiry boundary и expired-slot reuse PASS. Clock берётся из настоящего g_MatrixMap, не из подставного provider. В fixture задаются только recent/expired timestamps внутри тестового CInfo. |
| Windows HAL + owning Rangers host | Два original Init/transition/four-frame/Deinit цикла, восемь настоящих 800×600 readbacks, полная очистка и gamma/window restoration PASS. Новые пять harness/Environment TUs инструментированы MSVC ASan; sanitizer ошибок после исправления нет. |
| Host ASan/UBSan compile | Изменённый полный Environment.cpp и oracle с актуальными generated headers скомпилированы. **Execution NOT RUN**; это не новый Linux sanitizer runtime PASS. |
| ARM64 compile | Те же два TUs скомпилированы AArch64 GCC. **QEMU execution NOT RUN** для этого нового oracle. |
| devkitA64 | Новая отдельная стандартная диагностическая NRO; оригинальный CInfo::AddIgnore присутствует в SDK ELF. Compile/link/package и hashes проверяет итоговый audit. |
| Switch | NOT RUN / HARDWARE_PENDING. |

Первые MSVC ASan link inputs имели разную STL annotation ABI. Сохранённый
LNK2038 результат исправлен выбором `_DISABLE_VECTOR_ANNOTATION` и
`_DISABLE_STRING_ANNOTATION` для новых TUs, согласованным с существующими
неинструментированными библиотеками. Address instrumentation и memcpy overlap
interception остались включены — последующее baseline падение это подтверждает.
Полный engine/Rangers не объявляется ASan-инструментированным; Windows ASan
не доказывает LeakSanitizer/UBSan execution. Большие уже пройденные suites
не повторялись без основания.

Новый oracle исполняет реальные методы CInfo на настоящем мире и не заменяет
обязательные device/resource операции. Дополнительные AI-бои или управляемый
результат боя не симулируются. Interactive loop E581 остаётся прежним отдельным
доказательством; данная проверка не выдаёт paused readbacks за новый gameplay run.

## Воспроизведение

```powershell
$env:PYTHONIOENCODING='utf-8'
python tools/matrixgame_ai_ignore_native_build.py
python tools/matrixgame_ai_ignore_windows.py --baseline
python tools/matrixgame_ai_ignore_windows.py
python tools/matrixgame_ai_ignore_cross_compile.py
python tools/matrixgame_ai_ignore_sdk.py
python tools/matrixgame_ai_ignore_audit.py --output reports/matrixgame-e2e582-ai-ignore-recheck.json
```

Baseline command намеренно завершается неуспехом из-за сохранённой старой
Environment.cpp. Остальные команды должны пройти. Audit создаёт новый файл
exclusively. [Точные команды, hashes, логи и результаты](matrixgame-e2e582-ai-ignore-evidence.json).

Новая отдельная NRO:
`build/matrixgame/e2e582-ai-ignore/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro`.

Размер **14895301 bytes**, SHA256
`2519b5122f556cf9be1cbd8b53126e1fd8a9e50d833c68c4d763862afa2e0d5e`,
build ID `82034333146d-d694ae629dae07811ef3`. NRO/ASET/NACP/fonts и source/
dependency fingerprints проверены. Все 78 protected game inputs, доступные
frozen/save artifacts, primary/cumulative NRO, logs2.7z и clean upstream pins
проверены и сохранены.

Прежняя E580 NRO сохранена с SHA256
`b6ec6ce859ce21c9471084d1e63be8a45d7f492f0f98a31e0b3cb421e4ee6c8b`.
Стандартная цель и ресурсные roots описаны в
[инструкции E575](../docs/matrixgame-e2e575-hosted-diagnostic.md).

## Остаток

Изменение исправляет настоящую memory/time ошибку оригинального AI, но не
закрывает весь аудит 64-bit исходников и не доказывает полный native/deko Init.
Подтверждённая аппаратная граница по старым logs2 остаётся
ReadParams/ApplyGammaRamp до map ctor. Далее открыты native Init/map/form/loop,
image/audio/HID/F9/focus/recovery, полный бой и возврат результата хозяину.
Устройство и SD не использовались; commit/push не выполнялись.
