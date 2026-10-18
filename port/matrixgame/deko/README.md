# Первый deko3d geometry pass

Статус: compile/link prototype, **не GPU runtime PASS**. Это начало MG-3,
не готовый renderer MatrixGame. Стабилизационная NRO 3772031 не менялась.

Реализованы owned deko device/graphics queue, bounded vertex/index uploads,
offscreen RGBA8 + Z24S8, indexed triangles, базовый diffuse/light shader и GPU→CPU
readback после queue completion. Mesh upload копирует все 32 байта native SVOVertex
(position/normal/UV); CPU simulation не меняется. Mesh остаётся на GPU до замены,
а DrawFrame переиспользует command/uniform/readback allocations. SetMesh проверяет
размеры и finite fields до публикации новых буферов. DrawFrame проверяет все submesh
ranges/эффективные индексы до изменения commands/uniforms. Перед заменой/уничтожением
ресурсов ожидается GPU idle. Форматы/лимиты прототипа: target до 2048×2048,
до 65536 вершин и 1048576 triangle indices, uint16 index buffer.

Public mesh boundary не следует reinterpret_cast в SVOVertex*: разные C++ object
типы требуют memcpy или явного копирования полей. Layout подтверждён отдельным
настоящим MSVC x86 compile/run: vertex=32, normal offset=12, UV offset=24,
SVOIndices16=6. Target matrixgame-mesh-layout-probe в соседнем native-probe/CMakeLists.txt.

`MeshDraw` принимает column-major clip/normal matrices. D3DX row-vector conversion
остаётся задачей интеграции. Depth zero-to-one, origin upper-left; culling в первом
проходе выключен. Освещение пока базовое diffuse + ambient, UV передаётся в fragment shader и используется для RGBA8 sampling. Полная эквивалентность D3D9 lighting/materials **не заявляется**.

## Build

Из корня на Windows: `py -3 tools/matrixgame_deko_build.py`.
В devkitPro Linux container: `python3 tools/matrixgame_deko_build.py`.
Используется установленный devkitA64/libnx/deko3d/UAM. SDK не модифицируется.

Скрипт компилирует GLSL в настоящий DKSH через UAM, создаёт временный embed-header,
компилирует renderer и отдельный link probe, линкует ARM64 PIE ELF с libdeko3d/libnx.
Проверяются ELF machine, наличие реальных device/queue/indexed draw/readback symbols
и отсутствие unresolved strong symbols. build/matrixgame/deko/build-report.json
содержит scope, tool versions, source/artifact SHA-256 и размер. Непрошедшая попытка
инвалидирует прежний report PASS. Никакой NRO или второго hardware-кандидата нет.

Probe содержит две контролируемые triangle groups с разными base vertex/uniform color
и проверку red/blue coverage readback для
будущего физического запуска; **в текущей проверке main не выполнялся**. CPU/QEMU
не используется вместо deko GPU. Скомпилированный ELF не доказывает ни первый кадр,
ни GPU resource cleanup, ни FPS. Source и SDK примеры не включают закрытые ресурсы.

## Следующая интеграция

Нужны native texture formats/mipmaps и полный sampler state, подключение mesh/material
submission к engine MatrixLib, реальные
matrix/winding checks, stencil shadows, blend/fog/multi-texture stages, animation,
render targets, particles/UI, swapchain/передача экрана основной игре, profiling и
физический первый кадр. MG-2 engine Run, MG-4 gameplay и MG-5 campaign остаются
обязательными. Auto-win и software подмена 3D не используются.

API основан на установленном deko3d и официальном
[Primer](https://github.com/devkitPro/deko3d/blob/master/Primer.md).
Shaders компилируются offline [UAM](https://github.com/devkitPro/uam), а не на Switch.

## Native submesh ranges

`mesh_submission.hpp` точно переводит две native ветки SVOUnion: обычный TriStart*3
и отрицательный IBase, с добавлением shared buffer offsets. Сохраняются surface ID,
first index, signed base vertex, MinVertexIndex/NumVertices и количество индексов.
Overflow/negative extents отвергаются до GPU; индексный массив не переписывается.
Индексный buffer может содержать padding/неиспользуемые элементы; проверяются только
выбранные ranges. Это нужно для native shared-buffer addressing.

DrawFrame принимает до 256 упорядоченных groups одного uploaded vertex/index pool.
Кадр очищается и считывается один раз. Каждая group имеет отдельный 256-byte uniform
slot; последний transform не подменяет предыдущие. Валидация bounds пока выполняется
на CPU index shadow при каждом batch, GPU queue ждёт idle ради безопасного readback.
Это не принятый production performance путь; GPU timing и дальнейшее caching требуют
профиля. Превышение лимитов даёт ошибку, а не молчаливое отбрасывание геометрии.

Portable ASan/UBSan и ARM64/QEMU: 2048 vertex-selection comparisons, обе native branches,
negative base vertex, padding, out-of-range metadata и integer overflow controls.
Настоящий MSVC x86 probe дополнительно вызывает translator с native SVOUnion.
CMake/CTest target matrixgame-mesh-submission-test добавлен в native project.
Эти проверки не подтверждают GPU command execution или материал/порядок реальной сцены.

## RGBA8 textures и samplers

SetTextureRGBA8 принимает явный source pitch и slots 1..63. Upload сохраняет RGBA
байты/alpha и порядок строк, выравнивая отдельный staging pitch до 32 байт. Последняя
строка не обязана содержать trailing padding. Невалидные планы/размеры и перекрытие
source/staging отвергаются до записи. DDS/DXT, native BGRA conversion и mipmaps этим
не декодируются: boundary принимает уже RGBA8 pixels.

Реализованы owned deko images/descriptors, buffer-to-image transfer, sampler bindings
и diffuse×texture shader. Каждая group выбирает image slot и один из четырёх samplers:
nearest/linear × repeat/clamp. Slot 0 — внутренний белый identity для явно untextured
pass. Missing/cleared slot и неизвестный sampler дают ошибку; отсутствующие native
assets не подменяются этим identity. Полный D3D9 texture-stage/anisotropy/mip state ещё
не перенесён.

Upload budget 64 MiB учитывает retained old textures + new image + staging одновременно.
При нехватке места старый slot сохраняется. ClearTexture и замена ждут queue idle;
перед следующими draws инвалидируются image/descriptor caches. Это implemented
lifecycle; native GPU leaks/OOM/replacement ещё требуют физической проверки.

Portable ASan/UBSan и ARM64/QEMU проверили 156 row layouts, byte/alpha preservation,
source immutability, zero staging padding, tail guards, short rows, overflow и alias
rejection. GPU shaders и ELF связаны с настоящими copyBufferToImage/bindTextures.
Future physical probe содержит две textures с alpha 128/255 и разные sampler/group
bindings. Main не выполнялся: sampling, alpha output и descriptor lifetime НЕ PASS.

## Данные настоящих VO

`vector_submission.hpp` соединяет нативный `VectorGeometry` с SetMesh/DrawFrame.
Передаются все исходные вершины/индексы и surface-first frame ranges; material state
задаёт вызывающий код. Это один явный pass, без подстановки недостающих материалов.
Build script использует CMake и линкует настоящий STRG/VO loader в GPU ELF.

Probe имеет необязательный `--geometry-only <standalone-VO>` для будущей физической
проверки первого кадра без текстур. Его readback ещё не выполнялся. Существующий
контролируемый texture probe остаётся default режимом. Новая NRO не выпускается.
Проверка всех 431 оригинальных VO выполнялась отдельно на CPU/MSVC/ARM64, не на GPU.
См. [отчёт](../../../reports/matrixgame-native-vector-20261018.md).

## PNG из CFile/PKG

`png_texture.hpp` предоставляет UploadPngTexture: настоящий CBitmap/FilePNG
декодирует PNG в straight RGBA8, renderer копирует staging до возврата.
Probe `--png <path>` содержит nearest readback comparison для будущего запуска
на Switch. Сейчас подтверждены только SDK compile/link; GPU execution отсутствует.
Это не material/skin pipeline и не полный texture cache. См.
[отчёт PNG](../../../reports/matrixgame-native-bitmap-20261018.md).

## DDS mip chains

SetTextureAsset принимает bounded native TextureAsset и загружает все supplied
mips, включая оригинальные BC1/2/3 blocks. Явные mip samplers добавлены рядом с
прежними no-mip вариантами. Copy upload/readback использует byte pitch DkCopyBuf.
Future probe `--texture <source> <64x64.rgba>` сравнивает base-level GPU readback
с внешним oracle; hardware run пока отсутствует. Подробности и ограничения:
[отчёт DDS/STRG](../../../reports/matrixgame-native-texture-20261018.md).

`--texture-request <name>` в future probe связывает original query/sidecar flags,
CFile resolution, native Load и SetTextureAsset. Вывод scope: upload completion,
draw_executed=false, battle_executed=false; физический запуск отсутствует.
Alpha-test/blend skin passes ещё требуют интеграции. См.
[texture requests](../../../reports/matrixgame-native-texture-request-20261018.md).

## Per-draw raster state

MeshSubmission/VectorMaterial теперь несут MeshRasterState: alpha compare/reference,
blend factors/op, depth test/write и color mask. Поддерживаемые legacy register IDs
проверяются атомарно; unsupported states отвергаются. Каждый draw переустанавливает
полное поддерживаемое состояние в настоящем deko3d. Shader ABI не меняется.

MSVC/native/ARM64 snapshots сверены с actual Windows D3D9 NULLREF Set/Get. Это не
image proof. Future `--raster-state` содержит восемь GPU readback controls; он не
запускался. Original skin selection/combiners ещё не подключены. См.
[отчёт raster state](../../../reports/matrixgame-native-raster-state-20261018.md).

TextureAsset/TextureRequest в deko ELF теперь включают actual JPEG decoder.
Build report фиксирует SDK JPEG header/library hashes и проверяет libjpeg symbols.
Все 1507 CPU texture assets проверены отдельно; Horizon/GPU JPEG sampling не
исполнялся. См. [JPEG report](../../../reports/matrixgame-native-jpeg-20261018.md).
