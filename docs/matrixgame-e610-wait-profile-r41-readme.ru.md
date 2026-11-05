# MatrixGame E610 r41 — два замера на Switch

**MEASUREMENT ONLY / HARDWARE PENDING.** r41 разбивает ожидания GPU по
причинам и измеряет стадии записи журнала. Порядок draw, синхронизация
очереди, лимиты памяти и синхронная запись JSONL сохранены. Для отката
достаточно снова запустить соответствующую диагностическую NRO r40.

## Установка и NetLoader

Для запуска с microSD положите две NRO в отдельную папку:

```text
/switch/matrixgame-e610-r41-test/matrixgame-e610-quick-smoke.nro
/switch/matrixgame-e610-r41-test/matrixgame-e610-interactive.nro
```

В hbmenu используйте Application Mode с полной памятью приложения. При
стандартном override удерживайте R при запуске установленного приложения.
Названия в меню — **MatrixGame r41 Quick Smoke** и **MatrixGame r41 Interactive**.

Через Wi-Fi можно передать и сразу запустить NRO через NetLoader. ПК и Switch
должны быть в одной доступной друг другу сети. В hbmenu нажмите Y, запишите
показанный IPv4 и выполните первую команду. После Quick снова откройте
NetLoader кнопкой Y и выполните вторую команду. Укажите адрес и распакованную
папку в первых двух переменных. Для NetLoader отдельное копирование NRO
вручную не требуется.

```powershell
$switchIp = '192.168.1.___'
$r41 = 'D:/путь/к/распакованному-r41'
$nxlink = 'C:/devkitPro/tools/bin/nxlink.exe'
& $nxlink -a $switchIp -p matrixgame-e610-r41-quick-smoke.nro (Join-Path $r41 'matrixgame-e610-quick-smoke.nro')
& $nxlink -a $switchIp -p matrixgame-e610-r41-interactive.nro (Join-Path $r41 'matrixgame-e610-interactive.nro')
```

Обе NRO читают уже подготовленные для r40 файлы:

- `/switch/space-rangers-hd-a-war-apart/game/` содержит `DATA/`, `CFG/`,
  `INSTALL.TXT` и `INSTALL_RUSSIAN.TXT` из одной локальной копии игры.
- `/switch/matrixgame-diag/game/` содержит `DATA/robots.pkg`,
  `CFG/robots/data.txt` и `CFG/robots/iface.txt`. Первый берётся из
  локального `windows/Space Rangers HD A War Apart/DATA/`, два других из
  `upstream/matrixgame/MatrixGame/CFG/robots/`. Если r40 работал, используйте
  файлы, уже находящиеся на microSD. Полный DATA заново копировать не нужно.

Внешние игровые файлы перечислены в манифесте. Шрифты и шейдеры встроены в NRO.

## Два запуска

1. **Quick Smoke.** Отпустите кнопки и дождитесь native/deko3d smoke,
   оригинального Init, четырёх видимых кадров карты и завершения с кодом 0.
   Зафиксируйте ориентацию, артефакты, ранний FAIL или зависание. B/Plus
   прерывают Quick.
2. **Interactive.** Дайте карте работать без ввода в том же режиме, что и
   r40, до 120-секундного лимита после Init. Это сохраняет условия для
   сравнения кадров 60–240. Штатные отметки — `session_end_reason=time_limit`,
   `CONTROLLED_STOP`, `process_exit=0` и `journal_close`. Если потребуется
   выйти досрочно, нажмите Minus+Plus и отметьте момент выхода. При сбое
   сохраните журнал и фото экрана.

| Кнопки Interactive | Действие |
| --- | --- |
| Правый стик, A/B | Указатель, левый/правый клик |
| D-pad, L/R | Прокрутка и поворот камеры |
| X, левый стик | Вход/выход робота и движение |
| ZR/ZL, Y | Огонь/Shift, отмена приказа |
| Plus, Minus+Plus | Меню, выход из диагностики |

Эти сценарии проверяют standalone Init и карту. Campaign/Gov, возврат в
Galaxy и победа остаются за пределами сценариев.

## Что прислать

Для каждого запуска сохраните `run.jsonl` и имеющиеся `*.bgra` из двух
новых папок `/switch/matrixgame-diag/logs/<run-id>/`. Снимки называются
`smoke.bgra`, `orientation-smoke.bgra`, `gamma-smoke.bgra` и
`original-first.bgra`. Прочий вывод приложения идёт в `output/` и
`rangers-user/` внутри папки запуска. При сбое можно приложить существующий
crash report и фото. Скопируйте журналы и снимки на ПК с microSD после
выключения консоли. NetLoader служит для отправки NRO, FTP для тестов не нужен.

| Заполнить | Quick | Interactive |
| --- | --- | --- |
| Портативный режим / док | ___ | ___ |
| Частоты и отличия от настроек r40 | ___ | ___ |
| Init, изображение, ориентация | ___ | ___ |
| Кадры, артефакты, зависания | ___ | ___ |
| Кнопки, если нажимали | ___ | ___ |
| Завершение и папка журнала | ___ | ___ |
