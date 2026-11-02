# MatrixGame E610 r4: managed texture residency

Основание: два аппаратных запуска r3 прошли ApplyVideoParams, но поздний Init
остановился с original managed texture decode failed: -1.
[Аппаратное evidence r3](matrixgame-e610-r3-hardware-20261102.md).
Имя ресурса и первичный код в r3 отсутствуют; точная историческая причина
не устанавливается задним числом.

## Исправление

Native Session сохраняла CPU mip data, но удерживала каждую GPU image до
Release/Reset; при заполнении фиксированной арены 64 МБ следующий upload
выбрасывал ResidencyExhausted. Legacy Decode превращал это в OutOfVideoMemory,
а оригинальный адаптированный LoadTextureFromFile возвращал NULL, теряя код.
Следующий Init выдавал общий -1. Этот путь воспроизводится адресным CPU-тестом
ограниченной арены; совпадение точной причины с аппаратным отказом пока не доказано.

Resident теперь при **ResidencyExhausted** выбирает managed GPU-копию с наименьшим
priority, затем с наиболее старым использованием. Выбор ограничен теми, чья
image имеет только собственную ссылку текстуры; default pool/render targets,
locked levels, текущий заменяемый ресурс и GPU images, удерживаемые другими
владельцами, не вытесняются. Записанные/in-flight packets сохраняют точную
версию image. CPU bytes, размеры, mip levels, handles и cache identity сохраняются.
Следующий Resident выполняет настоящий повторный upload из тех же bytes.

Каждая неудачная попытка должна убрать одну существующую eligible image;
без кандидатов исходная ошибка пробрасывается. Общие bad_alloc, ошибки decode,
transfer/queue и unsupported formats не маскируются повторными попытками.
Размер арены не увеличивался. Это ограниченная native priority/LRU policy,
не заявление о полном воспроизведении внутреннего алгоритма D3D9 для всех нагрузок.
Контракт CPU backing и вытеснения managed GPU copies согласован с
[Microsoft: Managing Resources](https://learn.microsoft.com/en-us/windows/win32/direct3d9/managing-resources)
и [Automatic Texture Management](https://learn.microsoft.com/en-us/windows/win32/direct3d9/automatic-texture-management).

В generated managed loader исходный отрицательный код больше не заменяется
NULL/-1: exception содержит имя ресурса UTF-8, число входных байтов, pool и
первичную ошибку. Null guard для альтернативного пути также содержит имя.
GPU pressure journal фиксирует free/largest, параметры запрошенной текстуры
и число managed evictions. В error path снимаются counters до Deinit.

Upstream не изменялся. Generated engine delta к E601 разрешена ровно для двух
файлов: MatrixLib/3G/OriginalManagedTexture.cpp и MatrixLib/3G/Texture.cpp;
все остальные generated hashes обязаны совпадать. Входные assets не менялись.

## Адресная проверка

Тестирует реальный native_texture.cpp и настоящий TextureAsset decode/mips.
Upload callback — явно обозначенная CPU image/arena model, не эмуляция GPU PASS
или полного CGame::Init. Проверяются LRU/priority, pinned image, locked texture,
default pool protection, rollback, восстановление всех mip bytes и освобождение.
Сорок одновременно живых handles из восьми encoded inputs давят на ограниченную
арену; все ресурсы после вытеснения восстанавливаются из исходных CPU bytes.
Восемь входов включают четыре исходных файла robots.pkg (PNG/JPEG/DDS/STRG)
и четыре контрольных DDS. Сохранены исходные SHA входов.

Windows/MSVC x86 CPU reference прошёл: 40 handles, 70 evictions,
4000 allocator operations, 12 formats, 8 decoded inputs. Host ASan/UBSan/LSan,
ARM64/QEMU UBSan и negative control проверяются отдельно; их результаты
фиксируются в итоговом evidence, не заменяются Windows-результатом.

Первые попытки всех четырёх runners сохранили link failure: прежние архивы E521
не предоставляли актуальные PrepareMips/CompressBC1. Runner подключает текущий
texture_mips.cpp; прежние результаты не перезаписываются.
Большие Windows боевые сценарии и полные регрессии не запускались.

Вторая ARM64 попытка с актуальным mip-кодом прошла link, но QEMU завершился
SIGSEGV. Она сохранена в arm64/2. gdb-multiarch локализовал рекурсию
libubsan checkDynamicType → libstdc++ __dynamic_cast → инструментированный
std::type_info::operator== в executable → libubsan. Ошибка воспроизводилась
без ресурсов в первом make_shared<Session>, до texture load. Исправление
probe build — -fvisibility-inlines-hidden; vptr, все остальные UBSan checks
и assertions остаются включены. Новый результат записывается отдельно.
Debugger распакован только в ignored build, SDK/системная установка не менялись.
Попытка arm64/3 показала, что libstdc++ явно помечает этот inline как DEFAULT,
поэтому одного -fvisibility-inlines-hidden недостаточно. Подтверждённое исправление
link map локализует ровно _ZNKSt9type_infoeqERKS_ (type_info::operator==):
инструментированный inline остаётся для собственных вызовов, но не подменяет
C++ runtime внутри libubsan. Повторная линковка тех же объектов (arm64/3/local-result)
прошла все восемь real inputs с полным UBSan, включая vptr. Финальный runner
выполняет отдельную полную компиляцию/линковку с этой картой; попытки 2/3 не затёрты.

## Аппаратная граница

VI owner, SDL binding, origin shader и полнокадровая identity presentation
проверка r3 сохранены. Названия r4 Quick Smoke / Interactive различаются в hbmenu.
Пути DATA/CFG и отдельные logs/output остались прежними.
[Инструкция r4](../docs/matrixgame-e610-managed-r4-readme.ru.md).

Полный Init, настоящие кадры карты и поведение memory pressure на deko3d
требуют нового пользовательского запуска. При новом отказе нужны новые run.jsonl
с именем ресурса и первичным кодом. Campaign/Gov, победа и приёмка всего игрового
цикла остаются вне standalone-теста. Пустые изображения и обход Init не применяются.

## Итоговая поставка

**SOFTWARE PASS / R4 HARDWARE PENDING.** Windows/2, host/2 и arm64/4 прошли:
40 handles, 70 evictions, полное восстановление CPU mip bytes, защита pinned/
locked/default ресурсов, 4000 allocator operations, 12 formats, 8 decoded inputs.
Host использует ASan/UBSan/LSan; ARM64/QEMU — полный UBSan/vptr с исправленной
RTTI-symbol binding. Старый native_texture.cpp из r3 в baseline/2 воспроизводимо
завершился ResidencyExhausted (expected negative control). Исторические
link failures и ARM64 SIGSEGV из попыток 2/3 сохранены, не объявлены PASS.
Оригинальные CPU core/codec archives не полностью инструментированы;
это адресное доказательство изменённого resource кода и модели GPU ownership.

Финальная SDK-сборка: build/matrixgame/e610-managed-r4/3.
Source SHA: `d5af0be47d5d48b8d8f392c403272cb6f9486026`.
Build ID: `d5af0be47d5d-65a4fae0c985699dfa07`.
Compile/link, отсутствие неразрешённых strong symbols, NRO0/ASET/icon/NACP,
встроенные шрифты и соответствие ELF/NRO build ID — PASS.
Сверены 685 source hashes, ровно два разрешённых generated engine изменения,
164 прежних файла dist/logs/saves и артефакты предыдущей SDK-попытки.
Исходные игровые/frozen inputs и основные NRO также прошли builder hash checks.

В обеих ELF сохранены Session::EvictManaged, исходная decode/upload диагностика,
pressure journal, VI mode query и orientation checker. Старый общий строковый
маркер original managed texture decode отсутствует. Определения SDK default
window/startup отсутствуют, SDL привязан к настоящему окну владельца.

Финальный ZIP: `dist/matrixgame-e610-switch-test-r4-managed-final.zip`, 10054684 байт.
SHA256: `6cd564d1706a82b6940313efdaaa608ed240d5b57e5591059050fe2cf8d3f945`.

| NRO | SHA256 | ELF build ID |
| --- | --- | --- |
| Quick Smoke | `2ed35cd45cda62b1fb3e6ce1e9f5378436e226e5d14d7ab5209f3b8be41f3084` | `608668b5c018541199cae7a94006e6cf44bde0df` |
| Interactive | `178ebcef6b4c41114a647f668327cac4d93e2ccdc721accaa7fe86d28175940f` | `b8a45aa51d34b5d8b29c4c71a19f034971d04c1d` |

ZIP содержит ровно шесть файлов (две NRO, README.ru.md, manifest.json,
SHA256SUMS.txt, build-info.json), CRC и байты сверены с папкой пакета.
Игровые ресурсы, сохранения и debugger в поставку не включены.
Промежуточный ZIP r4 и прежние r2/r3 пакеты оставлены неизменными; пользователю
предназначен только указанный **r4-managed-final.zip**.
[Машиночитаемое evidence](matrixgame-e610-managed-r4-evidence.json).

Копировать только две NRO в `/switch/matrixgame-e610-r4-test/`. Ресурсы оставить
на прежних путях. Запустить MatrixGame r4 Quick Smoke, затем MatrixGame r4
Interactive (Minus+Plus — выход). До пользовательских результатов r4 не заявляем
полный Init/кадры карты или устранение точного аппаратного отказа r3.
