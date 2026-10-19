# E2E-5.12 SAVE-10: отказ от Windows RSG/v167 на host

В пользовательском каталоге обнаружен настоящий Windows `QuickSave.sav` с UTF-16 заголовком `RSG`, `v167`, 981208 байт, SHA-256 `84509d9ab5411555c0a361d5616c94aae6ef3a5c00fa81782ea64192b5c7bd73`. Для пробы использована только его проверенная SHA-256 копия в игнорируемом `port/switch/build/e2e5-host-game/runs/81fb4beef93e48118da2315198c3d278/`; исходный файл после пробы сохранил ту же сумму.

До исправления загрузчик входил в десериализацию `RSG/v167`, затем отклонял файл по исключению `TGalaxy.IdToStar`. Теперь он проверяет сигнатуру и версию сразу после открытия, пишет `E2E5 LOAD_REJECTED unsupported save format; expected RSG64/v168` и выходит до разбора несовместимого игрового состояния.

Проверка на оптимизированном host-бинарнике SHA-256 `ca4fb2cb3e86aa64defa8bd00ebfe6d0898986edc8e58860adadcdfcda1fc6e4`:

| Сценарий | Локальный отчёт в `port/switch/build/e2e5-host-game/optimized/` | Результат |
| --- | --- | --- |
| Копия Windows `RSG/v167` | `save10-windows-v167-fixed.json` | `LOAD_REJECTED`, без краша/таймаута, возврат в меню, SHA исходника и копии неизменны |
| Собственный `RSG64/v168` | `save10-valid-v168.json` | загружены ход 301, 5700 кредитов, корабль 2, звезда 3, 16 скриптов; следующий ход 302, файл не изменился |

`py_compile`, `git diff --check` и 38 unit tests прошли (5 fixture-dependent tests пропущены). Это HOST-проверка; Switch round-trip и совместимость с Windows-форматом не заявляются.
