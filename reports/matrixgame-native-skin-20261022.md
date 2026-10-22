# MatrixGame: skin resources → VO surfaces → deko draw

22.10.2026. Продолжение E2E-5.18 после `c585b49`, локальный `master`, без Switch.
Коммит кода: `9df6f981ac1edbe13b6043c9bbdeca72e8a6bcb8`. Push не выполнялся.

## Реальное продвижение

Предыдущему draw требовался вручную заданный материал каждой поверхности.
Теперь `NativeDekoSession::DrawSurfaceTextures` читает имена из настоящего
`CVectorObject::GetSurfaceFileName`, разрешает их через native skin library и
оригинальный кэш, затем передаёт texture leases в существующий deko upload/draw.
Skin library принадлежит сессии; уничтожается после renderer, перед кэшем.

`matrixgame_skin_overrides.py` проверяет pinned SHA `MatrixSkinManager.cpp/.hpp`
и генерирует `SkinResourcesNative.cpp`:

- `NativeSkinLibrary::Acquire`: исходный prefix `CSkinManager::GetSkin` до
  публикации объекта и D3D `Prepare`. Сохранены ParamParser, позиции
  base/gloss/back/mask/UV speed, `.` для отключения gloss, автоматический
  `_gloss`, ветвь `GSP_SIDE → GSP_SIDE_NOALPHA` и значения групп.
- Gloss setting передаётся владельцем явно вместо неготового `g_Config`.
  Factory возвращает типизированные cache handles, не `CTextureManaged*`.
- Публикация через shared_ptr/vector заменяет raw HAlloc: поля нетривиальны,
  исключение не должно оставлять увеличенный счётчик/полуобъект. Ключ дедупликации
  сохраняет четыре texture identity и две скорости, исключает текущую UV позицию.
- `NativeSkinLibrary::Takt`: всё исходное тело `CSkinManager::Takt`, изменён
  доступ к контейнерам экземпляра. Сохранён один шаг wrap и граница `-1`,
  включая поведение больших дельт; fmod вместо оригинального алгоритма нет.

Это **не полный перенос TU MatrixSkinManager.cpp**, исходного `GetSkin`,
D3D `SMatrixSkin::Prepare` или `SSkin` callbacks. `NativeSkin::Preload` — явная
проверка всех заданных texture resources, не подмена исходных preload callbacks.

## Что действительно выполнялось

**Host ASan/UBSan PASS и ARM64/QEMU PASS:**

`NativeInitRuntime::Begin → original Cache → original skin resource prefix
→ dedup → original Takt → VO surface names → SurfaceMaterials
→ PrepareCachedFrame/BigVB/BigIB → Clear/Deinit → повторный вход`.

Проверены две lifecycle-итерации, разделение групп по alpha flag, явный и
автоматический gloss, `.` и выключенный gloss setting, mask/back, signed UV speed,
дедупликация после Takt, некорректная группа/пустой ключ, отсутствующая и
обрезанная PNG, инвалидирование handles при Clear и уничтожении кэша.
GNU linker wrap `_Znwm` вызвал настоящие std::bad_alloc на позициях 1/3/6
в каждой итерации: шесть отказов на host и шесть на ARM64. При отказе skin
не опубликован; последующая загрузка проходит. ASan/UBSan/leaks чисты.

Read-only вход — настоящий wood05.vo, SHA-256
`04497dcc94ede26f183d847a4dd094d3ba05bc825d09fcfa73fa36bfae80981e`.
Для контролируемой интеграции CStorage сохраняет **отдельный derived VO** в папке
теста, меняя только surfs/texs на PNG fixture. Исходная VO не изменяется.
PNG и её base/gloss копии — **не игровые текстуры**, SHA исходной PNG
`d81cd93a0fccbc517719f9cb6563f6837327dc32a412f59238a95699831ba210`.
Это исполнение исходного кода с контролируемым ресурсом, не visual parity.

**devkitA64 compile/link PASS:** слинкован фактический entry
`MatrixGameNativeSkinDraw → NativeDekoSession → DrawSurfaceTextures
→ Acquire/SurfaceMaterials → SetTextureAsset → DrawFrame`.
Strong undefined symbols: 0. Entry offline **не вызывался**.
ELF: `build/matrixgame/native-skin/sdk/cmake/matrixgame-native-skin-probe.elf`.
SHA-256 `e4a1f741c40edd0ecbdc2619b894667a7ce006fce10c49f14fd3a4a452749643`.
Build ID `20519f2e0bdcac657ee9bceccdaf9045d739220a`.

Generated effective sources одинаковы в трёх сборках по SHA-256. Host/QEMU
исполняют CPU-часть и fault cases; SDK компилирует deko TU без GNU fault wrapper.
build_dirty=true: сборки до коммита; нормализованные изменённые исходники сверены
с его blobs. Следующий коммит — docs. Подробности, hashes, flags и команды:
[машинное свидетельство](matrixgame-native-skin-20261022-evidence.json).

## Оставшаяся граница

`NativeSinglePass` принимает только ordinal base texture и передаёт alpha-test
flag существующему raster adapter. Side/gloss/mask/back effects отвергаются явно.
Lighting, sampler и остальные raster inputs остаются явным native shader contract.
D3D image parity не заявляется. UV проверены на CPU; анимация маски на GPU ещё
не реализована. `DrawSurfaceTextures` — ordinal путь, не определение GSP_SIDE
для роботов по модели.

`VectorObjectCpu.cpp::LoadSpecial` по-прежнему отвергает режимы кроме OLF_NO_TEX.
Для настоящих skins нужны typed SSkin callbacks и эквивалентный Prepare/pass
contract; исходный CVectorObject::Draw их ещё не получил. Полные
VectorObject.hpp/.cpp требуют D3D9. В 3g.cpp::L3GInitAsDLL остаётся borrowed
device/window contract с WndProc, caps/gamma; offscreen deko-сессия ещё не включена
в исходный CGame::Init. RunGameLoop, CFormMatrixGame и бой не исполнялись.
Compile inventory 14/92 из E2E-5.18 не повторялся; новых full-TU PASS не заявляем.

**HARDWARE_PENDING:** GPU entry, изображение/реальные материалы, отказ GPU
allocation, GPU cleanup/reentry. Новый ELF не является установочной NRO.
Следующий Switch-прогон использует прежний
[вечерний сценарий](../docs/e2e517-evening-switch-check.md) и замороженные комплекты;
код этого отчёта в их NRO отсутствует.

## Воспроизведение и сохранность

CMake source: port/matrixgame/native-engine, target: matrixgame-native-skin-probe.
Host: RelWithDebInfo и -fsanitize=address,undefined -fno-omit-frame-pointer.
Запуск: `tests/probe_matrixgame_cached_frame.py --skin-resources --binary <binary>
--model <wood05.vo> --texture <plain.png> --output <fresh-build-dir>`.
ARM64: тот же driver с --qemu; paths зависимостей/flags в evidence JSON.
SDK: `python tools/matrixgame_init_sdk.py --skin-resources --output <fresh-build-dir>`.
Логи: build/matrixgame/native-skin/. Старые standalone suites не повторялись.

Сверены до/после 13 защищённых файлов: saves/config/logs, пользовательский
provenance, основной и GPU аппаратные ZIP, G:/pack.zip, исходные VO/PNG.
Все SHA совпали. Upstream/gitlink, основная NRO, игровые ресурсы и настройки
консоли не изменены. Push нет.
