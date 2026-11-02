# E2E-5.75 — joint native/deko3d + owning Rangers diagnostic NRO

Статус: **SDK compile/link/standard NRO package/audit PASS; HARDWARE_PENDING**.
Новая отдельная NRO подготовлена; основная игровая NRO и прежние артефакты
сохранены. Полный successful native Init/map этим не доказан.

## Изменения

В стандартной diagnostic build добавлен опциональный
`MATRIXGAME_DIAGNOSTIC_RANGERS_HOST=ON`. Только в этой конфигурации native-engine
подключает существующий native callback target, проверенные original Rangers
archive, OKGF/softfloat и обычные portlibs. Native-core target используется один;
дублирования package-reader implementations нет. Цель `matrixgame-native-diagnostic-nro`
остаётся отдельной от основной NRO и ALL.

Entry создаёт RangersHostSession после GPU smoke, выбирает явный Rangers read
root и private run-specific user root, передаёт full16 services в native options.
Host объявлен до runtime и разрушается после него. Success/error branches явно
закрывают runtime перед host; cancellation использует тот же порядок RAII.
Exception screen показывается после cleanup original runtime/host.

MatrixGame read root и Rangers install root независимы. Это учитывает реальную
ошибку test setup E574: native Init ожидает исходные CFG/robots text files.
Default paths и argv overrides описаны в пользовательской инструкции.

Fingerprint теперь включает признак host-enabled, hashes трёх original external
archives и dependency manifest. CMake отслеживает эти inputs при reconfigure.
Свежие native/host objects/archives и SDK link inputs записаны после сборки.
Исходный installed SDK и original source pins не изменены.

## Готовые артефакты

NRO:
`build/matrixgame/e2e575-hosted-diagnostic/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro`

14891205 bytes, SHA256
`9dcd90d5175bc0ae7cbfe6db75b180b1fca406b01655880cf64d6f4f31cab356`.

ELF: 53729208 bytes, SHA256
`fe1f702c45537986aa2c4c3461faf86e9342e6ef685600bd4e355d9c0b6bd82a`.

Первый успешный build до добавления audit tool сохранён в `sdk/first-build`;
финальный build пересобран инкрементально для актуального source fingerprint.
Старые E568 и прочие NRO не перезаписывались.

## Проверки

- Standard SDK configure/build target, AArch64 PIE link и strong undefined = 0.
- В одном ELF реально присутствуют CGame::Init, NativeInitRuntime,
  DekoDevice, RangersHostSession, Data/Audio/Music owners и все original callbacks.
- NRO segment bounds, ASET, embedded icon/NACP, пять original font assets и
  build-id/source/dependency fingerprint PASS.
- Актуальность E574 host/ASan/lifetime artifacts сверена с текущими sources/objects;
  большие CPU suites повторно не запускались.
- Clean upstream cpp/matrixgame/okgf pins, protected frozen/primary/cumulative
  NRO, старый E568 и user logs2 archive сверены. Game inputs не изменены.

Evidence: `reports/matrixgame-e2e575-hosted-diagnostic-evidence.json`.
Точный план 26 ресурсов с source paths/bytes/SHA256:
`reports/matrixgame-e2e575-hosted-diagnostic-evidence-resources.json`.
План создан read-only; ресурсы не включены в NRO и не переносились.

## Достигнутая Init-цепочка и ограничения

Программно объединены native output-folder/CFile, StoredStateBackend и прежние
native graphics/input/screenshot компоненты с real Rangers data/config/packages,
sound/AFT/music, owning host и copied callback table через original Deinit.
Joint SDK dependency closure и отдельная стандартная NRO теперь доказаны.

Это сборочное доказательство. Последнее исполнение этой композиции — host E574
до явного Unsupported ReadCaps, с rollback и реальными callbacks. Combined NRO
ещё не исполнялась. Полный native Init/map, first map frames и последующее
поведение остаются открытыми; hardware boundary logs2 по-прежнему
ReadParams → ApplyGammaRamp, до map constructor.

Нужны ручной запуск новой NRO и фактическая следующая граница, graphics/readback,
audio/HID/focus/F9/recovery. ARM64/QEMU и Windows execution новой hosted цепочки
также NOT RUN. Агент не взаимодействовал со Switch. Commit/push нет.

Инструкция и воспроизведение:
`docs/matrixgame-e2e575-hosted-diagnostic.md`.
