# MatrixGame E2E-5.75: отдельная diagnostic NRO с Rangers host

Статус: **SDK compile/link/package PASS; Switch NOT RUN / HARDWARE_PENDING**.
Это новая отдельная сборка. Основная игровая NRO и прежняя E568 сохранены.
Аппаратный запуск и перенос файлов выполняет пользователь.

## Готовый бинарник

`build/matrixgame/e2e575-hosted-diagnostic/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro`

Размер: **14891205 bytes**.
SHA256: `9dcd90d5175bc0ae7cbfe6db75b180b1fca406b01655880cf64d6f4f31cab356`.

Обычный будущий путь на SD:
`sdmc:/switch/matrixgame-diag/matrixgame-native-diagnostic.nro`.
Бинарник создан стандартными SDK `nacptool`/`elf2nro`; icon/NACP/fonts встроены,
отдельный RomFS не требуется. Запуск требует hbmenu application mode, как прежняя
диагностика. Приложение само проверяет доступную среду libnx/HID.

## Ресурсы для будущего ручного теста

Сборка использует два явно разделённых read roots:

| Назначение | Путь по умолчанию |
| --- | --- |
| MatrixGame text CFG и robots.pkg | `sdmc:/switch/matrixgame-diag/game` |
| Original Rangers install/config/packages | `sdmc:/switch/matrixgame-diag/rangers` |
| Логи и вывод MatrixGame | `sdmc:/switch/matrixgame-diag/logs/<run>/` |
| Private Rangers user/config | `sdmc:/switch/matrixgame-diag/logs/<run>/rangers-user/` |

Точный read-only план —
`reports/matrixgame-e2e575-hosted-diagnostic-evidence-resources.json`.
В нём 26 source/destination entries с размерами и SHA256. Ресурсы не включены
в NRO и агентом не переносились. Нужны оригинальные пользовательские данные.

MatrixGame root по-прежнему содержит три файла: оригинальный DATA/robots.pkg и
исходные upstream CFG/robots/data.txt, iface.txt. Этот root не заменяется чистой
установкой Rangers без двух текстовых CFG. Добавлять вместо них зашифрованный
Rangers robots.dat не требуется.

Rangers root содержит INSTALL.TXT, INSTALL_RUSSIAN.TXT, CFG/Main.dat,
CFG/CacheData.dat, CFG/Rus/Lang.dat, Mods/ModCFG.txt и все 17 пакетов из исходного
base/language install list. Это default Russian/no selected mods profile,
сверенный с оригинальным GR_Main. Не следует переносить настоящие saves или
пользовательские settings: данный entry использует свежий private user root.

При запуске с аргументами argv[1] задаёт MatrixGame root, argv[2] — Rangers root.
В обычном запуске без аргументов используются пути из таблицы. Если пользователь
сам готовит единый read tree с обоими наборами исходных файлов, оба аргумента
могут указывать на него; стандартный manifest описывает два разных root.

## Что делает новая сборка

После прежнего GPU smoke создаётся настоящий RangersHostSession:
data/packages/config → audio/cache → music workers → full16 EngineCallbacks.
NativeGameOptions получает его таблицу; NativeInitRuntime копирует её и держит
binding до завершения original Deinit. Runtime разрушается до host, host — до
deko device. Это сохраняется при успехе, Init failure, отмене и unwinding.

Новые journal stages:

- `B.RangersHostSession` — создание реальных сервисов;
- `B.complete_original_Init` — только после возврата настоящего CGame::Init;
- `D.RangersHostSession` или `.error` — освобождение после runtime.Deinit.

Entry по-прежнему выполняет bounded original form/loop и требует четыре
завершённых map frames для полного runtime PASS. B/+ отменяют работу на безопасной
контрольной точке. Результаты сохраняются в отдельном run directory.

## Что подтверждено и что ещё открыто

Joint AArch64 PIE содержит original CGame::Init, native/deko backend, owning
Rangers host и все 16 original Robot functions. Strong undefined = 0. Проверены
NRO segments/ASET/icon/NACP, embedded fonts и fingerprint оригинальных
dependencies. Сами combined NRO/SDK ELF **не запускались**.

Прежние актуальные CPU доказательства E574: четыре original audio modes,
ASan/UBSan новых TUs, реальные text/sound callbacks через два ранних native Init
rollback cycles. Они подтверждают lifetime/dispatch, но не полный Init/GPU.

Последняя аппаратная граница по logs2 остаётся ReadParams → ApplyGammaRamp,
до map constructor; данная сборка её не продвигает без ручного запуска.
Нужны новая фактическая Init/map граница, graphics/readback/audio/HID/focus/F9/
recovery и первые полные map frames. ARM64/QEMU и Windows execution новой hosted
цепочки также не выполнены. Не следует считать сборочный PASS runtime PASS.

Из нового ручного run полезны весь run directory с run.jsonl и появившимися
readback/screenshot files; отсутствие файла или более ранний отказ тоже является
результатом. Агент устройство не использовал.

## Повторение сборки

Из текущего локального checkout с devkitPro и прежними проверенными dependencies:

```powershell
python tools/matrixgame_hosted_diagnostic_sdk.py
python tools/matrixgame_hosted_diagnostic_audit.py --output reports/matrixgame-e2e575-recheck.json
```

Стандартная отдельная цель:

```powershell
cmake --build build/matrixgame/e2e575-hosted-diagnostic/sdk --target matrixgame-native-diagnostic-nro
```

Это инкрементальный рецепт, не bootstrap из пустого checkout: требуется E573
original Rangers dependency archive и реальные OKGF/softfloat archives. Точная
configure command, fonts/SDK inputs и hashes сохранены в SDK result.json и
native-diagnostic-build.json. Выходы остаются внутри отдельного build directory.

Commit/push, изменения upstream/frozen/ресурсов/сейвов и перенос на Switch не
выполнялись.
