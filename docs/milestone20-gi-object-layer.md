# M20 — portable GI object layer

M20 вводит `platform::gi_object::GIObject`: один объект владеет метаданными и
байтами одного GAI-ресурса, текущим кадром и одним CPU-изображением. Ресурс
загружается через существующий `Package`/GAI-путь; Format 0 декодируется
существующим CPU-декодером, а Format 2 — существующим RLE CPU-декодером. Новый
декодер, глобальный cache/LRU, worker и фоновые загрузки не добавляются.

`Update(delta_ms)` использует M17 `gai_playback_cpu::AdvanceBy` и декодирует
кадр только после смены source frame. `Draw(scene)` — единственное место,
где объект передаёт текущий кадр в M19 `Scene`; основной Switch-код не создаёт
`SceneSprite` из decoded pixels.

Switch-диагностика открывает `DATA/common.pkg`, выбирает Asteroid и ещё два
лексических декодируемых `DATA/*.gai`, создаёт три `GIObject`, задаёт top-left
позиции, слои `0/10/20` и alpha `255/255/128`. В каждом кадре M17 и M20
обновляются одним monotonic callback, затем все объекты перерисовывают текущую
сцену поверх M12 heartbeat. Логируемые доказательства: `[M20] GI object BEGIN`,
`objects=3`, ресурс/кадр/координаты/слой каждого объекта, fingerprint сцены и
`[M20] GI object PASS`.

## Проверки и текущий статус

`host-gi-object-test` строит синтетический package с двумя GAI-анимациями и
проверяет загрузку, переход кадра, порядок слоя, точный полупрозрачный RGB565
пиксель `0x03ef`, visibility и детерминированный fingerprint. На Windows/MSYS
цель компилирует только нужные CPU-модули OKGF, поэтому ей не требуется JPEG
host SDK. ARM64 NRO успешно собран локально. GitHub Actions `37753205358`
прошёл M20 host-регрессию и отдельный symbol-audit. Hash-bound Switch NRO
`73340fc` (SHA-256 `0F7B909287B28B4D6DD7CE3617DF73E869907ED9F328F9E6A71AF81970523243`)
запустил три реальных `DATA/Asteroid/*.gai`, сохранил alpha/layer, прошёл M20
и M17, выполнил 3,824 presents за 172,463 ms, затем `PLUS` и `[BOOT] COMPLETE`.
M20 COMPLETE.

## Граница M21 AlphaBitmap

Анализ upstream `EC_CacheAlphaBitmap` показывает, что `TCAlphaBitmapEC`
строит три буфера (`TransBuf16`, `TransAlphaBuf16`, `AlphaBuf`) из RGBA,
участвует в `EC_Cache` queue/acquire и создаёт `GR_DX` texture cache через
`TGraphBufGR`/Direct3D. Ни один из этих типов или путей не входит в M20.
M21 должен отдельно спроектировать CPU-only представление alpha-буферов и
явные ownership/eviction правила; эта подготовка не является реализацией M21.
