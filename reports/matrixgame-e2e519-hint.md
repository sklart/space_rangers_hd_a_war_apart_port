# E2E-5.19 — полный MatrixHint и владение текстурой при отказе

24.10.2026, продолжение `11bcc08`, локальный master, без push.

## Найденный дефект и исправление

Подключён полный оригинальный `Interface/MatrixHint.cpp`. Исходные bitmap
layout/merge/copy алгоритмы, текстовые overloads, sound и draw methods сохранены.
Фабрика CTextureManaged явно направлена в genuine CreateManaged; portable
CacheTexture не приводится к чужому динамическому типу. D3D calls используют
существующие typed device contracts. Header и layout CMatrixHint не менялись.

Общий host bootstrap исполнил настоящий Build(-1, ..., SHintElement*) на
контролируемой 2×2 RGBA bitmap. После настоящего Unsupported на GPU-границе
исходный Build оставил созданную текстуру без владельца: registry **2→3**.
Проверка неизменности registry завершилась assertion failure. Это не
предположение по исходнику и не фиктивный GPU success; failing log сохранён
в `build/matrixgame/e2e519/hint-v1-integration/host/runtime.log`.

Причина: original caller-owned texture создаётся до CreateLock; при исключении
до создания CMatrixHint нет объекта, destructor которого мог бы её освободить.
Текстура не публикуется в parent cache, поэтому CacheClear ей не владеет.

Исправление оставляет временного владельца у настоящей текстуры до завершения
оригинального constructor CMatrixHint. Затем владельцем становится подсказка;
второй guard защищает её до завершения m_CopyPos assignment. Успешный путь
возвращает тот же original object; порядок композиции и игровые результаты
не менялись. Это исправление MatrixGame; причиной исторического raster-дефекта
основной игры оно не объявляется.

## Доказательства и граница

Повторный общий host ASan/UBSan/leak-check и ARM64/QEMU прошли: после каждого GPU refusal
registry возвращается **2→2**, исходная bitmap неизменна, ClearAll и повторный
Build выполняются. В каждом из двух сеансов — два отказа/повтора. Новых
sanitizer exclusions нет; Hint TU сохраняет ASan, UBSan и vptr. Прежние Console
globals и семь world/effect vptr ограничений не расширялись.

Успешный GPU Build, actual CMatrixHint constructor/destructor runtime,
SoundIn/SoundOut runtime, PreloadBitmaps на ресурсах мира, текстовые overloads
и отрисовка **NOT EXECUTED**. Проверен именно отказ после реальной CPU
композиции с освобождением непереданной текстуры. Исключения после GPU lock,
OOM и UI text callback lifetime этим тестом не покрыты.

MSVC: полный MatrixHint.cpp COMPILE PASS, Windows runtime не запускался.
devkitA64: common-prefix COMPILE/LINK PASS, strong undefined=0. ELF SHA-256:
`3b1b8e17c9a78e93f676bb6457ea58a747e4ae2fc9325107aadfd02290a0639c`.
Switch/deko3d HARDWARE PENDING; новая NRO не создавалась.

Fresh CMake configure/generate PASS; обе compile recipes Hint сохраняют
function/data sections и vptr, полная повторная сборка не выполнялась.
Источники и результаты перечислены в
[evidence](matrixgame-e2e519-hint-evidence.json). Оба runtime завершились exit0,
по два сеанса; это отделено от SDK compile/link.

## Оставшаяся интеграция

Оригинальные destructor и SoundOut подключены вместе с полным TU. Это закрывает
их как реальные зависимости, не заменяет пустыми функциями. Принудительная
линковка полного владельца карты теперь даёт **FAIL8 вместо FAIL10**: остались
CIFaceList::HideHintButtons, CMatrixMapObject::ClearTextures и destructor,
destructors CMatrixBuilding/CMatrixCannon/CMatrixFlyer/CMatrixRobotAI/CMatrixSideUnit.
Полный владелец карты всё ещё не считается запущенным; CGame::Init, RunGameLoop, CFormMatrixGame,
мир и планетарный бой NOT EXECUTED.

Основные raster/исторические manual-publish ENOENT/freeze причины остаются
OPEN. Основная NRO, frozen hardware kits, пользовательские saves/resources и
настройки Switch этим изменением не затрагивались. Снова совпали13 доступных
protected hashes,107 файлов аппаратного прогона, frozen NRO/ELF и оба ZIP.
G:/pack.zip недоступен;14/14 не заявляется. Все245 engine files совпали в двух
views,66 core outputs в трёх views. Изменён только MatrixHint.cpp; layout/header
не менялись. Evidence собран до коммита, build_dirty=true, repo helper/test
hashes записаны отдельно.
