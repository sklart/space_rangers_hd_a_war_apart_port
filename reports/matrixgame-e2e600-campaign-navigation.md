# E2E-6.00 — настоящий перелёт и открытие правительства

Продолжение: E601 закрывает host snapshot/cleanup gate с явной границей версии
ARM64; E602 подтверждает настоящий бой до естественного поражения и обработку
результата в Gov. См. [E601](matrixgame-e2e601-revolution-seed.md) и
[E602](matrixgame-e2e602-campaign-frun.md). Ниже сохранены исходные попытки E600.

Проверяется Windows reference с частичной MSVC ASan instrumentation. Исходная
кампания создаётся заново без чтения/записи сохранений и без seed override;
исправленные E599 объекты повторно используются по SHA256. Вывод изолирован
в `build/matrixgame/e2e600-campaign-navigation`, игровые входы read-only.

`windows/1/result.json` PASS: игрок начал turn301 на станции рейнджеров,
исходный OrderTakeoff и calculation worker выполнили взлёт до turn302.
OrderLanding(HomePlanet,false) и исходные расчёты player-star/galaxy доставили
игрока на родную планету к turn305. Fixture не присваивает CurrentPlanet,
DockedTo, Position, CurrentStar или параметры мира и не подставляет объекты.
Порядок вызовов расчёта тот же, что у original synchronous turn callers.
Это CPU navigation proof, не визуальное прохождение star-map UI/film loop.

`government-windows/3/result.json` PASS: новая реальная кампания, взлёт,
посадка к turn314, затем настоящий TfGov::InitializeLayout, OnOpen и OnClose.
PortraitPanel активна, исходный DialogText содержит 492 UTF-16 units.
Штатный диалог построен из настоящего Player/Planet и original definitions.
После этого выполнены original world/worker cleanup и владельцы host/UI/script.

Отрицательные попытки сохранены. government-windows/1 — fixture compile error
(у WideString нет empty). government-windows/2 — ASan null read в
ScheduleCallbackTimer из TfPanelMain::OnOpen. По исходникам обнаружена пропущенная
отдельная layout-фаза: InitializeGlobalUiRuntime создаёт config trees, но Screen
в MainPanel/PlanetPanel/LoadPanel связывается только InitializeLayout. В третьей
попытке вызван настоящий метод; nil Screen не замаскирован проверкой или fake timer.

Воспроизведение:

```text
python tools/matrixgame_campaign_navigation_windows.py
python tools/matrixgame_campaign_navigation_windows.py --government
```

`snapshot-windows/3/result.json` PASS: original memory-only Save/Restore выполнены
после настоящей посадки и Gov OnOpen. Мир отсоединён, old Galaxy уничтожена и
пересоздана исходным загрузчиком. Сохранены Player id/name/money, planet id,
storage count, stars/planets, turn310 и persistent SaveCount/LoadCount. Проверено
отклонение Galaxy() и duplicate owner при отсоединённом мире; после restore
Galaxy() возвращает актуальный глобальный объект. Финальный cleanup exit0.
Snapshot не сохраняет файлы: в private runtime остались только startup/perf logs.

Для этого в production добавлен `RangersAchievementSession`: настоящий
InitializeAchievementDefinitions создаёт 82 определения, оригинальный local
backend читает только configured user directory через LoadLocalAchievements.
Никакие достижения не подставляются; обязательный original lookup при Player
LoadFromBuffer теперь получает действующий сервис. Owner освобождает настоящий
деревовидный ресурс после мира и до data host. Campaign owner владеет эксклюзивным
сроком жизни global campaign slot: исходный memory snapshot законно заменяет
Galaxy, поэтому хранить прежний адрес как идентичность владельца было неверно.
Возвращённые ранее references на мир/игрока должны быть получены заново после restore.

snapshot-windows/1 сохранила compile-only ошибку смешанного auto declaration;
snapshot-windows/2 — ASan nil AchievementDefinitions из GetAchievementData
при original TPlayer::LoadFromBuffer. Исправления runtime проверены третьей
попыткой, без отключения ASan и без замены чтения нулевыми значениями.

Host snapshot-asan/1 выполнил настоящий round-trip и cleanup, но LeakSanitizer
FAIL:30200 bytes/1214 allocations. Стеки выявили перезапись constructor-owned
TShip.RangerRelations list и пустого TConstellation.OutlinePolygons при загрузке.
Адресный snapshot overlay освобождает прежние ресурсы перед заменой; исходные
записи, разрешение IDs и snapshot XOR/restore semantics не меняются.
snapshot-windows/4 с этими исправлениями PASS: turn310, 73 звезды, 326 планет,
41 рейнджер, Player id2, planet id15, money800, storage3, original Gov dialog537.

Повторный snapshot-asan/2 остановлен UBSan ещё в generation warmup: signed
overflow `4 * 1069127773` в TriggerGovernmentRevolution, вызванном другим
случайным миром. Это не повторная оценка утечек snapshot: до round-trip данный
прогон не дошёл. Новый candidate `matrixgame_campaign_revolution_overrides.py`
сохраняет low32 текстового seed; 77 независимых compile-time oracle assertions
PASS host/devkitA64. Candidate runtime проверяется отдельно в
`build/matrixgame/e2e601-revolution-seed/1`; integration в production generator
пока PENDING, чтобы не менять fingerprints живой E600 ARM64 проверки.

SDK standard campaign/achievement targets и retained API AArch64 ELF — PASS,
zero strong undefined, compile/link only. После исправлений загрузчиков создан
новый `sdk-dependency/liboriginal-rangers-host-snapshot.a`: относительно E599
заменены только aGalaxy.o/aShip.o, остальные376 members сохранены. Отдельная
новая библиотека достижений включена в real closure. SDK result-1 и history
сохраняют прежний proof. Campaign library SHA256
`d1d87377c095df224e891beea2de646515b8e3d45b0420f58f2a3a9626dfd0ef`,
retained API ELF SHA256
`da32542eee97974a61b5834276ac534500036a9ed1f79a919a6e2e81c3012ddf`.
Этот SDK proof ещё не включает candidate revolution seed.

ARM64 snapshot-arm64/1 runner остановился до compile при чтении flags из старой
failed попытки. Исправленный runner использует actual successful E599 flags;
snapshot-arm64/2 прошёл все пять compile и link, QEMU runtime выполняется.
Общий E600 sanitizer gate пока FAIL, strict audit запускать как PASS нельзя.

Дополнительное воспроизведение:

```text
python tools/matrixgame_campaign_navigation_windows.py --snapshot
python tools/matrixgame_campaign_snapshot_cpu.py asan
python tools/matrixgame_campaign_snapshot_cpu.py arm64
python tools/matrixgame_campaign_snapshot_sdk_dependency.py
python tools/matrixgame_campaign_snapshot_sdk.py
python tools/matrixgame_campaign_revolution_checks.py
```

Полный Robot::FRun из Gov, обработка реального результата, награды/история/возврат,
естественная победа и native/deko runtime остаются NOT RUN. E593 natural defeat
в отдельном FRun fixture не является доказательством этой интегрированной цепочки.
Исходные StartPlanetBattle* UI handlers вызывают SaveGameToFile(auto-save);
их полный button workflow пока не запускался при запрете на изменения сохранений.
Это отдельная граница UI, не причина подменять или пропускать обработку результата.
Новая NRO не собиралась; аппаратных действий, commit/push, изменений сохранений
или upstream не было.
