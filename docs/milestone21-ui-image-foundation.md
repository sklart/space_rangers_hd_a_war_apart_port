# M21 — Portable UI Image Foundation

## Статус

**IN PROGRESS.** Host-путь для статичных изображений существует и проходит
`host-m21-ui-image-regression`, но это не M21 PASS. Release inventory и
независимый Simple oracle теперь есть, однако интегрированная fingerprint
сцены, CI, clean ARM64 build и один финальный аппаратный тест ещё не получены.
До них NRO на Switch не развёртывается и hardware evidence не запрашивается.

## Границы

`PortableImageObject` — отдельный от M20 `GIObject` путь:

```text
compressed image -> image_cpu -> Simple / Trans / Alpha -> layout -> RGB565 framebuffer
GAI/GI animation -> GIObject -> M19 Scene -> RGB565 framebuffer
```

Он не вводит Forms, controls, input, fonts, audio, cache/worker, `TGraphBufGR`,
Direct3D или ownership renderer-а. `GIObject` остаётся владельцем анимированных
GAI/GI ресурсов; `PortableImageObject` владеет только своим декодированным
статичным representation и освобождает его при reload/clear/destruction.

## CPU primitives

- `image_cpu` декодирует RGB565, BGRA8888, RGB888 и Gray8 с пределом 256 MiB
  до allocation; ошибка очищает destination.
- `SimpleBitmap` рисует RGB565 прямым clipped-copy и использует M18 alpha
  compositor только для `RGBA`; `HalfAlpha` относится лишь к RGB565 пути.
- `TransBitmap` строит keyed-RLE с ключом `0x0000`, поддерживает `270`,
  `Stretch=W,H`, `&`, clipping и HalfAlpha.
- `AlphaBitmap` строит один раз при `Load()` три validated RLE buffers и рисует
  строго `AlphaBuf`, `TransAlphaBuf`, `TransBuf`. `DecodeToBGRA` повторяет тот
  же порядок в нулевом BGRA destination.

Все RLE streams проверяются до draw: header, dimensions, stream length,
literal payload, row width/count и trailing bytes.

## Layout и object semantics

`Rect` использует half-open границы `[left,right) × [top,bottom)`. `SetSize`
задаёт client rect, но не масштабирует content. Final position равна
`x - origin_x`, `y - origin_y`.

Поддерживаются `Left`, `Center`, `Right`, `Top`, `Center`, `Bottom`, а также
`LeftFill`, `RightFill`, `TopFill`, `BottomFill`; reverse fills начинают с
right/bottom и идут назад. `CenterFill` намеренно возвращает unsupported:
это текущее observable upstream behaviour для этих image primitives. Alpha
pixel hit-test использует rendering в один RGB565 pixel и проверяет `!= 0`.

## Проверки

`make -C port/switch host-m21-ui-image-regression` объединяет retained
M12–M20 host regressions с image CPU, Alpha, Trans, Simple, layout и object
tests. Four-channel uncompressed PSD fixtures проверяют реальную partial-alpha
ветку portable OKGF без игровых файлов и без host JPEG/PNG dev dependencies.

Этот gate и M21 symbol audit добавлены в `package-host` CI. Он не является
подменой release corpus/oracle, ARM64 или Switch проверки.

## Release inventory и независимый oracle

`tests/probe_ui_images_release.py` read-only декодирует `Main.dat`, оба
language DAT и `CacheData.dat`, а затем выбирает только static resources,
которые реально разрешаются из конфигурации. В базовом 2.1.2500 релизе он
нашёл 800 `Simple` и 6 `Alpha` mode-ссылок, но ни одной `Trans` mode-ссылки.
Единственный детерминированный static Simple baseline —
`Bm.Planet.T.Spu00` из `Data/SE/Sputnik/00/Image`, сопоставленный с
`DATA/Planet/Spu00.png` в `common.pkg` (7,389 bytes).

`tests/probe_m21_simple_oracle.py` независимо читает package, проверяет PNG
chunks/CRC, сам применяет PNG filters и Adam7 passes и переводит indexed RGB
в little-endian RGB565. Для этого baseline он фиксирует source
`CRC32=a3721a9c`, `FNV64=32ebfdd05d7fa674` и decoded RGB565
128x60/pitch 256, `CRC32=51e16db2`, `FNV64=ffeaf550d3c28655`.

В исходном release config `Trans` имеет **NOT PRESENT**. Шесть `Alpha`
ссылок `Bitmap.BGObj.O1/O11/O12/O13/O14/O15` существуют в `Main.dat`, но не
имеют mapping в базовом `CacheData.dat`, `Rus/Lang.dat` или `Eng/Lang.dat`;
для M21 они обозначаются **release resource NOT PRESENT**, а не заменяются
произвольной PNG/PSD из package. Synthetic Alpha/Trans coverage остаётся
обязательной и проходит в host gate.

`host-m21-simple-release-test` предназначен для ПК с libpng development
headers: он сравнивает полный C++ PNG decode с указанным Python oracle. На
текущем Windows/MSYS host эта проверка пока BLOCKED (нет `png.h`/libpng
development package); это не выдаётся за C++ release PASS и не включается в
CI target, потому что CI не содержит локальную коммерческую game tree.

## Что не заявляется

M21 не переносит полный `TImageGI`. После него всё ещё будут отдельными
future scopes: `Anim`, `GraphBuf` как UI-object, text/fonts, parent/child UI,
input/focus, config-driven Forms, menu, audio и gameplay. M22 не начинается
автоматически.
