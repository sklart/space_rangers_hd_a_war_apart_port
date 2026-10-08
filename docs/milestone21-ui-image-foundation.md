# M21 — Portable UI Image Foundation

## Статус

**COMPLETE.** `host-m21-ui-image-regression`, independent release oracle and
CI `37770073920` passed before the only physical test. The final 7,547,184-byte
NRO SHA-256 was `6D387610DAD7DA57C1D9F21F00ABF84C67F9AA266421CF54C98567656C8FA3F1`
with embedded `build_git=fc8adfc`. On Switch, actual and expected M21 RGB565
scene fingerprints matched: CRC32 `4f915772`, FNV64 `52449ae8f8f56c6c`,
1,843,200 bytes. It ran 172,747 ms and presented 3,824 frames, retained the
M17 cycle evidence, exited via `PLUS` and reached `[BOOT] COMPLETE`.

Earlier `6fb6e98`, `4c7012b` and `82aa2e9` executions remain clean diagnostic
FAILs, not additional M21 hardware checkpoints: they exposed the Python
oracle's RGB565 expansion and full-height heartbeat-marker mistakes. `fc8adfc`
is the sole final hardware PASS.

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

`PresentationScene` — минимальная borrowed-entry сцена без ownership
renderer-а: она стабильно сортирует M20 `GIObject` и M21 `PortableImageObject`
по layer и рисует их в один RGB565 framebuffer. `test_m21_presentation_scene`
использует один GIObject, Simple, keyed Trans и partial-alpha Alpha, проверяет
repeat на чистом logical framebuffer и фиксирует два кадра: A
`1a829653`/`658ac816b5479db3`, B после GI transition
`383a8729`/`69ffb049d0071ebb`. Независимый
`tests/probe_m21_presentation_scene.py` воспроизводит этот synthetic
checkpoint без C++ и зафиксирован в CI.

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

## Runtime diagnostic integration

Диагностический runtime загружает ровно выбранный release `Simple`
`DATA/Planet/Spu00.png` через `PortableImageObject`, удерживает его decoded
RGB565 representation и до запуска persistent loop сверяет source и decoded
fingerprint с independent oracle. `Trans` и `Alpha` не подменяются: в текущем
release они логируются как `NOT_PRESENT`.

Первый fixed logical frame включает heartbeat frame 0, три M20 GIObject и
этот Simple image. `tests/probe_m21_release_scene.py` независимо получает
RGB565 framebuffer `1,843,200` bytes, CRC32 `4f915772`,
FNV64 `52449ae8f8f56c6c`; runtime сравнивает тот же fingerprint до обычной
animation. Instrumented Switch run `82aa2e9` получил эти же actual значения:
исправление oracle добавляет полный vertical heartbeat marker, прежде
ошибочно пропущенный вне `(0,0)`. Предыдущие clean shutdown остаются
diagnostic **FAIL**, а не crash Homebrew и не hardware PASS. Для исправленного
NRO остаются terminal CI и повторный hash-bound Switch test.

## Что не заявляется

M21 не переносит полный `TImageGI`. После него всё ещё будут отдельными
future scopes: `Anim`, `GraphBuf` как UI-object, text/fonts, parent/child UI,
input/focus, config-driven Forms, menu, audio и gameplay. M22 не начинается
автоматически.
