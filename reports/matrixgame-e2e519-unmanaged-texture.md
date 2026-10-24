# E2E-5.19 — оригинальный CTexture и default-pool loss/reset

24.10.2026, продолжение `959c6d8`, локальный master, без push.

## Новый исполняемый оригинальный код

Подключены все шесть out-of-line методов CTexture: OnLostDevice, OnResetDevice,
LoadFromBitmap, Unload, Load, MakeSolidTexture. Inline constructor/destructor/
Preload/Tex/Set остаются в исходном header с прежним layout. В обоих generated
и future full Texture.cpp сохранены одинаковые адаптированные тела методов.
Остальной Texture.cpp целиком **не перенесён и не компилировался**; при будущем
подключении full TU selected texture TUs нужно исключить из того же link.

CreateUnmanaged создаёт настоящий original CTexture, caller-owned на том же
allocator contract, что CreateManaged. Оба типа используют единственный
существующий registry. Original CBaseTexture::OnLostDevice/OnResetDevice
подключены в его TU; их dispatch не заменён отдельным списком.

Сохранён исходный порядок backup default→system, copy каждого mip, restore
system→default, dirty/update, флаги и OOM-counter/usage union. MakeSolidTexture
использует исходный system-memory fill и UpdateTexture. Bitmap path сохраняет
original CBitmap DDS serialization, default pool и точные D3DX filters
TRIANGLE|DITHER / BOX. Дублирующие UpdateTexture внутри исходного mip loop
не оптимизировались.

Новый SDK backend предоставляет настоящие Describe/Surface/Dirty/Update/
D3DX surface copy/decode. Вне Windows отсутствующий GPU явно отклоняется.
ResourceDesc вынесен в отдельный contract: принятые трёхполевые LevelDesc и
legacy_texture_backend.hpp/.cpp **не менялись**, что проверено против Git blob.
Ни новый default-pool объект, ни портируемые потребители не заменяют глобальную
portable cache factory: CacheClass::Texture там по-прежнему может означать
другой CacheTexture. Original consumers нужно явно направлять в CreateUnmanaged.

## Реальные результаты

Общий host ASan/UBSan/leak-check и ARM64/QEMU, два сеанса:

- genuine factory, тип, registry, повторный Unload и пустые lost/reset callbacks;
- original bitmap DDS prefix и MakeSolidTexture доходят до явного SDK refusal
  без устройства; объект остаётся unloaded и корректно уничтожается;
- настоящий unloaded CTexture передан actual g_Cache через Add; последующий
  parent CacheDeinit уничтожает его виртуально и возвращает общий registry в0.
  Реальный cache heap/owner и повторный вход проверены без подставного мира.

Windows MSVC x86 / настоящий D3D9 HAL:

- **original MakeSolidTexture** создаёт настоящий 8×8 default-pool ресурс;
- **original LoadFromBitmap** сериализует bitmap и создаёт настоящий ресурс
  с четырьмя mip levels 8→4→2→1;
- actual global OnLostDevice переносит их в system memory. Проверены pool,
  format, dimensions и все пиксели каждого уровня с учётом pitch;
- duplicate loss оставляет ту же backup texture; managed guard остаётся
  unloaded. Global OnResetDevice возвращает default-pool ресурс, сбрасывает
  TF_LOST, повторный reset не пересоздаёт его. Следующий loss/readback проверяет
  сохранность пикселей после restore; оба ресурса проходят два цикла;
- повторный Unload, Destroy и registry cleanup завершаются успешно.

Это выполнение оригинального CTexture, не только низкоуровневого API helper.
В standalone SDK caller нет NativeInitRuntime::Begin; cache-parent/heap proof
получен отдельно в общем host/QEMU runtime. Windows allocator использует
существующий fallback contract. Проверенный resource Usage=0; render-target/
dynamic usage и отказ посередине allocation/copy не покрыты. Использован
controlled in-memory bitmap; пользовательские ресурсы не записывались.

Физический Device::Reset не вызывался: подтверждены **уведомления и перенос
ресурса**, а не весь device recovery. File-based CTexture::Load только compile
proof. Полная миникарта, её StoreTexture/RestoreTexture и Switch/deko3d
**NOT EXECUTED / HARDWARE PENDING**. Source failure paths/OOM transactional
cleanup не объявляются полностью проверенными или исправленными.

## Фактический cache-owner contract

Windows linkage показала, что NativeCacheFactoryBound тянула весь init_runtime
с неперенесёнными dependency closures камеры/интерфейса. В cache_owner_state.cpp
вынесен единственный owner slot и та же проверка `active && active->CpuReady()`.
Begin/Deinit используют ссылку на этот slot. Остальной init_runtime восстановлен
из предыдущего blob и сравнён целиком: кроме include/slot alias/перемещённой
функции изменений нет. Заглушка состояния не вводилась.

Общий host/ARM64 runtime повторно прошёл после разделения: реальная portable
cache factory, parent cleanup, отказы повторного владельца и два входа остаются
рабочими. Windows SDK test слинкован с настоящими CacheNative/cache_resources,
owner-state и ранее собранной original VO library; Cache.hpp и
VectorObjectShared.hpp с её source view совпадают побайтно. VO suites не
повторялись. Build failures до закрытия этих реальных dependencies сохранены.

## Сборка, provenance и оставшаяся граница

devkitA64: common-prefix COMPILE/LINK PASS, strong undefined=0, CTexture /
CreateUnmanaged retained. ELF SHA-256:
`0592983e0c4222f8ef6e8b07db2c491bb6347d5217aa8f150a9d6d211d5494c1`.
Новая NRO не создавалась; Switch runtime HARDWARE PENDING.

Host/ARM64 и Windows результаты разделены в
[evidence](matrixgame-e2e519-unmanaged-texture-evidence.json). Новых sanitizer
исключений нет; прежние Console globals и шесть world/effect vptr ограничений
сохраняются. Fresh CMake configure/generate PASS; семь новых/обновлённых compile
recipes проверены, повторная полная сборка не выполнялась.

245 engine files в трёх source views совпали, отдельно66 core outputs в трёх
views. Изменились только Texture.cpp, selected managed TU и новый unmanaged TU.
Новые repo backend/owner/test sources имеют отдельные hashes. Upstream/core
не менялись, evidence собран до коммита, build_dirty=true.

Map owner остаётся **LINK FAIL11**: этот шаг закрывает настоящий texture runtime,
но не оставшиеся derived object/side/minimap/hint/interface destructors.
Точные symbols записаны в evidence. CGame::Init, RunGameLoop, CFormMatrixGame,
загруженный мир и планетарный бой **NOT EXECUTED**.

Снова совпали13 доступных protected inputs,107 аппаратных файлов, frozen main
NRO/ELF и оба аппаратных ZIP. G:/pack.zip недоступен. Сейвы/ресурсы и настройки
Switch не менялись. Причины intro raster, двух historical manual-publish ENOENT
и зависаний карты/консоли остаются OPEN; новых аппаратных данных нет.
