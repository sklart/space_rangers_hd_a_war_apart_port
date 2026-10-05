# Evening Switch smoke

1. Скопируйте `port/switch/SpaceRangersHDAWarApart.nro` в
   `sdmc:/switch/space-rangers-hd-a-war-apart/`.
2. Убедитесь, что собственная установленная игра находится в `game/DATA/`
   рядом с NRO.
3. Запустите NRO один раз и дождитесь возврата в меню/окно.
4. Пришлите единственный файл
   `sdmc:/switch/space-rangers-hd-a-war-apart/port.log`.

Ожидаются строки `[BOOT]`, `[PLATFORM]`, `[FILESYSTEM]`, `[PACKAGE]`,
`[RESOURCE]`, `[OKGF]`. Для baseline пакета package log должен содержать
51 папку, 1890 файлов, 1940 записей, depth 4, hash `9c74d6b37be3edd2`,
`DATA/Asteroid/00.gai` и CRC32 `045269e4`.
