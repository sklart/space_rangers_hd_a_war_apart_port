# MatrixGame E610 r6: SDK provenance и bounded material packets

Основание: [аппаратный r5](matrixgame-e610-r5-hardware-20261102.md) завершался
в dkMemBlockCreate из DekoCombinerProgram::Prepare при DrawLandscape миникарты.
Доказан DkResult_Fail, не конкретный NV/CPU/GPU OOM. Точный первичный NV Result
в r5 отсутствует; последующая правка не объявляет его восстановленным.

## Исправленная линковка SDK

Причина расхождения: materials.cmake всегда добавлял release deko3d, а позднее
native-engine добавлял deko3dd. Первым разрешался release archive. Selector
теперь объявлен до material target и един для native consumers/SDK probes.
При MATRIXGAME_DIAGNOSTIC_DEKO_DEBUG=ON выбирается deko3dd; OFF сохраняет
обычный release вариант. SDK, upstream и основная NRO не изменяются.

Новый gate разбирает реальные archive object members каждого map: требуются
dk_device/dk_memblock/dk_queue/dk_cmdbuf из выбранной библиотеки и отсутствие
members другого варианта. Для debug проверяются SetContextForDebug и debug
RaiseError(DkResult, char const*) при отсутствии release RaiseError(DkResult).
Package gate повторяет provenance check и включает его в manifest.
Metadata label больше не считается доказательством фактического выбора SDK.
Реальный r5 map принят как release и отвергнут как debug отдельной проверкой.

## Uniform и batch lifecycle

Material fragment/fog/raster/vertex данные объединены в один настоящий SDK
MemBlock 4096 байт: offsets 0/512/768/1024. VertexUniforms занимает 1328 байт;
регионы не пересекаются и соблюдают DK_UNIFORM_BUF_ALIGNMENT. Шейдерные binding
slots, значения, порядок draw и форматы исходных ресурсов сохранены.
Preflight проверяет shading/vertex program до выделения, packet padding обнуляется.
SDK Bind использует GPU address того же блока плюс соответствующий offset.
Фиктивных allocations, пустых UBO или пропусков обязательных операций нет.

До r6 одновременно могли жить 2048 draw packets с тремя GPU blocks на packet
(upload + два uniforms). Теперь максимум 64 packets с двумя blocks на packet
(upload + один uniforms), то есть до 128 blocks packets; другие ресурсы SDK
сюда не входят. ByteLimit 256 МиБ сохранён, счёт bytes учитывает реальный
NativeDrawUpload layout плюс один uniform block. Count bound независим от bytes:
множество небольших allocations не может ждать достижения прежнего byte budget.

Drain сохраняет логическую frame/scene, submit/wait подтверждает completion,
и только затем освобождает packet/image/target owners. При отказе completion
owners удерживаются. Не отключаются SDK ошибки, проверки state или draw calls.
Добавлены batch_drains/max_live_draw_packets/max_live_draw_bytes counters.
Это снижает давление числа живых mappings, но не доказывает точную историческую
причину NV Fail. Больше waits может ухудшить FPS; ускорение не заявляется.

## Короткие проверки

- Windows/MSVC, host ASan/UBSan/LSan и ARM64/QEMU UBSan: PASS packet bytes,
  offsets/alignment, отсутствие overlap, padding, независимые packets,
  short storage и invalid shading отвергаются до записи.
- По двум сохранённым Windows captures: 92 frames, 208808 primary draw calls,
  max frame 4038, max batch 64, 3310 submissions. Проверены точный порядок,
  полный retirement и удержание owners при injected submit/completion failure.
  Это CPU queue model и реальные данные счётчиков, не новый бой или GPU proof.
- Независимый CSV oracle подтвердил точные новые totals; assertions не ослаблены.
- Извлечённый r5 header с лимитом 2048 провалил точный assertion нового bound
  (expected exit 6). Baseline/2 дополнительно проверяет причину отказа по assertion.
- Полные Windows бои и общие регрессии не повторялись.

Результаты: build/matrixgame/e610-material-r6-checks/{windows,host,arm64}/1/
и baseline/2/. Программный успех этих проверок не заменяет hardware runtime.

## Открытая граница

Сохраняются реальные Init/Takt/Draw entry, managed residency, staging setters,
VI owner/SDL binding и orientation readback. Пакет standalone: Campaign/Gov entry,
возврат результата в Galaxy и победа не проверяются. Полный Init/миникарта,
четыре видимых игровых кадра, GPU/HID и FPS требуют ручного запуска r6.
[Инструкция](../docs/matrixgame-e610-material-r6-readme.ru.md).
