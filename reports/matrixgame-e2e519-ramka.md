# E2E-5.19: Windows integration и оригинальный Ramka

25.10.2026. Предыдущая граница: CGame::Init исполняет ресурсы/config/cache на
host/ARM64, затем отказывает без Window/Device providers. Здесь эта граница
исполнения полного Init ещё не пройдена.

## Полная Windows-сборка

Новый CMake-кандидат original-init-windows собрал весь текущий native engine
с MSVC x86 и настоящими SDK headers. Это не только четыре отдельных TU.
Линковка original-init остановилась на пяти связях. Включённая ниже оригинальная
Ramka закрыла одну: повторная адресная линковка с новым managed object имеет
четыре оставшихся unresolved, без новых связей.

| Блокер | Оригинальный файл |
| --- | --- |
| L3GRun | MatrixLib/3G/3g.cpp |
| CMatrixEffectPath constructor | MatrixGame/src/Effects/MatrixEffectPath.cpp |
| CMatrixMap::CalcVis | MatrixGame/src/MatrixVisiCalc.cpp |
| CMatrixMap::CalcMapGroupVisibility | MatrixGame/src/MatrixVisiCalc.cpp |

Статус Windows full Init: **COMPILE PASS, LINK FAIL, RUNTIME NOT RUN**.
Другие подтверждения Linux/ARM64 Init не заменяют этот результат. В Windows
link также есть LNK4098 из-за сочетания существующих CRT библиотек; целостный
runtime кандидата этим не подтверждён.

## Изменение

CTextureManaged::Ramka добавлен в общий OriginalManagedTexture.cpp и полный
адаптированный Texture.cpp. Исходный алгоритм записи четырёх краёв, pitch,
порядок Lock/Unlock и managed/non-managed ветви сохранены. Изменены только
SDK boundary types и вызовы существующих LockRegion/Release адаптеров.
После обратного применения этих замен весь метод побайтно совпадает с
предыдущим нормализованным исходником. Нового renderer или фиктивного draw нет.

Настоящий D3D9 HAL: оригинальный метод вызван для полной рамки, внутреннего
прямоугольника, одного пикселя, одной строки и одного столбца. Для каждой формы
проверены все 56 ARGB пикселей 8x7 texture, включая неизменённую область. Два
цикла создания/Release/recreate прошли; реальный SDK state lease восстановлен.
Это region lock/readback на настоящей GPU texture, не полноценный бой.

Первый тест ошибочно ожидал отказа отрицательного rectangle; HAL его принял,
и assertion отказа не прошёл. Эта проверка удалена: оригинальный Ramka требует
положительные размеры и границы внутри texture. В игровой метод дополнительная
политика clipping/validation не добавлялась. Вызывать его для произвольного
непроверенного rectangle по этим результатам безопасным не объявляется.

devkitA64: обновлённый OriginalManagedTexture.cpp **COMPILE PASS**. Нового matching
Switch ELF/NRO для Ramka этот этап не выпускает. ASan/UBSan и ARM64/QEMU именно
для нового метода не запускались; старые suites повторно не прогонялись.

## Доказательства и сохранность

[Evidence](matrixgame-e2e519-ramka-evidence.json): исходные/объектные SHA-256,
HAL результаты, SDK compile command, Windows baseline build и оставшиеся связи.
Полные команды Windows-кандидата и HAL находятся в указанных build-путях;
matching hashes включены в evidence. Это адресная перелинковка поверх только что
собранных Windows-библиотек, не успешная полная повторная Windows-сборка.

15 доступных защищённых файлов и все 107 аппаратных файлов снова совпали по
SHA-256. Основная NRO/ELF, оба frozen ZIP, ресурсы и сейвы не изменены. Push нет.
Аппаратные P0 остаются открытыми; факт опасного peer-eviction отдельно описан
в reports/e2e5/hardware-20261023-cache-retained.md и причиной фона не объявляется.
