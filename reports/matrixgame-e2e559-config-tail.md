# E2E-5.59 — полный original ReadParams после gamma

26.10.2026. **FULL ORIGINAL ReadParams CPU PASS; NATIVE MAP/INIT HARDWARE PENDING.**

Проверка продвинула CPU integration через оставшуюся после gamma часть
оригинального parser: damages, цены/характеристики, key actions, labels,
descriptions и camera parameters. Production renderer не менялся; подготовленная
E558 NRO сохранена побайтно. [Финальное evidence](matrixgame-e2e559-config-tail-evidence.json).

## Уточнение причины аппаратного отказа

При source audit обнаружена неточность в E557: первый вызов gamma идёт из
`CGame::Init → g_Config.ReadParams → ApplyGammaRamp`, до `ApplySettings`.
`ApplySettings` только копирует настройки. Следующий вызов ApplyGammaRamp
есть в оригинальном PrepareMap. Наблюдённое исключение и идентификация NRO
в logs2 остаются прежними; исправлены только объяснение caller и Init sequence.

## Что добавлено

Отдельный проект `port/matrixgame/native-config-test` собирает **полный
MatrixConfig.cpp**, с полным объявлением CMatrixConfig, настоящими CHeap,
CBlockPar/CFile и исходным `CFG/robots/data.txt`. Это не извлечение одного
GenRamp и не минимальное объявление класса из E558 scalar test.

Только device routing в этом тесте — явный GammaOnly fixture, использующий
production NativeGammaStore. Его ReadCaps и AvailableTextureBytes бросают
Unsupported: никакое создание GPU, window attachment, Init или map construction
не имитируется успешным ответом. `Current().ready` выставляется только для
компонентной диспетчеризации ReadyDevice; это не результат negotiation.
Тестовые globals указывают на реальные heap/data owners этого процесса.

Сначала actual base Device::ApplyCalibratedGamma воспроизводит точный отказ
E557 на реальном parser. К этому моменту cursors уже созданы, labels/descriptions
ещё отсутствуют. Затем GammaOnly принимает исходную WORD таблицу, и тот же
полный ReadParams заканчивается без исключения. Повторный ReadParams использует
его настоящий Clear. После двух повторов Clear вызывается дважды. Всё повторено
во второй отдельной heap/data/gamma-owner session.

## Результаты

| Gate | Доказательство |
| --- | --- |
| HOST ASan/UBSan | PASS, без отключения sanitizer для config TU. Четыре полных ReadParams, две baseline-refusal/retry сессии; настоящий parser tail и повторный cleanup. |
| Linux ARM64/QEMU | PASS, тот же full TU/parser/data/production gamma store, четыре возврата ReadParams. |
| Windows Win32 reference | PASS, MSVC full native-config TU и native file/heap backend, wchar16. Это CPU reference, не D3D9 HAL и не новый полный Windows Init. |
| Strings | Реально созданы по 25 labels и descriptions. Проверены source values в начале labels и в конце descriptions; пять cursors соответствуют actual block. |
| Numeric/config observations | Сверены damages, AI strengths/radii/cooldowns, building hitpoints, key actions, timings, prices, overheat, item chars, selected camera values и все cursor/label/description strings. Часть numeric arrays и strings сериализованы без pointers и struct padding. Четыре прохода внутри каждой платформы совпали; затем совпали все три platform blobs. |
| Input integrity | data.txt SHA до/после одинаков; все 255 generated E558 original source files проверены. |

Общий SHA-256 наблюдений:
`882fde997f987ef037f2665b960a123e75593febbc15b1a376e25cb5ed774fc2`.

Этот hash относится к выбранным сериализованным значениям, не к полному memory
image CMatrixConfig и не к карте. Четыре возврата оригинального метода доказывают
исполнение всей его CPU body; hash не расширяет это до Init/игрового поведения.
Синтетические label/description cleanup arrays из прежней prefix-проверки
здесь не используются.

## Сборка и воспроизведение

```powershell
$taskPython = 'C:/Users/Sklyarov/AppData/Local/Programs/Python/Python313/python.exe'
& $taskPython tools/matrixgame_config_tail_checks.py host
& $taskPython tools/matrixgame_config_tail_checks.py arm64
& $taskPython tools/matrixgame_config_tail_checks.py windows
& $taskPython tools/matrixgame_config_tail_audit.py --output build/matrixgame/e2e559-config-tail/recheck.json
```

Checks используют проверенный read-only `build/matrixgame/e2e558-gamma/sdk/source`.
Результаты/полные команды/бинарные files: `build/matrixgame/e2e559-config-tail`.
WSL использует доступный make. Host CMake проходит нормальное compiler ABI
определение в отдельном build-native; ASan/UBSan наследуются от native-core.
Для ARM64/Windows передаются настоящие generated PNG/JPEG configuration headers
от тех же уже имеющихся dependency builds. Системные пакеты не устанавливались,
старые build directories и их результаты не удалялись. Новые вызовы audit
создают JSON эксклюзивно. Hashes codec binaries записаны в финальном evidence.

Production input files из E558 metadata все совпали с текущими hashes; поэтому
новая SDK/NRO сборка не требуется для этих test-only additions. Последняя NRO
остаётся [E558 gamma candidate](matrixgame-e2e558-gamma.md), SHA-256
`2fb8689975480771e690cd764b89af803c6e2d2cd234fe24904c18cb7c86fdd1`.

## Границы

Новый run подтверждает полный **ReadParams с gamma-only CPU provider**.
Он не выполняет g_Sampler.ApplySettings, map constructor, RobotPreload,
PrepareMap, deko image allocation или map Draw/Present. Их прежние независимые
доказательства не заменяются этим parser test. Следующая фактическая аппаратная
граница после E558 gamma candidate всё ещё неизвестна; E557 logs2 остановился
раньше карты. E558 на устройстве ещё не запускалась.

Сохранность проверена: E558/E556 и primary/cumulative NRO, оба прежних dist
пакета, upstream pin/clean status, 15 protected inputs, extra frozen artifacts,
CFG/robots.pkg, 107 hardware inputs и исходный пользовательский logs2 archive.
Новых NRO, commit/push, переноса файлов на Switch и взаимодействия с устройством нет.
