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
проходе выключен. Освещение пока базовое diffuse + ambient, UV ещё не используется
для sampling. Полная эквивалентность D3D9 lighting/materials **не заявляется**.

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

Нужны textures/samplers, подключение mesh/material submission к engine MatrixLib, реальные
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
