# E2E-5.19 — граница переноса полной миникарты

24.10.2026. Исходная точка: `db45c2d6d3ea003377b2df39732e4c032271c1eb`.

Выполнена одна адресная host-компиляция **полного** подготовленного
`MatrixGame/src/MatrixMinimap.cpp` на тех же include/define/sanitizer flags,
что у принятого MatrixWater. Исходный view не изменялся. Результат — compile
FAIL, не runtime. Команда и полный вывод сохранены отдельно:
`build/matrixgame/e2e519/minimap-baseline/result.json` и `compile.log`.

## Конкретные следующие зависимости

1. `Clear`, `StoreTexture`, `RestoreTexture`: прямые COM Release,
   GetSurfaceLevel, CreateTexture, AddDirtyRect, UpdateTexture и D3DX surface
   copy. Для этих операций уже существуют настоящие legacy_texture,
   unmanaged_texture и offscreen helpers; требуется перенаправить исходные
   вызовы, сохранив ветви отказа и порядок освобождения. Нельзя заменять
   StoreTexture/RestoreTexture пустыми методами ради закрытия map linkage.
2. `Init` и `RestoreTexture` используют CACHE_CREATE_TEXTURE; иконки в
   SMMTex::Load получают CTextureManaged через C-style cast результата кэша.
   В нынешнем portable cache логический class tag не гарантирует тот же
   динамический C++ тип. Эти потребители должны явно использовать genuine
   CreateUnmanaged/GetManaged. Глобальную portable factory подменять нельзя.
3. `SetAngle`, `BeforeDraw`, Map2World/World2Map требуют недостающих
   D3DXMatrixTranslation и двумерных TransformCoord/TransformCoordArray.
   Это оригинальная геометрия миникарты; её следует сохранить через
   существующий DirectXMath contract с адресными численными проверками.
4. `DrawRadar`, `Draw`, `RenderBackground`, `RenderObjectToBackground`:
   sampler/render state, target/depth/viewport, transform, material/light,
   Begin/EndScene. Часть операций уже покрыта общими адаптерами, но это ещё
   не доказательство выполнения полного рендера миникарты.
5. `LoadMinimapFromCache`/`StoreMinimapToCache`: настоящий GPU readback
   GetRenderTargetData и bitmap/PNG lifecycle. Существующий surface copy
   не следует автоматически считать эквивалентом GetRenderTargetData.
   Нужно сохранить исходный путь и проверить pitch, пиксели, владение и
   повторный вход на настоящем SDK backend.

Исходник upstream не изменялся. Новых SDK stubs, fake map и фиктивных
результатов не добавлено. Этот baseline не уменьшает ранее подтверждённые
11 map-owner missing symbols. CGame::Init и игровой мир не запускались.
Полная миникарта и Switch/deko3d остаются NOT EXECUTED / HARDWARE PENDING.

Основная NRO, аппаратные комплекты, пользовательские сейвы и ресурсы не
затрагивались; проба записывает только новые build artifacts.
