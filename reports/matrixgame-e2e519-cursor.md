# E2E-5.19: полный оригинальный MatrixCursor и GDI/system backend

24.10.2026. Родитель `6c73d3d`. Локальный master, без push.

MatrixCursorLifecycle.cpp заменён в native-cache полным MatrixCursor.cpp:
SetVisible, Select, CalcUV, Takt, Draw и прежние constructor/Clear. Сохранены
переключение system/software, fallback, display-counter loops, reverse animation,
UV, порядок состояний и оригинальные CInstDraw вызовы. Конструктор и Clear
совпадают с ранее проверенными. После обращения конкретных адаптаций полный TU
совпал с предыдущим prepared original; MatrixCursor.hpp побайтово не изменён.

Новый cursor_backend изолирует настоящие Windows ShowCursor, D3D9::ShowCursor,
CreateIconIndirect и используемые оригинальным Select методы WinBitmap
Clear/Init. Сохранены BITMAPV4HEADER, маски, swap/restore каналов, bottom-up
копирование и GDI DC/bitmap lifetime. GetDC защищён RAII для отказов. WinBitmap
Save/capture этим этапом не переносился. Windows target получает gdi32.

На backend без SDK/GDI эти операции явно Unsupported; готовность software
cursor на Switch из этого compile/link результата не следует.

## Подтверждённая интеграция

- **Host ASan/UBSan/LeakSanitizer:** новые полные cursor/backend CPP compile/link,
  общий bootstrap cleanup/reentry PASS, exit 0. Новых sanitizer exclusions нет.
  Оригинальный cursor constructor без окна по-прежнему отказывает до публикации.
- **ARM64/QEMU:** compile/link и общий bootstrap PASS, exit 0.
  Forced map-owner link: **2 → 1 unresolved**, закрыт SetVisible, новых нет.
  Единственная оставшаяся связь этого probe — Text::Render.
- **devkitA64:** compile/link отдельного ELF PASS. После изменения Windows-only
  части backend выполнены адресная компиляция текущего backend и relink, без
  повторной полной сборки; финальный strong undefined 0 и ELF hash проверены.
  Добавление gdi32 находится в if(WIN32), ARM64 flags/source list не меняет.

Host/ARM64 переиспользуют неизменённые текущие native objects. Старый lifecycle
object удалён из архива перед добавлением полного TU; duplicate symbols нет.

## Настоящий Windows HAL/GDI backend runtime

**PASS, exit 0:** созданы шесть настоящих иконок (три кадра × два входа).
Независимый SDK probe через GetIconInfo/GetDIBits подтвердил размер 4×4,
hotspot (1,1), RGB всех пикселей и ориентацию строк. После каждого входа
восстановлен прежний системный cursor handle; итоговые GDI/USER counts стабильны.
Проверены реальный system display counter, вызовы D3D9 cursor visibility,
Clear/Clear и уничтожение иконок. Проверялся RGBA32 путь, используемый Select.
Это не доказательство видимого hardware cursor или software sprite draw.

Первый cold handle baseline дал +4 GDI handles и остановил тест. Независимый
чистый SDK warmup воспроизвёл тот же +4 **до вызовов адаптера**; затем все шесть
входов имели одинаковые counts и restored=1. Наблюдение отделяет однократную
SDK/GDI инициализацию от роста на каждый вход; первоначальный FAIL сохранён.
Тест не является длительным аудитом всех GDI caches процесса.

## Полный cursor runtime ещё НЕ подтверждён

Подготовлены тест оригинального Select/SetVisible/Takt/Clear и software Draw с
HAL pixel readback. Он **COMPILE PASS, LINK FAIL, NOT EXECUTED**. Подключение
настоящих init_runtime.cpp/MatrixGame.cpp/GameDefaultsNative.cpp вместо fake
globals раскрывает 44 связи игрового owner graph: renderer/map lifecycle,
подготовка карты, progress, интерфейсы, hints/history и другие CGame::Init paths.
Полный список и точные команды сохранены. Многие реализации уже присутствуют
в ARM64 native-cache; этот standalone Windows probe ещё не включает полный
Windows комплект соответствующих объектов. Это не 44 доказанных отсутствующих
реализации движка. Фиктивные globals/maps и force-unresolved link не применялись.

Backend-тест выше не закрывает этот отдельный full-cursor gate. Оригинальные
Select, анимация, texture choice и Draw в сборке присутствуют, но runtime пока
не доказан. Число один относится только к ARM64 constructor/destructor probe,
не к полному CGame::Init.

**CGame::Init NOT CALLED; карта NOT CONSTRUCTED; RunGameLoop и бой NOT RUN.**
Switch/deko3d — **HARDWARE PENDING**. Offscreen rectangle Clear остаётся отдельным
backend blocker. Исторические причины повторяющегося текста, manual-publish
ENOENT и полных зависаний карты/выхода основной игры остаются **OPEN**.

Основная NRO/ELF, оба hardware ZIP, сейвы и ресурсы не менялись: сверены SHA-256
13 доступных protected inputs и 107 аппаратных файлов. G:/pack.zip недоступен.
Новой NRO нет; push не выполнялся.

[Source fidelity, команды, PASS/FAIL, оставшиеся связи и хэши](matrixgame-e2e519-cursor-evidence.json).
