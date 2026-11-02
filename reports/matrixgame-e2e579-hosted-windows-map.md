# E2E-5.79 — full original Init/map с настоящим Rangers host на Windows

Статус: **Windows HAL full Init/map/frames/Deinit PASS; original texture factory
Windows/host-ASan/ARM64 PASS; SDK compile/link/package PASS; Switch NOT RUN**.
Evidence: `reports/matrixgame-e2e579-hosted-windows-map-evidence.json`.

Впервые текущий owning Rangers host прошёл совместно с original MatrixGame
полный Init, настоящую карту и её кадры на PC D3D9. Это новая композиция, а не
повтор старого standalone Windows теста без Rangers callbacks. Найдены и
исправлены три реальные ошибки, важные для 64-bit native порта. Основная NRO,
прежние E575/E578 артефакты, upstream/frozen/resources/saves сохранены.
Commit/push и взаимодействия со Switch не было.

## Исправления production source

### Реальные original cache classes

`CreateCacheTexture` всегда создавал явный CPU-only `CacheTexture`. Он подходит
к typed material API, но не является `CTextureManaged`. Full original Init с
живыми Rangers callbacks впервые входит в `CTransition::BuildTexture`, берёт
`CACHE_CREATE_TEXTUREMANAGED()` и вызывает original `LoadFromBitmap`. Старая
factory делала этот исходный cast некорректным. CDB воспроизвёл попытку COM
Release по адресу `0x2`; это не missing DLL и не ошибка GPU driver.

Добавлен scoped `OriginalTextureFactoryScope`. `InitializeOriginalGame` до
вызова `CGame::Init` связывает настоящий factory: `HNew(g_CacheHeap) CTexture`
или `CTextureManaged`. Он сохраняется через все Init/frame callbacks и
освобождается после original `CGame::Deinit`, когда объекты уже разрушены.
Original constructors, bitmap upload, texture lifecycle и backend operations
сохранены. Никаких COM-подобных CPU объектов вместо обязательной текстуры нет.

Минимальный native CPU material API продолжает получать свой явный
`CacheTexture` и immutable `TextureAsset` handle. Borrow такого material из
original texture отклоняется. Full-game owner без оригинальной factory теперь
получает явную ошибку вместо возврата несовместимого CPU объекта. Exclusive
binding, invalid/null factory, cleanup и reentry проверены.

Production files: `cache_resources.hpp/.cpp`, `original_game.cpp`.

### Два 32-bit pointer-copy дефекта

После исправления factory Init дошёл до реального рендера фона миникарты.
Первый null-доступ в `CTerSurface::BeforeDraw` оказался повреждённой sorted
queue: исходный `memcopy_back_dword(..., rc)` сдвигал массив указателей как
массив 4-byte words. На x64/ARM64 это копирует половину нужных данных.
Следующий такой же дефект найден в `CMatrixMapStatic::Sort` первого map draw.

В generated overlays оба right shifts используют pointer-width `std::memmove`.
Оба left shifts также переведены с overlapping `memcpy` на `memmove`.
Алгоритм сортировки, сравнение, лимиты, вставка и порядок объектов не менялись;
для 32-bit pointer width сохраняется прежний результат. Общий dword-copy helper
не менялся: его serialized/pixel callers по-прежнему копируют именно words.

Изменены только `tools/matrixgame_terrain_overrides.py` и
`tools/matrixgame_map_static_overrides.py`; pinned upstream не редактировался.
Падения сохранены в CDB logs/maps/EXEs. Старые terrain/static source и object
копии находятся в `baseline-terrain/` и `baseline-static-sort/` под E579.

## Реальное Windows исполнение

Собраны текущие пять production MatrixGame archives: **159 MSVC x64 objects**.
Переиспользованы 337 проверенных Rangers objects E577, реальная static SDL
dummy/Ogg/Vorbis цепочка и original installed data. Три новых scenario TUs и
original font resource образуют joint EXE. CRT `/MT`, assertions включены.
Старые unrelated provider/selection/input preflight suites не запускались.

В каждой из **двух** полных сессий:

1. Original data/config/cache и full16 Rangers services создаются и остаются
   живыми через runtime. Таблица скопирована; host/context остаётся borrowed.
2. Настоящий `CGame::Init` проходит config, map ctor, RobotPreload, storage load,
   PrepareMap, interface/final resources и D3D9 Reset 800×600, затем возвращается.
3. Загружена original карта **276×285**, groups **28×29**.
4. Реальный hosted loading transition завершается за **30 original Takt/Draw
   frames** по 25 ms. Он не отключался. Его начальный одноцветный captured
   loading screen не выдаётся за изображение карты.
5. После завершения transition выполняются **четыре** original map Takt/Draw,
   readback и Present. В каждой 800×600 RGB capture — **147288 distinct colors**.
   Всего сохранено **восемь PNG**; требования к nonuniform readback сохранены.
6. Original Deinit освобождает map/cache/skin/resource owners. Rangers host
   остаётся живым до завершения Matrix cleanup, затем освобождается сам.
7. Повторный Deinit безопасен. Cwd не меняется, resource/output scopes очищены.
   Window geometry, gamma, framebuffer/depth/viewport и device/timer leases
   проверены настоящими Windows SDK readbacks после выхода.

Полный stdout и callbacks/resources journal сохранены. Final audit нашёл два
завершённых журнала с текущим composition source SHA и успешным `B.CGame::Init`,
без failure events. PNG независимо декодированы: chunks/CRC/zlib/filters,
размеры и число RGB colors. Один фактический кадр также визуально просмотрен:
видны карта, роботы, здания, вода, интерфейс и миникарта.

Кадры: `build/matrixgame/e2e579-hosted-windows-map/runtime/output/map-frame-0.png`
… `map-frame-7.png`. Это **Windows reference**, не Switch capture, не проверка
слышимости SDL audio, физического HID или полного battle playthrough.

В test recipe дополнительно исправлены собственные ошибки: journal теперь
получает настоящий 64-hex composition fingerprint; cleanup не вызывает
EndScene после уже завершённого/detached successful runtime. Ни production
assertions, ни nonempty image check ради прохождения теста не ослаблялись.
Неуспешные попытки и их отдельные logs/results сохранены.

## Проверки CPU, rollback и source compatibility

`test_matrixgame_original_texture_factory.cpp` выполнил actual dynamic-type
checks, original/CPU material boundary, exclusive scope, отказ invalid factory,
очистку и два цикла reentry:

| Профиль | Результат и предел |
| --- | --- |
| Windows MSVC x64 | Compile/link/runtime PASS, без GPU load в этом unit test |
| Host ASan/UBSan | PASS с leak detection; test, cache_resources и original_game instrumented, reused dependencies имеют прежний instrumentation scope |
| ARM64/QEMU | Compile/link/runtime PASS тех же assertions; не ARM GPU/ASan |
| Actual Windows hosted map | Две полные реальные HAL sessions и восемь visible map frames PASS |
| Early hosted native Init rollback | Повторены только два текущих caps-refusal cycles: real text/sound, copied table, host lifetime и cleanup PASS |
| devkitA64/libnx/deko3d | Текущие factory и оба queue overlays входят в новую standard diagnostic NRO; link/package PASS, runtime NOT RUN |

Для independent PNG audit добавлен opt-in RGBA decode к прежнему чистому
stdlib verifier. Default RGB contract сохранён; старые 36 PNG / 18 layout cases
E567 проверены без нового renderer/test build. Новый RGBA readback дал те же
147288 RGB colors, что независимый C++ SDK readback counter. Pillow не нужен.

Ранее пройденные большие standalone host/ARM64/SDK и old Windows suites не
повторялись без причины. Новые joint attempts требовались для воспроизведённых
production failures. Долгий host ASan link отслеживался по тому же живому tool
handle до фактического exit 0; из-за отсутствия промежуточного вывода не
перезапускался и не прерывался.

## Новая отдельная SDK NRO

`build/matrixgame/e2e579-hosted-windows-map/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro`

- **14891205 bytes**.
- SHA256 **`020b6d3debff52d5ae1efb757b2aee10b6e9886692d83a2d2d8abac2cabf0474`**.
- Build ID `82034333146d-dd869150af77e82b22bc`.
- Target `matrixgame-native-diagnostic-nro`; обычные SDK nacptool/elf2nro.
- AArch64 PIE, strong undefined **0**; real owning Rangers services, CGame::Init
  и DekoDevice присутствуют в ELF.
- NRO/ASET/icon/NACP, source/dependency fingerprints и link inputs проверены.
- Это diagnostic artifact с известным runtime gap ниже; **Switch NOT RUN**.

E575 NRO сохранена с SHA
`9dcd90d5175bc0ae7cbfe6db75b180b1fca406b01655880cf64d6f4f31cab356`.
Основная и cumulative NRO не менялись. Resource roots, private outputs и
read-only 26-entry resource plan прежние; см.
[инструкцию E575](../docs/matrixgame-e2e575-hosted-diagnostic.md) и
[дополнение E579](../docs/matrixgame-e2e579-hosted-windows-map.md).

## Воспроизведение

Из корня проекта, с установленными SDK/VS/Python/WSL dependencies:

```powershell
$env:PYTHONIOENCODING='utf-8'
python tools/matrixgame_hosted_windows_map_native_build.py
python tools/matrixgame_original_texture_factory_checks.py windows
python tools/matrixgame_original_texture_factory_checks.py host
python tools/matrixgame_original_texture_factory_checks.py arm64
python tools/matrixgame_hosted_texture_prefix_checks.py
python tools/matrixgame_hosted_windows_map_checks.py
python tools/matrixgame_hosted_texture_sdk.py
python tools/matrixgame_hosted_windows_map_audit.py --output reports/matrixgame-e2e579-hosted-windows-map-recheck.json
```

Native build/check wrappers используют текущие production targets и сохранённые
архивы прошлых проверок; exact paths/hashes записаны. Audit создаёт новый файл
exclusively. Protected 78 game inputs, доступные frozen/save artifacts,
primary/cumulative NRO, clean upstream pins и `D:/logs2.7z` сверены read-only.
Игровые resources/settings/saves, upstream и frozen bundles не изменялись.

## Точная достигнутая цепочка и недостающий native-компонент

Windows reference теперь доказывает full original Init → карта → transition →
map frames → Deinit **с реальными Rangers services**. Это закрывает dependency
и lifetime gates этой композиции на PC, включая обязательную ветку transition,
которая при null Rangers interface прежде не исполнялась.

На Switch/non-Windows обязательная ветка `CTransition::BuildTexture` ещё не
интегрирована: `transition::CaptureDC`, Bitmap save/readback и GDI-compatible
bitmap/DC/copy operations явно выбрасывают Unsupported. Наличие full Rangers
table делает эту ветку достижимой в native diagnostic. Следующий конкретный
software компонент — настоящий framebuffer capture provider, подающий реальные
captured pixels в оригинальный transition algorithm. Нельзя пропускать transition
или подставлять одноцветную/выдуманную loading image вместо обязательного capture.

Полный native/deko Init/map/frames в новой NRO остаётся **PARTIAL / NOT RUN**.
Последняя измеренная Switch boundary из старых logs2 — ReadParams → ApplyGammaRamp
до map ctor — не заменяется PC-результатом. Нужны последующие native software
integration, затем пользовательские hardware image/audio/HID/focus/F9/recovery
проверки. Аппаратный запуск и перенос файлов остаются за пользователем.
