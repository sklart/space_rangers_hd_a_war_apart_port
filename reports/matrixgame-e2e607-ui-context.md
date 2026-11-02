# E607: настоящий UI-контекст и владение ресурсами

`RangersUiContext` читает оригинальный CFG.TXT без записи, создаёт оригинальные
TCCInterface, RGB565-буферы, таблицу смешивания и курсоры, подключает ML/Data/ZPos
из Main.dat и глубины интерфейса. Время жизни: Host → UiContext → Script →
GlobalUi → Campaign; освобождение в обратном порядке. Этот компонент заменяет
ручную подготовку обязательных UI-зависимостей, но не весь Windows startup.

В `InitializeSoftwareRenderer` исправлена очистка при исключении до публикации
флага готовности. Проверка вызывает настоящий отказ malloc/realloc при позднем
выделении PendingPoints после создания всех трёх графических буферов. После
ошибки буферы освобождены, принадлежащие контексту глобальные значения восстановлены.
Общий RNG оригинального TCCInterface не перематывается.

Проверки и неизменяемые результаты:

| Проверка | Результат | result.json относительно корня |
| --- | --- | --- |
| Host ASan/UBSan/LSan | PASS, два цикла UI/script и отказ выделения | build/matrixgame/e2e607-ui-context/host/2/result.json |
| ARM64/QEMU | PASS, два цикла и отказ выделения | build/matrixgame/e2e607-ui-context/arm64/1/result.json |
| Windows ASan | PASS, два цикла; частичная инструментализация | build/matrixgame/e2e607-ui-context/windows/2/result.json |
| devkitA64 | Compile/link PASS, retained UI/caller/campaign ELF, ноль strong undefined | build/matrixgame/e2e607-ui-context/sdk/1/result.json |

У этих четырёх результатов сохранены as-built-sources и source-snapshot.json.
Первая host-попытка сохранена как INCOMPLETE: перехват только realloc не вызвал
нужный отказ. Финальная проверка перехватывает malloc и realloc, учитывает размер
оригинального Integer и требует фактического срабатывания. Это исправление
инъекции теста, а не дополнительный обнаруженный сбой runtime.

Машинный аудит: [matrixgame-e2e607-ui-context-evidence.json](matrixgame-e2e607-ui-context-evidence.json).
Приватные runtime-каталоги host/ARM64/Windows пусты. Защищённые игровые входы,
frozen-комплекты, upstream pins, прежние NRO и E604 ZIP проверены без изменений.
SDK-архив содержит 378 исходных объектов; относительно E606 заменён только
renderer_platform.o. Новая NRO на этом этапе не создавалась.
Retained ELF: 57 367 608 байт, SHA-256
`74ead7ed29a33a7e3045dbefac519030f626681ee462c07afb5c4756f6088410`.

Для повторения отдельных проверок (создаются новые attempt-каталоги):

```text
python3 tools/matrixgame_ui_context_cpu.py host
python3 tools/matrixgame_ui_context_cpu.py arm64
python tools/matrixgame_ui_context_windows.py
python tools/matrixgame_ui_context_sdk.py
```

Первые две команды исполняются в WSL из корня проекта. Готовые большие проверки
заново не запускались: здесь проверены их результаты и снимки исходников.

Полный бой с новым UiContext пока не проверен. Предыдущая E606 Windows-проверка
доказывает natural defeat и оригинальный Gov history/reward/return с прежней
подготовкой UI. Полная native Gov entry, управление, deko GPU runtime и
аппаратная приёмка остаются открытыми; естественная победа не подтверждена.
Следующий этап — совместная проверка нового контекста с кампанией и боем,
затем общая native entry и отдельная воспроизводимая NRO.
