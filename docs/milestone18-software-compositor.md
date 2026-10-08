# M18: portable software compositor

M18 накладывает один реальный decoded кадр на существующий RGB565 framebuffer
`GR_Main::ScreenRenderBuffer`, не создавая `TGraphBufGR`, `TgiGR`, Direct3D
объекты, cache/UI/audio runtime или upstream `GI_GAI` путь.

## Граница модуля

`port/switch/platform/software_compositor.*` — самостоятельный CPU-only модуль.
Он принимает BGRA source и RGB565 destination, ограничивает прямоугольник
экраном и необязательным clip, проверяет переполнения координат и pitch, затем
выполняет opaque conversion или целочисленный alpha blend. Пустое пересечение
успешно и ничего не меняет; некорректные параметры возвращают ошибку.

`host-software-compositor-test` фиксирует точные RGB565 pixels, CRC32/FNV-1a,
50%-ный blend красного поверх синего, negative clipping, repeatability и
некорректные входные данные. Отдельный symbol audit запрещает `GR_DX`,
`Direct3D`, `TGraphBufGR`, `TgiGR`, `TCGiEC`, `TCGaiEC` и `GI_GAI`.

## Runtime diagnostic

После M16/M17 приложение читает embedded `DATA/Asteroid/00.gai`, sequence 0,
первую позицию sequence и corresponding source frame, декодирует её через M16
Format-2 CPU decoder и alpha-композитит BGRA кадр в центре экрана. Hook вызывается
после M12 heartbeat и до уже существующей `BeginFramePresentation` /
`EndFramePresentation`; без установленного hook M12 ведёт себя как прежде.
Кадр не анимируется. `PLUS`, persistent M12 loop и shutdown остаются теми же.

`port.log` должен содержать `[M18] compositor BEGIN`, resource/sequence/frame,
decoded width/height/pitch, destination coordinates, CRC32/FNV, `[M18] compositor
PASS` и итоговый `[STAGE] M18 compositor PASS`.

## Статус

Реализация и ARM64 build готовы; Switch hardware proof ещё требуется. До него
M18 не считается COMPLETE и не является доказательством игрового UI или gameplay.
