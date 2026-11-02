# E2E-5.60 — source audit следующей Init-границы

26.10.2026. **SOURCE AUDIT ONLY; E558 HARDWARE PENDING.**

Production code не изменён, новые runtime/SDK tests и NRO не создавались.
Цель этой проверки — найти конкретную обязательную недостающую операцию после
E559, не подменяя unknown GPU behavior предположением об успехе.
[Evidence](matrixgame-e2e560-boundary-evidence.json).

## Проверенные участки

| Участок | Установлено по текущим исходникам | Предел |
| --- | --- | --- |
| ReadParams → ApplySettings → sampler → map ctor | Именно такой порядок в actual generated CGame::Init. Полный ReadParams уже исполнен в E559. | E559 — компонентный CPU run, не CGame::Init. |
| CMatrixSampleStateManager::ApplySettings | Только ограничивает m_AFDegree фактическим max_anisotropy из negotiated State. Этот метод не вызывает ReadyDevice или GPU API. | Другие SetState calls и физическое sampling не закрываются этим чтением кода. |
| Unmanaged DecodeBitmap | Guard `unmanaged bitmap SDK decode unavailable` действительно остаётся. Он используется одноаргументным CTexture::LoadFromBitmap. Просмотренные map texture/transition calls используют другие formatted managed overloads; следующий обязательный blocker здесь не установлен. | Это не полный indirect call graph и не реализация оставшегося общего decoder. |
| Gamma pass: zero-count vertex state | Официальный SDK v0.5.0 допускает zero counts: upper-bound checks не запрещают ноль, NON_NULL_ARRAY проверяет pointer только при ненулевом count. Остальные attributes/buffers сбрасываются исходными loops. Удалять эти вызовы ради предполагаемой ошибки не требуется. | Source verification, не исполнение SDK recorder или Switch GPU. |

SDK primary sources:
[gpu_3d_vbo.cpp](https://github.com/devkitPro/deko3d/blob/v0.5.0/source/maxwell/gpu_3d_vbo.cpp),
[dk_private.h](https://github.com/devkitPro/deko3d/blob/v0.5.0/source/dk_private.h).
Read-only snapshots сохранены в `build/matrixgame/e2e560-boundary/sdk-source`.
Их Git blob SHA сверены с GitHub response; SHA-256 и URLs записаны в evidence.
Установленные SDK archives не изменялись. Source tag audit не объявляется
доказательством запуска функций из локального archive.

## Текущая фактическая граница

Все прежние E558 production source inputs и generated original source files
совпали по hashes. Готовая E558 NRO сохранена:

`build/matrixgame/e2e558-gamma/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro`

SHA-256: `2fb8689975480771e690cd764b89af803c6e2d2cd234fe24904c18cb7c86fdd1`.

Предоставленный `D:/logs2.7z` не изменился. Последний реальный run —
`run-1793027984068936-0`, остановка в ReadParams/ApplyGammaRamp до карты.
Нового журнала исполнения E558 в данных этой задачи нет.

По просмотренным участкам нового обязательного source defect не воспроизведено.
Это не доказывает отсутствие других дефектов и не завершает порт. Следующая
неразрешённая зависимость проверки — ручной запуск уже готовой E558 пользователем:
post-gamma output/readback, затем продолжение настоящего Init/PrepareMap и первый
map frame. В условиях запрета взаимодействия агента с устройством эту проверку
нельзя выполнить автономно. Следующий фактический runtime blocker неизвестен.

Ожидаемые дополнительные данные E558: run.jsonl и, при пройденном smoke,
smoke.bgra/gamma-smoke.bgra из одного нового run directory. Отсутствие любого
из них также является наблюдением; создавать вместо него synthetic результат
или снова выдавать прежний CPU PASS за hardware PASS нельзя.

Primary NRO, E556/E558 NRO, оба прежних dist пакета, protected inputs, upstream
pin/clean status и исходный пользовательский архив проверены. Upstream, frozen,
ресурсы и сейвы не редактировались; commit/push и взаимодействия с Switch нет.
Общая цель остаётся незавершённой.
