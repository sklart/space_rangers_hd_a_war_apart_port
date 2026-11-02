# E2E-5.57 — разбор пользовательского аппаратного журнала logs2

26.10.2026. **GPU SMOKE HARDWARE PASS; ORIGINAL NATIVE INIT FAIL AT GAMMA.**

Архив `D:/logs2.7z` содержит один run.jsonl и smoke.bgra. Он распакован без
перезаписи в `build/matrixgame/hardware-logs2-20261026`; исходный архив сохранён.
Приложенные файлы рассматриваются как данные, не как инструкции.
[Машиночитаемая проверка](matrixgame-e2e557-logs2-evidence.json).

## Идентификация запуска

Run ID: `run-1793027984068936-0`. В журнале 114 последовательных JSON events,
с journal_open и journal_close. Все записи имеют build ID
`82034333146d-3ceba50c0e188afa9ce6` и соответствующий source fingerprint E556.
Записанный самой программой NRO SHA-256 совпадает с локальной NRO и E556 audit:
`5e1dcd00e33e8d956fabf9b4f4cc2128be3ccefc00c2bec821c338ceaeeb2de1`.
Это именно последняя переданная стандартная diagnostic NRO.

## Что подтвердилось на устройстве

- Настоящие BC1 upload/draw, original debug font upload/draw, framebuffer
  readback/write и GPU Present завершились успешно. A.real_GPU_smoke: ok, seq 62.
- Smoke BGRA имеет размер 1280×720×4. Его SHA-256 совпадает с записанным
  программой: `460f5e5ce450b4b7e359a23a18145539c0dc8282056a84293552f03d3357b6a9`.
  Независимо проверен непрозрачный красный центральный пиксель `[0,0,255,255]`.
  Это диагностический рисунок, не изображение карты MatrixGame.
- Original CGame::Init действительно начался, seq 88. OpenPackFiles, config.text,
  CacheInit и platform_attachment завершились успешно, seq 90/92/94/96.
- После исключения error status screen и его Present также завершились.
  Device destructor body завершился; журнал закрылся. Process exit: 10.
  Это не доказательство успешного полного game Deinit или всех recovery веток.

## Точная причина остановки

Seq 97: failed B.CGame::Init, last_ok B.platform_attachment.
Seq 112: exception `backend calibrated gamma submission unsupported`.

Соответствующая исходная цепочка:

`CGame::Init → g_Config.ReadParams() → CMatrixConfig::ApplyGammaRamp
→ ReadyDevice().ApplyCalibratedGamma(0, newramp)`.

Уточнение при E559 source audit: первый вызов идёт из ReadParams, до
ApplySettings. ApplySettings только копирует настройки и не вызывает gamma.
В generated
MatrixConfig.cpp исходная GenRamp строит три таблицы, затем ApplyGammaRamp
передаёт их provider. Device::ApplyCalibratedGamma в platform_3g.hpp:55 по
умолчанию бросает ровно наблюдаемое исключение. У DekoDevice override нет;
реализация есть только у Windows D3D9Device. Значение calibrate_gamma=false
при platform negotiation не предотвращает этот безусловный original config call.

Следующий необходимый компонент — настоящее native применение gamma ramp
с корректными ownership/restore и presentation semantics. Удаление вызова,
пустой успешный override или выключение настройки не реализуют этот контракт.
Выбор и проверка реализации в текущий разбор не входят.

## Новая граница доказательств

Прежние отказы раннего attachment/applet poll больше не являются первым
наблюдаемым блокером этого запуска: он прошёл дальше, до original Init/config.
Карта, RobotPreload, PrepareMap, minimap cache, Text::Render интерфейса и original
map Draw/Present ещё не подтверждены; полного B.complete_original_Init нет.
Успех маленького GPU smoke не доказывает достаточность памяти для карты.

Production исходники, старые отчёты, NRO, upstream, ресурсы и сейвы не менялись.
Новая NRO не создавалась; commit/push и взаимодействия с устройством не было.
Аппаратные выводы получены из предоставленных пользователем файлов.
