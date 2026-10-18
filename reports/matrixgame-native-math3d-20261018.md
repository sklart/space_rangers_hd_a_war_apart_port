# MatrixGame: оригинальная CPU-математика Math3D — 18.10.2026

Статус: Math3D.cpp компилируется в native CPU library. Это необходимая часть камеры,
пересечений и траекторий, но не запуск CGame::Init/Run или самого боя.

## Реализация и происхождение

Подготовка проверяет 17 MatrixLib inputs, включая исходные:

- Math3D.hpp: f444325e3bfea812f94c8b2c1e8d06670686af65675c9ba6cad762b2278c7b14
- Math3D.cpp: ce5c5be94a6cbce29ee766922c43d70940590541a639b0268638a6aa3fe21804

Те же SHA подтверждены у исходника фактически собранного MSVC x86 reference.
Алгоритмы Math3D остаются исходными; D3DX math API получает настоящие CPU operations
через [официальный DirectXMath](https://github.com/microsoft/DirectXMath).
D3D device/resource/window методов или заглушек здесь нет.

DirectXMath commit: 49696c31d8f4446798c763ddbfc7434a4a5a1084.
SAL annotation header из dotnet/runtime: ec0059c08c077e839c6eb8827c846530a891d7e2.
Headers и MIT licenses vendored для offline build; exact LF-normalized SHA находятся
в port/matrixgame/third_party/math-provenance.json и проверяются при подготовке.
Граница SAL на GNU не предоставляет runtime API. Исходные submodules не изменены.

D3DXVECTOR2/3: 8/12 bytes, D3DXMATRIX: 64 bytes, D3DXPLANE: 16 bytes.
У vector3/matrix align=4, translation row offset=48. Сохранены row-matrix semantics.
Векторы/матрицы не требуют Win32 wchar/pointer ABI; внутренний CTrajectory имеет
обычный ABI целевой платформы и не выдаётся за исходный x86 object layout.

Устранены 64-bit/strict-aliasing нарушения float bit helpers, включая чтение
unsigned long из 4-byte float. SPlane builders синхронизируют раздельные dxplane,
norm/dist, UpdateSignBits синхронизирует обратное направление. CTrajectory получает
инициализированный heap, запрещённый owning copy, безопасный memmove при перекрытии,
обновление segment count после успешного allocation и сброс length cache при Init.
Terminal CutBefore сохраняет последний сегмент вместо первого. Empty/invalid-count
путь и singular CalcPick отвергаются. Plane/cache/cut и invalid-count пути отдельно проверены native инвариантами;
совпадение с ошибочным/undefined legacy поведением не обещается.
Win32 DTRACE instrumentation исключена из подготовленной копии, не заменяя логику.

## Original reference и точность

Asset-free original x86/D3DX9 control trace: **48281 float + 2824 integer values**.
Охвачены vector operations, matrix inverse, viewport picking, triangle/line/sphere
intersections, projection/distance, table trigonometry, color/bit helpers и
инициализация/движение/продолжение двух видов траектории.

Fixture: tests/fixtures/matrixgame/math3d-x86-control.txt.zlib; metadata рядом.
Uncompressed trace SHA256:
681b07790b445fe8078df971feb8294cb0455b6c2a865ff3d8b251d34effc5a3.
Fixture integrity и длина проверяются до сравнения. Настоящих игровых данных в Git
fixture нет. Integer results сравниваются точно. Для float контролируется
abs(native-reference)/max(1,abs(reference)) <= 2e-5.

| Среда | Максимальная scaled ошибка control | Максимальная absolute ошибка |
|---|---:|---:|
| ASan/UBSan host, DirectXMath SSE | 2.861e-6 | 7.6e-6 |
| MSVC x64, DirectXMath SSE | 2.861e-6 | 7.6e-6 |
| Linux ARM64/QEMU, DirectXMath NEON | 2.394e-6 | 6.2e-6 |

NEON branch подтверждён выполнением на ARM64/QEMU, не только компиляцией. Native
GNU compile использует -ffp-contract=off, MSVC /fp:precise. Это ограниченное численное
сопоставление, не универсальная побитная эквивалентность D3DX9 и DirectXMath.

## Настоящая геометрия

Matrix/Building/ruins/b0.vo: 3512 вершин, 128 ray/triangle попаданий из реальных
vertex/index buffers. Source SHA256:
de47bf4b533f2e179d85dae860740d63923f8e339ad15ca43b4cb4fa27e88ed6.
Vertex FNV32 c23062f8, index FNV32 2e244f14: подтверждены независимым Python PKG/STRG
reader. Опция include_payloads у storage oracle не меняет его прежний default output.

Лучи построены через центры треугольников с ожидаемыми t=32, u=v=1/3; вырожденные
треугольники отсеиваются по площади. Это 128 проверок геометрии, а не весь VO loader.
Узкие треугольники имеют отдельную float error оценку: исходный x86 уже отклоняется
от 1/3 до 2.9872e-5. Поэтому допуск этих случаев 5e-5, при неизменном control допуске.

| Среда | Max barycentric error относительно 1/3 | Max delta от original trace |
|---|---:|---:|
| Original x86 | 2.9872e-5 | 0 |
| ASan host / MSVC x64 | 7.0534e-6 | 2.9117e-5 |
| ARM64 NEON | 1.5329e-5 | 2.9862e-5 |

Host ASan/UBSan и MSVC x64: **6/6 CPU contracts PASS**.
SDK devkitA64/libnx: Math3D contract ELF linked, undefined symbols **0**,
Build ID **171742b6950fae0efc8d8dcef45bed85cd9c63ab**. ELF не запускался на консоли,
NRO не создавался, аппаратный кандидат 3772031 не заменён.

## Воспроизведение и границы

```sh
cmake -S port/matrixgame/native-core -B build/mg-core-asan \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCORE_SANITIZERS=ON
cmake --build build/mg-core-asan --parallel 4
ctest --test-dir build/mg-core-asan --output-on-failure
bash tools/matrixgame_file_arm64.sh --math
bash tools/matrixgame_file_arm64.sh --math \
  --package /path/to/game/DATA/robots.pkg --entry Matrix/Building/ruins/b0.vo
```

Full original trace принимается runner через --reference /path/to/math-trace.txt.
Локальные proof folders: math-x86-fixtures, math-real-asan, math-msvc-fixtures,
math-arm64-final под build/matrixgame/native-core. В CI добавлен asset-free NEON
contract, existing sanitizer suite и SDK build; remote CI проверяется отдельно.

Native RNG, cache, полный VO/map loader, CGame::Init/Run, GPU execution/swapchain,
текстуры/анимация/шрифты/звук и campaign handoff остаются открыты. Не обещана
эквивалентность произвольного NaN/out-of-domain input, полная allocation-failure
проверка или глобальный RSS budget. MG0–MG5, физическая Switch, freeze после «Готово»
и FPS/battery требования этим этапом не закрыты. Наличие NEON не считается замером
ускорения или расхода батареи.
