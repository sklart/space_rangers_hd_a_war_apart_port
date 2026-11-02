# E2E-6.01 — snapshot/cleanup closure и seed революции

Host ASan/UBSan/LeakSanitizer PASS без suppressions: настоящая кампания,
взлёт/посадка, Gov OnOpen/OnClose, original memory-only snapshot round-trip и
полный cleanup завершились exit0. Turn306, 73 звезды, 344 планеты, 41 рейнджер,
Player id1, planet id15, money870, storage3. Script RNG не фиксировался.

Этот gate использует E600 loader ownership fixes: прежний TShip.RangerRelations
list и пустой TConstellation.OutlinePolygons освобождаются перед заменой при
загрузке. Предшествующий E600 LSan failure30200/1214 сохранён; следующий E600
warmup UBSan failure не являлся повторной оценкой этих утечек.

Новый случайный мир выявил signed overflow `4 * 1069127773` в текстовом seed
TriggerGovernmentRevolution. Единственное исправление выражения:
`CurrentTurn / 10 * int32(GenerationSeed)` → `CurrentTurn / 10 * GenerationSeed`.
Деление выполняется по-прежнему в signed32 с truncation toward zero, умножение
даёт исходные low32 через unsigned32, передача в original int32 API сохраняет
биты по C++20. RNG calls, выбор правительства, новости и награды не подменяются.
77 независимых constexpr checks (11 turn boundaries ×7 seeds, включая точный
найденный случай) PASS host GCC и devkitA64. Это pure arithmetic compile proof.

ARM64/QEMU E600 snapshot-arm64/2 также завершился exit0: turn308, 73 звезды,
336 планет, 36 рейнджеров, money760, storage3, настоящий snapshot и cleanup.
Он использует непосредственно предшествующий код без нового revolution seed.
Полный ARM64 runtime с этим последним выражением не повторялся: delta отдельно
подтверждена 77 devkitA64 assertions и SDK compile/link. Различие версий явно
сохранено в [component evidence](matrixgame-e2e601-revolution-seed-evidence.json).

SDK: новый отдельный `liboriginal-rangers-host-revolution.a` заменяет только
aPlanet.o относительно E600; остальные377 original members неизменны. Standard
campaign/achievement targets и retained API AArch64 ELF PASS, strong undefined=0;
runtime NOT RUN. Прежние библиотеки, ELF и NRO сохранены.

При проверке production generator обнаружен собственный пропуск подключения
E599 overlays: aPlanet/aRanger/EC_Ether/aGalaxyEvent/EC_Expression выходили через
ранний return до применения helper. Routing исправлен. В fingerprint source
cache добавлены все четыре campaign helpers, чтобы их изменение инвалидировало
кэш. Старый большой effective-source corpus не перезаписывался.

`production-pipeline/4/result.json` PASS: на свежем настоящем script-width
overlay вызван штатный generated_source; все восемь полных CPP byte-identical
исходникам, реально скомпилированным в SDK. Дополнительно сравниваются 13
затронутых методов. Это закрывает именно generator/compiled-source equivalence,
не выдаётся за дополнительный runtime. Первая generator-попытка с ранним return
сохранена как INCOMPLETE; последующие маршруты проверены отдельно.

Воспроизведение новых адресных gates:

```text
python tools/matrixgame_campaign_revolution_checks.py
python tools/matrixgame_campaign_revolution_sdk.py
python tools/matrixgame_campaign_pipeline_checks.py
python tools/matrixgame_campaign_revolution_audit.py --output reports/matrixgame-e2e601-revolution-seed-evidence-new.json
```

Создание SDK dependency/fixture происходит в новых каталогах и запрещает
перезапись готового архива. Текущий existing SDK incremental reconfigure/link
выполнялся через сохранённый fixture/sdk-closure.py с history сохранением.
Защищённые game/frozen fingerprints, upstream pins/clean state и прежние NRO
проверены аудитом. Аппаратных действий, commit/push и изменений сохранений не было.
Полный бой и возврат результата проверяются следующим этапом E602.
