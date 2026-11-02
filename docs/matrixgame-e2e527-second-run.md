# MatrixGame: второй диагностический запуск после исправления window attachment

Эта сборка исправляет подтверждённый первым журналом отказ
`native applet window not attached`. Подготовлена для **второго** ручного запуска.
Кандидат на Switch ещё не запускался: HARDWARE_PENDING.

## На SD

Скопируйте только `matrixgame-e2e527-diag-run2.nro` в
`sd:/switch/matrixgame-diag/matrixgame-e2e527-diag-run2.nro`.
Запускайте именно файл с суффиксом `run2`; старый `matrixgame-e2e527-diag.nro`
содержит прежнюю ошибку. Основную NRO Space Rangers HD не заменять.
Точный SHA-256 новой NRO указан в manifest.json и SHA256SUMS.txt рядом с файлом.

Уже скопированные для первого запуска три файла остаются на прежних местах:

| Файл личной копии | Путь на SD |
| --- | --- |
| DATA/robots.pkg | `sd:/switch/matrixgame-diag/game/DATA/robots.pkg` |
| CFG/robots/data.txt | `sd:/switch/matrixgame-diag/game/CFG/robots/data.txt` |
| CFG/robots/iface.txt | `sd:/switch/matrixgame-diag/game/CFG/robots/iface.txt` |

Их SHA-256/размеры/исходные пути перечислены в resources.json; ресурсы в пакет
не включены. Если файлы уже совпадают, повторно копировать их не нужно.
`robots.dat` не добавляйте. Шрифты, shaders, NACP и icon встроены в NRO;
остальные файлы комплекта нужны для проверки на компьютере, а не запуска.

## Запуск и ожидаемый результат

Откройте hbmenu в application mode тем же способом, что при первом запуске:
первый журнал уже подтвердил корректный applet type. При стандартной настройке
Atmosphère это удержание R при запуске установленной игры; при иной конфигурации
используйте назначенную там кнопку.
[Официальная настройка override](https://github.com/Atmosphere-NX/Atmosphere/blob/master/config_templates/override_config.ini).

В hbmenu выберите **MatrixGame diagnostic run2** — файл
`matrixgame-e2e527-diag-run2.nro`. Отпустите кнопки.
Ожидается smoke с красным прямоугольником и оригинальным debug font, затем
original Init, original Draw, четыре кадра карты и автоматический cleanup/выход.
Smoke — диагностический кадр, не кадр MatrixGame. Карта пока не подтверждена
аппаратно; при следующем реальном отказе программа остановится и запишет его.

B или + запрашивают безопасный выход. Во время синхронного Init это может
произойти только после возврата вызова. Общий deadline 120 секунд проверяется
на управляемых границах, но не прерывает блокирующий вызов GPU/SDK.
При обычной ошибке программа попытается показать FAILED примерно на две секунды
и выполнит cleanup; при SDK fatal штатный fatal handler может завершить процесс
без C++ unwind. Если зависло, зафиксируйте время/экран и завершите приложение
через HOME, если доступно. Не извлекайте SD из работающего приложения.

## Что прислать

После завершения скопируйте **весь новый** каталог
`sd:/switch/matrixgame-diag/logs/run-…/` и передайте архивом.
Первый `run-1793012722082021-0` сохраните отдельно; он относится к старой сборке.

Новый каталог содержит ранний run.jsonl и, если соответствующие операции
достигнуты, smoke.bgra, original-first.bgra и output/ с кэшем миникарты.
Даже неполный журнал важен. При отсутствии каталога сообщите видимый экран.
До анализа результатов не назначается третий запуск.

## Исправление и доказательства

Ранние smoke/status теперь имеют собственный настоящий native attachment.
Перед CGame::Init он освобождается; оригинальный платформенный вход подключает
свой attachment сам. После отказа Init error UI подключается лишь после Deinit.
Строгий запрет двойного Attach сохранён. Исходный Init/Draw не заменены.

HOST ASan/UBSan и ARM64/QEMU проверяют реальный Lifecycle и production
InitAsNative/Deinit с явной IPC-моделью: исходный guard, exclusive ownership,
обычную передачу, smoke/Init failures, повторный вход, отмену и rollback.
Это не аппаратная отрисовка smoke и не полный CGame::Init на host.
SDK compile/link и отсутствие unresolved strong symbols проверяются отдельно.
Полная Windows HAL регрессия не повторяется: общий Init/Draw/resources контракт
не менялся. Реальный GPU smoke/карта/Present кандидата требуют этого запуска.

Воспроизводимые команды и хеши входов — в manifest.json/build-inputs.json.
Из текущего подготовленного workspace:

```powershell
python tools/matrixgame_handoff_checks.py host
python tools/matrixgame_handoff_checks.py arm64
python tools/matrixgame_actual_draw_checks.py sdk --output build/matrixgame/e2e546-window-handoff --diagnostic-source build/matrixgame/e2e543-journal/source
python tools/matrixgame_diagnostic_package.py --base build/matrixgame/e2e546-window-handoff --output dist/matrixgame-e2e527-diag-run2 --name matrixgame-e2e527-diag-run2.nro --title "MatrixGame diagnostic run2" --audit tools/matrixgame_handoff_audit.py --readme docs/matrixgame-e2e527-second-run.md
```

Упаковщик отказывается перезаписывать существующую NRO. Это сборка из текущего
workspace с оригинальными assets; чистый master сам по себе их не содержит.
Commit/push, замены основной NRO, frozen, ресурсов и сейвов не выполняются.
