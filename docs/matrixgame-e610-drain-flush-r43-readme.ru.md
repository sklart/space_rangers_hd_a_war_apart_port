# MatrixGame r43. A/B отправки GPU-батчей

**SOFTWARE PASS / HARDWARE PENDING.** Пакет содержит одну Interactive NRO.
Режимы различаются одним вызовом в drain по лимиту draw на видимой карте.

| Аргумент | Действие |
| --- | --- |
| `--drain-flush=explicit` | A. Путь r42 с отдельным `queue.flush()` перед `waitIdle()`. |
| `--drain-flush=sdk` | B. Отправка очереди через `waitIdle()` с его fence и внутренним flush. |

Оба аргумента по умолчанию выбирают пакетный журнал r42. При запуске
без аргументов остаётся прежний путь с отдельным GPU flush и синхронным
журналом. Вариант B сохраняет лимит в 64 draw и порядок команд.
Память draw, командные страницы и ссылки на поверхности освобождаются
после успешного ожидания. Принудительные drain и Present используют
прежний путь.

## Два запуска через NetLoader

Откройте hbmenu в Application Mode и нажмите Y. Отправьте A. По окончании
A снова откройте NetLoader и отправьте B. Для обоих запусков используйте
один файл, одинаковый режим консоли и одинаковые настройки. Оставьте
карту без ввода до штатного 120-секундного лимита.

```powershell
$nxlink = 'C:/devkitPro/tools/bin/nxlink.exe'
$nro = 'D:/путь/к/распакованному-r43/matrixgame-r43-gpu-flush-ab.nro'
& $nxlink -a 192.168.2.225 -p matrixgame-r43-gpu-flush-ab.nro $nro --args '--drain-flush=explicit'
# После завершения A и повторного открытия NetLoader
& $nxlink -a 192.168.2.225 -p matrixgame-r43-gpu-flush-ab.nro $nro --args '--drain-flush=sdk'
```

NRO использует те же каталоги данных, что и r42. В архив входят NRO,
инструкция, манифест и SHA256. Журналы и снимки создаются в двух новых
каталогах `/switch/matrixgame-diag/logs/<run-id>/`.

Для сравнения нужны `run.jsonl` и доступные BGRA из обоих запусков.
Проверьте `metadata/draw_drain_flush` и `metadata/journal_mode`.
Сравниваются стенное время кадров 60–240, `profile.batch_wait`,
`profile.batch_gpu_wait` и `batch_explicit_flush_calls`.
Последний счётчик учитывает вызовы приложения. Внутренние отправки
и автофлаши deko3d могут выполняться в обеих версиях.
