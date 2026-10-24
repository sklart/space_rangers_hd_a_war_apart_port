# E2E-5.19: полные оригинальные владельцы интерфейса

24.10.2026. Родитель `3e179ee`. Локальный master, без push.

В native-cache включены полные CInterface.cpp (CInterface/CIFaceList),
CIFaceMenu.cpp, CIFaceStatic.cpp, CIFaceImage.cpp, CAnimation.cpp, CCounter.cpp
и CHistory.cpp. Сохранены обработчики, динамические панели, меню конструктора,
текстовые состояния, создание/удаление элементов, анимация и история конфигураций.
Методы, открывающие новые зависимости, не удалялись ради линковки.

## Что изменилось

`tools/matrixgame_interface_owners_overrides.py` добавляет доступ к настоящим
типам Texture и legacy texture adapter. TRUE/FALSE заменены bool literals,
D3DLOCKED_RECT — существующим LockedRect, D3D_OK — нулевым кодом успеха,
D3DFMT_A8R8G8B8 — `TexturePixelFormat(21)`. Именованные текстуры используют
существующий GetManaged с настоящим cache ownership.

Алгоритмы и время жизни ресурсов в семи модулях не менялись. Полные effective
TU сопоставлены с camera-sdk/source после обращения конкретных адаптаций и
канонизации bool/кода успеха: остаточных различий нет. Все engine headers
совпали; существующие objects можно переиспользовать без изменения ABI.
Статики, ранее перенесённые в GameStaticResources.cpp, остаются у одного владельца.

Первая компиляция обнаружила ошибку адаптера: CreateLock принимает
TexturePixelFormat, а не platform3g::PixelFormat. Исправлен только тип enum,
исходное значение 21 и аргументы создания текстуры сохранены. Первые журналы
с ошибкой сохранены; последующий кандидат успешно скомпилирован.

## Результат интеграции

**Map-owner link probe: 17 → 7 unresolved.** Закрыты 16 прежних связей
CIFaceList/CInterface/CIFaceMenu; раскрыты шесть следующих зависимостей.
Точные before/after lists и link-команды находятся в evidence JSON.

Оставшиеся блокеры:

| Файл оригинального движка | Отсутствующие связи и причина |
| --- | --- |
| MatrixGame/src/MatrixInstantDraw.cpp | CInstDraw::BeginDraw, оба AddVerts, ActualDraw: полный пакетный renderer ещё не подключён; нужны реальные staging/device adapters |
| MatrixLib/3G/Texture.cpp | CTextureManaged::GetPixelColor: не включён в выбранные managed methods; оригинал запрашивает LockRect для одного пикселя, существующий adapter принимает только полный уровень |
| MatrixGame/src/Text/Render.cpp | Text::Render: оригинальный текстовый layout/render использует Text::Font, D3DX и GPU readback; полный путь ещё не перенесён |
| MatrixGame/src/MatrixCursor.cpp | CMatrixCursor::SetVisible: полноценные system/software cursor operations ещё не интегрированы |

Это точная граница зависимостей конструктора/деструктора карты. Остальные
методы карты и полный CGame::Init могут открыть дополнительные зависимости.
Число семь не является количеством всех оставшихся задач порта.

## Подтверждение и ограничения

- **Host:** семь полных CPP compile/link PASS; общий bootstrap под ASan/UBSan
  с LeakSanitizer exit 0, две сессии. Новых sanitizer exclusions нет.
- **ARM64/QEMU:** семь полных CPP compile/link и общий bootstrap PASS,
  exit 0. Map-owner forced link остаётся FAIL с указанными семью методами.
- **devkitA64:** отдельный bootstrap ELF compile/link PASS, strong undefined 0
  в этом executable. Неиспользуемые sections удалены linker GC; это не успешная
  линковка полной карты. Новая NRO не создавалась.

Host/ARM64 используют неизменённые объекты актуального camera-кандидата и
новые семь CPP. Общий runtime подтверждает сохранность уже работающего
bootstrap, **новые UI lifecycle/input/render paths NOT EXECUTED**.
Повторные изолированные CPU-прототипы не создавались. Для следующего шага
необходимо подключить оригинальный CInstDraw и закрыть настоящий texture/text
backend, после чего запускать полноценное владение и повторный вход UI.

**CGame::Init NOT CALLED; карта NOT CONSTRUCTED; RunGameLoop и бой NOT RUN.**
Switch/deko3d — **HARDWARE PENDING**. Прямоугольный Clear из предыдущего этапа
также остаётся неподдерживаемым.

13 доступных protected inputs, frozen main NRO/ELF, оба hardware ZIP и
107 аппаратных файлов совпадают по SHA-256. G:/pack.zip недоступен для
повторной проверки. Сейвы, игровые ресурсы и настройки консоли не менялись.
Push не выполнялся.

Основные аппаратные причины повторяющегося текста, двух исторических
manual-publish ENOENT и полных зависаний карты/выхода остаются **OPEN**.

[Команды, source fidelity, результаты и контрольные суммы](matrixgame-e2e519-interface-evidence.json).
