# MatrixGame — Text::Render / DeviceIdentity, проверка текущей интеграции

Text::Render / DeviceIdentity программно закрыты через оригинальный Font.2Ranger.
Реализация уже находилась в рабочем дереве; в этом проходе она проверена заново
на текущих production sources, без замены шрифта и изменения ресурсов.

Путь: оригинальные Text/Render.cpp → GetFont / Font.cpp / Parser.cpp →
text_backend → native_text / native_text_backend → native texture session upload →
оригинальный bitmap copy / UI. Font.2Ranger использует исходный Rangers.ttf;
Verdana 12/13/14 соответствуют остальным исходным BuildFonts-запросам.
Coverage, метрики и kerning получены из оригинальных шрифтов. Данные font0.sfnt
побайтно совпадают с исходным Rangers.ttf. Пустая текстура и fallback-шрифт
не используются. Отсутствующие глифы/сложный shaping явно Unsupported.

DeviceIdentity — стабильный ключ живой native texture session для исходного
кэша шрифтов. Владельцы font/surface/target зарегистрированы; COM-объекты
не имитируются. EndScene публикует новый image и CPU bytes только после
успешного upload. При исключении сохраняются старые данные; возможен retry.
Managed invalidation сохраняет CPU data, смена session вызывает пересоздание
исходного font cache. Это адресная CPU растеризация с native upload, поскольку
оригинальный Text::Render сам считывает промежуточный результат в bitmap.

| Категория | Подтверждение |
| --- | --- |
| HOST | Новый ASan/UBSan прогон: 17940 glyph/layout goldens; оригинальный Text::Render «Меню» до bitmap; parser, ownership/cleanup, upload failure/retry, cache rebind, managed reupload, write-only factory outputs. Upload в тесте — CPU fault model. |
| ARM64/QEMU | Новый прогон тех же production backend и original Text TUs: PASS. Это не Switch GPU. |
| WINDOWS HAL | Сохранённый E2E-5.24 oracle: 17940 точных BGRA/rect/return сравнений. Проверены неизменность rasterizer и glyph assets; большой oracle повторно не запускался. |
| COMPILE/LINK | Новая полная E2E-5.38 devkitA64 ELF, 183 собственных link inputs, 0 strong undefined; наличие original Text::Render, DeviceIdentity, native Draw/OriginalAsset проверено по symbols. |
| SWITCH HARDWARE | NOT RUN / HARDWARE_PENDING. Native Init, PrepareMap, текст на GPU и первый Present не исполнялись на консоли. |

В новом audit сохранены SHA исходников, assets, ELF и всех link inputs;
проверены 15 protected entries, 107 аппаратных входов, robots.pkg, CFG,
frozen GI source, основная/root NRO и чистый upstream pin.
Ресурсы и сейвы не редактировались. Commit/push и новая NRO отсутствуют.

[Машиночитаемое evidence](matrixgame-text-current-evidence.json).
[Первоначальная реализация и oracle](matrixgame-e2e524-text.md).
[Последовательность Init и cleanup](../docs/matrixgame-init-sequence.md).

Следующий аппаратный блокер Init не наблюдался: Switch не запускался.
Нельзя объявлять карту загруженной по ELF или Windows-эталону.
Для первой native карты требуется реально исполнить platform/device creation,
original Init → RobotPreload → stor.Load → PrepareMap → Draw → Present.
Старые raw HAL reset-retry FAIL, P0, GI repeating-text causality,
manual-publish ENOENT и Switch hangs этим text PASS не закрываются.
