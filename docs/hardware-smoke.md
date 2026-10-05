# M10 Switch hardware baseline

## Первичная установка

1. Создайте `sdmc:/switch/space-rangers-hd-a-war-apart/`.
2. Скопируйте всю оригинальную игру в `game/`.
3. Скопируйте `SpaceRangersHDAWarApart.nro` в корень каталога.

## Каждый следующий тест

Заменяйте только `SpaceRangersHDAWarApart.nro`; `game/` не трогайте.

После запуска заберите `logs/port.log`. Сообщите, появилась ли diagnostic
frame с линиями и треугольником, нормальны ли цвета и aspect ratio, и вернулся
ли NRO самостоятельно в hbmenu.

Ожидаются строки `[BOOT]`, `[STAGE]`, `[M7]`, `[M8]`, `[M9]`, `[PACKAGE]`,
`[RESOURCE]` и `[SHUTDOWN]`. До загрузки DAT log отдельно проверяет
`install.txt`, `cfg.txt`, `DATA/common.pkg` и `install_russian.txt`.

Успешный M10 log последовательно содержит package collection, `INSTALL.TXT`,
language/packages (`configured sources=18`), Main/Lang/CacheData DAT semantic
fingerprints, `CFG create/save/reload`, derived M9 state, RGB565 framebuffer,
SDL renderer/texture, output/destination, present, package baseline и
`[BOOT] COMPLETE`. При первом запуске допустимо `CFG create PASS`; при
следующем — `CFG create SKIP existing`, но fingerprints и package baseline
должны совпадать. Для baseline пакета package log должен содержать
51 папку, 1890 файлов, 1940 записей, depth 4, hash `9c74d6b37be3edd2`,
`DATA/Asteroid/00.gai` и CRC32 `045269e4`.
