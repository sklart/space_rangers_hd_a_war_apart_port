# MG-3: первый deko3d geometry pass, 18.10.2026

Статус PARTIAL / COMPILE-LINK ONLY / HARDWARE PENDING. Стабилизационная NRO
3772031 и её matching ELF не изменены. Новый NRO не выпускался.

Реализован отдельный `port/matrixgame/deko/mesh_renderer.cpp`: owned device/queue,
bounded vertex/index upload, RGBA8 + Z24S8 target, uniform/shader state,
indexed triangles и GPU→CPU readback после queue completion. GPU mesh allocations
сохраняются между Draw; command/uniform/readback memory переиспользуется. Ресурсы
ожидают idle перед заменой/уничтожением, command buffer уничтожается до backing memory.
Это исходный код intended lifecycle; фактический GPU cleanup пока не измерен.

Два GLSL shader скомпилированы установленным UAM 1.1.0 в настоящие DKSH. Renderer
и link probe связаны devkitA64 16.1.0 в ELF64 AArch64 PIE с libdeko3d/libnx.
Machine, реальные dkDeviceCreate/dkQueueCreate/dkCmdBufDrawIndexed/
dkCmdBufCopyImageToBuffer symbols и отсутствие unresolved strong symbols PASS.
Source/shader/library/ELF SHA-256 и размеры: build/matrixgame/deko/build-report.json.
Скрипт tools/matrixgame_deko_build.py повторяем на native Windows Python и Linux SDK;
ошибка инвалидирует прежний report PASS. Generated shader header/binaries только в build.

Нативный MSVC x86 layout probe включает настоящий VectorObject.hpp из чистого
snapshot pin. Проверены: SVOVertex=32, normal offset=12, UV offset=24,
SVOIndices16=6. Compile/run exit=0. Snapshot header побайтово совпадает с pinned
upstream header. Proof/log: build/matrixgame/deko/x86-layout-{proof.json,build.log,log}.

Physical link probe main НЕ выполнялся. Он содержит geometry fixture для будущей
проверки readback; не настоящий MatrixGame mesh/map, не победа и не бой.
GPU первый кадр, winding/matrix/material parity, runtime lifetime и frame times
остаются непроверенными. Позднейшая интеграция обязана сохранять native submesh
ranges/base vertex/material dispatch; текущий проход рисует один triangle-list mesh.

Полные textures/samplers, stencil shadows, blend/fog/multitexture, animation,
render targets, particles/UI, swapchain и handoff основной игре ещё не реализованы.
Базовый diffuse/ambient shader не выдаётся за эквивалент всего D3D9 fixed-function.
MG-2 engine entry/Run, MG-4 gameplay и MG-5 campaign остаются открыты.

Доступны deko3d и OpenGL библиотеки SDK. deko3d рассматривается как предварительный
backend для прототипа; выбор не объявлен подтверждённым до измерений/первого кадра.
Использованы официальный [deko3d Primer](https://github.com/devkitPro/deko3d/blob/master/Primer.md)
и [UAM](https://github.com/devkitPro/uam); SDK/examples и закрытые assets в Git не копировались.
В существующий full ARM64 CI добавлен только compile/link step, без дополнительной NRO.
Remote результат новой ветки пока отсутствует.
