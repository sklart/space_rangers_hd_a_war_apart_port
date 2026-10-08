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

**COMPLETE — HOST PASS / CI PASS / ARM64 BUILD PASS / HARDWARE PASS.** GitHub
Actions run `37739330544` passed the compositor regression and symbol audit. The
tested NRO was 7,199,024 bytes, SHA-256
`D2322AB4881796FFE3EB55CF493A6191A54547B636EBA10C24EC3FBF91BE9DDB`, with
embedded `build_git=f9833a5`. On Switch it decoded frame 0 as 33x40 BGRA/pitch
132 (`83f66519`/`eb000366ca288b23`), composited it at 623,340 in 1280x720,
and logged both M18 PASS markers. The supplied screenshot confirms the Asteroid
was visibly displayed. M12 ran 3,331 frames/presents for 166,725 ms, exited by
`PLUS`, and reached `[BOOT] COMPLETE`; the retained M17 first cycle also passed
in 5042 ms. M18 remains a narrow compositor diagnostic, not a game UI or
gameplay implementation.
