# MatrixGame E2E-5.27 — native output/cache provider

Подключён реальный provider для original PathToOutputFiles и файлового кэша
миникарты. Native entry передаёт явный private output root, который принадлежит
NativeInitRuntime до конца Deinit. Full native Init/первая карта/кадр на Switch
ещё не выполнялись: PARTIAL / HARDWARE_PENDING.

## Что изменено

- output_paths::NativeScope хранит отдельный root; FolderItem удерживает
  собственный snapshot пути. Native ветка не имитирует Shell PIDL и не возвращает
  фиктивный успех отсутствующего provider. Неуказанный root остаётся явной ошибкой
  native output scope unavailable при обращении к выходным файлам.
- NativeGameOptions.output_root копируется при входе. После успешного Init scope
  переходит к runtime owner; после отказа Init снимается при unwinding. Deinit
  освобождает binding после original map/font/cache teardown. Повторный вход
  не наследует чужое output binding.
- Standalone SDK entry задаёт sdmc:/switch/srhd-awa-matrixgame-native. Это отдельное
  дерево выходных файлов; ELF не запускался, на SD ничего не создавалось.
- Original PathToOutputFiles сохраняет SpaceRangersHD + Cache/Screenshots layout,
  исходный порядок PersonalLocation/PersonalPath/CreateFolder и Windows fallback.
  Добавлена последняя native separator boundary Finalize; Windows без native scope
  получает исходный путь без преобразований.
- Native backend выполняет настоящие create_directories. Ошибки не превращаются
  в созданные каталоги. Существующий каталог возвращает false, как original
  CreateDirectoryW. Никакого удаления output tree при Deinit нет.
- Wide -> UTF-8 boundary поддерживает wchar16/wchar32, кириллицу и non-BMP.
  NUL/непарные surrogate/недопустимые scalars отвергаются. Root явно абсолютный;
  traversal, выход за него и существующие symlink в output пути отвергаются.
- У CMinimap адаптированы ровно два прямых filesystem вызова: exists при загрузке
  кэша и fs::path при его сохранении. Они используют FilesystemPath вместо
  зависимого от locale wide path constructor на POSIX. PNG алгоритм, размеры,
  texture formats, system/default transfers и original draw не заменены.
- Независимая подготовка sources и SDK preparation дают одинаковые 252 файла.
  От E2E-5.26 изменились ровно MatrixGamePathUtils.cpp и MatrixMinimap.cpp.
  Upstream и original game/render/skin algorithms не редактировались.

## Исполненные проверки

| Gate | Реальное доказательство |
| --- | --- |
| HOST ASan/UBSan | Original PathToOutputFiles + production output provider + actual CFile Create/Write/OpenRead/Read. Unicode/non-BMP, slash conversion, реальная directory/file failure, double binding, invalid root, traversal/symlink, retained FolderItem, unwind/rebind. PASS; настоящая host filesystem, без native GPU/Init. |
| ARM64/QEMU | Тот же filesystem/CFile contract. PASS. Записанные payload bytes совпали с HOST и Windows. |
| HOST/ARM64 native Init prefix | Actual original CGame::Init выполняется до намеренно отказывающего ReadCaps fixture. Output binding уже доступен; два cleanup цикла снимают его и engine owners. Это prefix/rollback proof, не успешный GPU Init. |
| WINDOWS filesystem | Тот же original PathToOutputFiles/CFile round-trip с wchar16, включая кириллицу/non-BMP, и исходный Windows path passthrough без native binding. PASS. |
| WINDOWS HAL | Полный original Init/RobotPreload/stor.Load/PrepareMap/Draw/Deinit дважды с actual HAL и private output root. Наблюдались ровно по одному MAPCACHE_COLD_RENDER, MAPCACHE_STORED и MAPCACHE_LOADED. Первый Init реально нарисовал/сохранил кэш, второй реально загрузил его. |
| WINDOWS pixels | Каждый кадр: 1008 actual SDK draws, indexed=1006, primitives=111812. Оба PNG 800×600 совпали с прежним hash f6f0d25c1e7b2a194f6523f8c96b9a74f7b7385525e5bb5cf7696de7f09a52bb. Device/window/gamma восстановлены. |
| COMPILE/LINK | Полный devkitA64 native-entry ELF; 178 link inputs, 0 strong undefined. |
| SWITCH HARDWARE | NOT RUN / HARDWARE_PENDING. SD/devoptab runtime, deko GPU Init/upload/draw/readback/present и первая карта не проверены. |

Наблюдательные Windows harness/minimap copies находятся только в build.
Они добавляют output binding и три сообщения; original алгоритмы сохранены.
Windows outputs изолированы в уникальном native-output-* внутри
build/matrixgame/e2e527-output/windows-frame. Единственный cache artifact —
настоящий PNG 512×512 desert1_3e.cmap.515A225F;
SHA256 bd6fb237804767bce7ae0483a52a31ea2a679a294c570e0066a2a109d06b0af5.

ELF: build/matrixgame/e2e527-output/sdk/matrixgame-native-entry.elf

SHA256: 5cc8c45100554f202fbf26b6eca357caa14d9a767ee6adf373840367b7b9be06

Воспроизводимость: tools/matrixgame_output_checks.py prepare / host / arm64 /
windows / sdk; tools/matrixgame_output_entry_checks.py host / arm64;
tools/matrixgame_output_windows.py; tools/matrixgame_output_audit.py.
Результаты и точные commands/source hashes — в build/matrixgame/e2e527-output
и [evidence](matrixgame-e2e527-output-evidence.json).

## Где остаётся native цепочка

Source-traced guard CMinimap::RenderBackground -> PathToOutputFiles ->
PersonalLocation снят реальным provider. SDK standalone entry передаёт root;
original cache IO и соответствующий Windows GPU path реально исполнены.
На Switch сама последовательность original Init ещё не исполнялась, поэтому
первый фактически падающий native GPU метод пока НЕ УСТАНОВЛЕН.

Следующая работа — защитить original minimap target/depth/viewport/scene/resource
cleanup на native exception path и проверить необходимые cold/warm native
render/transfer paths. Их Windows исполнение не доказывает работу deko3d.

Остаётся точный известный guard отдельной release/reset ветки:
CMatrixMap::ReleasePoolDefaultResources -> CDeviceState(map_device::StateBackend())
-> map stored device state backend unavailable.
DekoDevice пока не реализует original StoredStateBackend; существующий native
CaptureState/lease другого уровня этот контракт не заменяет.
После него — полный native Init/первая карта/первый кадр, управление боем и
встраивание в основную игру с аппаратной приёмкой.

## Защищённая область

NRO не создавалась и не изменялась; commit/push не выполнялись. Upstream pin,
основная NRO, frozen комплекты, resources/robots.pkg/CFG, сейвы и аппаратные
входы проверяются hash audit. Новых Switch hardware tests нет.
P0, GI repeating-text causality, manual-publish ENOENT и исторические Switch hangs
остаются OPEN. Этот provider и host evidence их не закрывают.
