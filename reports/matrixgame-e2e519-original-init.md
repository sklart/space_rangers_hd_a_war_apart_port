# E2E-5.19: выполнение оригинального CGame::Init

25.10.2026. Локальный кандидат поверх `be73fdb`; Switch недоступна.

Оригинальный `CGame::Init` действительно вызывается на host и ARM64/QEMU.
На настоящем DATA/robots.pkg (232546988 байт) и копии оригинального CFG он
выполняет static_init, HNew для игрового heap, загрузку конфигурации и CacheInit.
Точная следующая граница: `legacy DLL entry requires registered borrowed Window/Device providers`.
Успешный возврат Init, загрузка мира, RunGameLoop, CFormMatrixGame и бой **NOT RUN**.
Switch GPU runtime **HARDWARE PENDING**.

## Владение

Старый CPU Begin уже создаёт heap/cache. Вызов исходного Init поверх него повторно
сбрасывал бы statics и заменял владельцев. Новый отдельный режим BeginSession
оставляет создание этих объектов самому оригинальному Init. HNew/HDelete не
заменяются std::unique_ptr/delete. В Init добавлены проверка свежего владельца
и уведомление после static_init; в Deinit — проверка владельца. После удаления
трёх вставок и include/alias весь effective MatrixGame.cpp идентичен предыдущему.
Поля NativeInitRuntime и effective headers прежние: ABI повторно используемых
объектов сохранён.

При исключении стековый CLoadProgress уже разрушен: глобальный указатель
сбрасывается перед оригинальным Deinit, затем освобождаются native bindings.
Прямые Init/Deinit поверх CPU-сеанса отвергаются до изменения его heap/cache.

## Реальные результаты

| Среда | Результат |
| --- | --- |
| Host ASan/UBSan, detect_leaks=1 | Оригинальные Init/Deinit, очистка, два входа на том же owner для каждого сценария PASS |
| ARM64/QEMU | Те же реальные сценарии и повторный вход PASS |
| devkitA64/libnx | Отдельный original-game-init.elf compile/link PASS, strong undefined = 0; runtime NOT RUN |
| Windows MSVC x86 | Четыре изменённые translation units COMPILE PASS; полного runtime кандидата нет |

Отсутствующий пакет вызывает исходный CException из file_native.cpp:223. Настоящий
пакет доходит до отказа legacy DLL providers. Реальные флаги statics/heap/data/cache
перед очисткой: `1/1/0/0` и `1/1/1/1`. После оригинального Deinit globals/cache/packages
пусты, дополнительный Deinit безопасен. Старый CPU режим отдельно проверен на
сохранность указателей при отказе прямых Init/Deinit. Новых sanitizer exclusions нет.

Доказана очистка именно двух исполненных префиксов. Поздние ошибки GPU/map не
проверены. Ветка передачи пары borrowed Window/Device только компилировалась.
Original Deinit потенциально бросает, а NativeInitRuntime::Deinit остаётся noexcept:
неожиданное исключение очистки фатально. Универсальный unwind GPU Init не доказан.

Собраны четыре изменённые единицы и отдельный driver; остальные библиотеки взяты
из проверенной базы output-paths. Новый CMake target matrixgame-original-init
добавлен; результаты получены адресными командами evidence, а не повторной полной
сборкой этого target. Основная игра не пересобиралась.

## Артефакты и ограничения

[Evidence](matrixgame-e2e519-original-init-evidence.json) содержит команды,
stdout/stderr, SHA-256 исходников, объектов, библиотек и matching ELF.
Результаты находятся в build/matrixgame/e2e519/original-init-{host,arm64,sdk,msvc}.
Новая NRO не создавалась.

Проверена неизменность 15 доступных защищённых файлов (основная NRO/ELF, два
frozen ZIP, сохранения и другие входы), всех 107 аппаратных файлов, настоящего
robots.pkg и исходного CFG. Пакет читался через symlink, CFG — из отдельной копии.
G:/pack.zip недоступен. Push не выполнялся.

Следующий блокер — реальные Window/Device providers для L3GInitAsDLL, затем
следующие фактические операции Init. Основные P0 (источник фонового растра,
исторические manual-publish ENOENT, аппаратные зависания) здесь не закрываются.
