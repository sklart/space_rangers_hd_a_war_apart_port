# E2E-5.98 — оригинальная кампания и настоящий Player

Общая цель **PARTIAL**. Оригинальный генератор кампании исполнился до stage 8
на Windows и host; полный host sanitizer gate завершился **FAIL** из-за утечек
памяти. Победа и возврат результата в Gov/галактику в этом сценарии не запускались.
[Итоговый evidence: успешные и неуспешные gates отдельно](matrixgame-e2e598-campaign-evidence.json).

Production `RangersCampaignSession` использует настоящие TThreadCreateNewGame,
Start/IdleEvent и исходный TThreadCreateNewGame_Execute. Пропуски этапов,
подстановка пустого мира, импорт сохранений и принудительные победы не
используются. Сохраняются original Randomize, генерация систем/населения,
назначение квестов, 300 warmup turns, стартовые предметы, preset, scripts и
integrity/module-size операции. Owner требует полный original global UI,
script/data/cache и TCCInterface контекст, отвергает чужую кампанию, хранит
мир до завершения потребителей и вызывает реальные деструкторы worker/world.

Исправлены три реальные зависимости новой цепочки:

- `fGameSettings` выделял 12 bytes под packed TStorageEntry, занимающий 20 bytes
  при native pointers64. Генератор port overlay теперь использует настоящий
  `sizeof(aPlayer::TStorageEntry)`. Upstream и frozen copies не менялись.
- `TQuestMessages` создаётся оригинальным платформенным startup, отсутствующим
  у standalone host. Campaign owner вызывает его настоящий конструктор и
  деструктор. Entries и lookup/fallback semantics остаются исходными.
- `RangersDataSession` теперь выполняет исходный
  `CacheDataRoot->AddMissingFromBlock(LanguageDataConfig->PlanetQuest)` после
  загрузки cache/mods. До этой операции оригинальный генератор останавливался
  на `PlanetQuest.0`; теперь читает реальные квесты из языковой конфигурации.

Windows attempt 5 partial ASan, exit0: stage8, 73 звезды, 347 планет, 45
рейнджеров, ход301, три storage entries; настоящие Player/CurrentStar/HomePlanet
и местоположение в мире проверены. 300 исходных warmup turns подтверждены
логом. Повторный owner отвергается, после настоящих деструкторов глобальные
ссылки освобождены. Сохранения не загружались и не записывались.

Host ASan/UBSan дошёл до stage8: 73 звезды, 333 планеты, 42 рейнджера, ход301,
три storage entries. Но LeakSanitizer при выходе обнаружил **226478 bytes в
4272 allocations**, exit1. Поэтому общий sanitizer result **FAIL**, несмотря
на functional markers. Marker `original_world_worker_cleanup=PASS` означает
проверку возврата деструкторов и очищенных globals; он **не доказывает полного
освобождения памяти**. Sanitizer включён, suppressions не добавлены.

Среди подтверждённых allocation sites: TGalaxy::SimplifyConstellationOutline,
TPolygon2D::MergeUnsharedEdges, TVarArrayEC, TPlanet RangerRelations и
TAchievementStats. Статически найдены конкретные ownership gaps:
TConstellation::RebuildOutlineSegments заменяет list без освобождения старых
segment records; TPlanet_Destroy удаляет relation entries, оставляя сам list;
TPlayer_Destroy не освобождает AchievementStats. Семантику borrowed ArrayValue
нельзя менять на безусловное free: требуется отдельный аудит владения.

Предыдущие неуспешные Windows попытки сохранены: attempt1 — null QuestMessages
в TTextQuest::Reset; attempt2 — неполная генерация с необработанным диагностикой
исключением; attempt3/4 — явный stage3/failure_stage12, затем журнал показал
`TDataEC.PathGetBuf. path=PlanetQuest.0`. Отдельные CDB/MAP материалы являются
DEBUG ONLY. Настоящий perf515 journal включён только в private output; RNG
не фиксировался диагностическим seed override.

ARM64/QEMU compile/link и настоящий полный runtime PASS, exit0: stage8,
73 звезды, 324 планеты, 45 рейнджеров, ход301, три storage entries. Все 300
исходных warmup turns завершены; вывод изолирован. Различие численности
населения между платформами ожидаемо при original Randomize без seed override;
битовая RNG-паритетность этим сценарием не доказывается.

SDK compile-only: в новом `liboriginal-rangers-host-campaign.a` заменён только
`fGameSettings.o`, остальные 377 original dependency members совпадают с E590.
Сохранён прежний WAV compatibility loader. Archive SHA256:
`3132693c7919592b97f7a4baaf2373aeaae14f9dee361e419ddf419d6ce3656c`.
Standard target `matrixgame-rangers-campaign` собрана вместе с зависимыми
global UI/script/data owners. Отдельный retained API AArch64 ELF связан с
новым архивом, strong undefined=0; SDK runtime NOT RUN.

| Артефакт внутри `build/matrixgame/e2e598-campaign` | Байты | SHA256 |
| --- | ---: | --- |
| sdk/rangers/libmatrixgame-rangers-campaign.a | 172710 | 2a5c9f1d60c2c8ce6474bd37d2f877be0374e0da9f659d682e1c39657f72c4fb |
| sdk/matrixgame-campaign-link.elf | 57139736 | 38e0c8e00b08c001ac4e3e3a86ed1f069d3988af0c755a657afa48eb96d954a4 |

Воспроизведение на установленных toolchains и проверенных E597 dependencies:

```text
python tools/matrixgame_campaign_windows.py
python tools/matrixgame_campaign_cpu_checks.py asan
python tools/matrixgame_campaign_cpu_checks.py arm64
python tools/matrixgame_campaign_sdk_dependency.py
python tools/matrixgame_campaign_sdk.py
python tools/matrixgame_campaign_partial_audit.py --output reports/matrixgame-e2e598-campaign-evidence-new.json
```

Новый SDK dependency archive создаётся один раз, прежний не перезаписывается.
Скрипты runtime сохраняют попытки в отдельных каталогах. Strict
`matrixgame_campaign_audit.py` требует все runtime gates PASS и с текущим
LeakSanitizer failure не должен проходить. Partial audit фиксирует failure
явно, без ослабления strict gate. Большой ранее собранный корпус зависимостей
повторно не компилировался; новые owner/data/generator/probe TUs
инструментированы, остальные оригинальные зависимости — частично.

Дополнительно проверен штатный `e2e_source_overrides.generated_source` на
неизменённом upstream fGameSettings.cpp: он действительно выдаёт native
sizeof allocation ровно в одном исходном месте. Генераторный gate сохранён
в `fixture/production-pipeline/result.json`; это отдельная проверка генерации
исходника, не дополнительный runtime. Итоговый partial audit проверил
защищённые game/frozen fingerprints, upstream pins/clean status, primary/root
NRO, пакеты E590 и SDK E592. `D:/logs2.7z` отсутствует: archive NOT VERIFIED.

Следующие обязательные работы: устранение подтверждённых утечек с сохранением
правильного владения; исходный Gov OnOpen, настоящий бой и результат,
награды/история/переход экрана; естественная победа и native/deko FRun runtime.
Новая NRO пока не создавалась: наличие stage8 не закрывает sanitizer gate
или полный battle-return gate. Аппаратную проверку выполняет пользователь.
Сохранения, игровые данные, frozen/upstream и прежние NRO не изменялись;
commit/push и взаимодействия со Switch не было.
