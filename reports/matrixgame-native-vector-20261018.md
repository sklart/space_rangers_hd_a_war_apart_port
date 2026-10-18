# MatrixGame: данные VectorObject → deko3d — 18.10.2026

Статус: нативная загрузка полного набора CPU-данных VO и передача geometry/draw
ranges в deko3d. Это не завершённый перенос класса CVectorObject, skin renderer,
анимационного проигрывателя или планетарного боя. Full engine target по-прежнему
требует D3D9; MG-2…MG-5 и аппаратные проверки остаются открыты.

## Перенос данных

tools/matrixgame_vector_overrides.py проверяет LF-normalized SHA оригиналов:

- VectorObject.hpp: 41af7cf50ab96af88b57ca5101cdf1b27d25ce6326517d532056142912ee0b72
- VectorObject.cpp: 11ffb4eb842b5cdf6041d0ddc3bf25a124b06bdd8ac36f090b4c2ac6d9586303

Подготовка core теперь проверяет 23 inputs. VectorLayout.hpp извлекает настоящие
SVOVertex, SVOTriangle, SVOUnion, SVOKadr, SVOFrameIndex, SVONormal, SVOKadrEdge
и SVOGeometrySimple из pinned header; исходные GPL notices сохранены. Device/cache
headers и FVF selector в этот набор payload layouts не входят. Размеры и vertex
offsets проверяются static_assert на MSVC, Linux и ARM64.

VectorGeometry загружает через реальный CStorage/CFile:

- все вершины/нормали/UV и 16-битные индексы;
- кадры, frame vertex/group IDs, группы и рёбра;
- метаданные анимаций, signed frame times и frame IDs;
- matrix names/IDs/offsets и все матрицы;
- имена поверхностей, свойства с сохранением порядка/дубликатов;
- simple geometry, её bounds, вершины и треугольники.

Строки — length-delimited UTF-16LE code units без wchar_t casts и нормализации.
Вся загрузка строит локальный владеющий объект; исключение освобождает частично
созданные векторы. Проверяются stride, количество массивов, связанные диапазоны,
surface/frame IDs, effective indexed vertex ranges, edge byte offsets, simple
geometry sizes и finite coordinates. Нулевые/INT_MIN animation durations отвергаются.
Новые массивы/имена бюджетируются до выделения: 128 МиБ на логические owned данные
одной загрузки. Это не RSS limit: CStorage имеет отдельный бюджет, STL overhead,
временные буферы и копии моделей требуют отдельного учёта в будущем кэше.

У файлов без edges оригинальный loader не читает edge fields кадров. В 12 кадрах
реального набора они равны 0xbaadf00d. Поля сохранены побайтно; проверка edge ranges
выполняется при наличии edge payload. Неиспользуемые значения не нормализуются.

FrameRanges воспроизводит surface-first порядок оригинального CVectorObject::Draw,
сохраняя порядок union IDs внутри поверхности и повторные IDs. Stable sort даёт
этот порядок без surfaces × groups обхода. Обе ветви IBase/TriStart используют
существующий VectorObjectRange; индексы/вершины не переписываются.

## Подключение к GPU

deko/vector_submission.hpp передаёт реальные vertex/index spans в SetMesh, а frame
ranges — в MeshSubmission. Материалы задаются явно для всех поверхностей; отсутствующие
не заменяются белым материалом. Это один заданный skin pass. Настоящие SSkin stages,
дополнительные passes, textures decoding/cache и shadow state ещё не подключены.

tools/matrixgame_deko_build.py теперь собирает CMake target с настоящей CPU library,
а не отдельно renderer/test objects. ELF audit требует GPU draw/upload/readback
symbols и реализации VectorGeometry::Load/FrameRanges. Он не подтверждает выполнение.
Probe поддерживает явный --geometry-only <standalone-VO>: настоящий первый кадр,
его bounds, непрозрачный untextured diagnostic pass и readback. Этот режим не является
боем или проверкой текстур. Probe на консоли не выполнялся, NRO не выпускается.

## Доказательства

Независимый Python oracle читает robots.pkg и STRG/ZL03 без native loader, затем
сравнивает побайтно канонический dump всех перечисленных payloads/metadata и draw
ranges каждого кадра. Закрытые ресурсы/dumps остаются в ignored build.

robots.pkg SHA-256: 8a22f856a27a8937ef28ac61274ffdb85cc2ff865492bd4d3ddeb2a6054011f0.

| Область оригинального набора | Количество |
|---|---:|
| VO | 431 |
| Вершины | 569593 |
| Треугольники | 334803 |
| Кадры | 1913 |
| Группы | 1913 |
| Анимации (метаданные) | 483 |
| Matrix tracks | 142 |
| Модели с simple geometry | 269 |
| Максимум групп одного кадра | 16 |

MSVC x64, ASan/UBSan и ARM64/QEMU проверяют весь набор. Совпавший aggregate SHA-256
списка канонических dumps:
f65b0c70523ad49d2736bf954380c4f2b308092edda2c75a9f7b0f1e4ff538d4.

CI использует только контролируемые, неигровые fixtures: plain/compressed VO,
отсутствующие optional tables и unused edge fields; 12 malformed controls проверяют
ссылки, strides, overflow, NaN, нулевой duration, непарные columns и embedded NUL.
Это не замена локальной проверки настоящих 431 ресурсов.

Финальные локальные проверки: MSVC x64 11/11 CTest PASS, ASan/UBSan 11/11 PASS;
все 431 реальные VO совпали на MSVC, ASan и ARM64/QEMU. 3 валидных и 12 malformed
controlled cases проходят на этих платформах. Отдельный исправленный math contract
прошёл GCC 13 + TSan и ARM64/NEON без изменения эталона/допусков.

SDK build всех native CPU/API targets и deko probe PASS. nm --undefined-only
пусто для обоих новых ELF. Это локальные compilation proofs, не release artifacts:

- build/matrixgame/deko/matrixgame-deko-mesh-probe.elf:
  SHA-256 ecc652cc64b00e7be323528347a45b35d8f4d6ffa5e465b6240639ee47657316;
  GNU Build ID 476177f90012c0e80893ecd5893793cf4fdae8ed.
- build/mg-engine-boundary-switch/core/matrixgame-native-vector-link-probe.elf:
  SHA-256 62beb10350c74382918757340efe824d18b93b1ced667891e3d749cfeae979ed.

Build report с хешами исходников, pinned preparation provenance и shaders находится
в ignored build/matrixgame/deko/build-report.json. Принятых измерений GPU/FPS нет.

```sh
cmake -S port/matrixgame/native -B build/mg-vector-asan -DBRIDGE_SANITIZER=address -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/mg-vector-asan --parallel 2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ctest --test-dir build/mg-vector-asan --output-on-failure
python3 tests/probe_matrixgame_native_vector.py --binary build/mg-vector-asan/core/matrixgame-native-vector-test --output build/mg-vector-real --package /path/to/game/DATA/robots.pkg
bash tools/matrixgame_file_arm64.sh --vector --package /path/to/game/DATA/robots.pkg
python3 tools/matrixgame_deko_build.py
```

SDK compile/link, CTest и QEMU не доказывают GPU frame, FPS, battery, Init/Run,
AI/боевую логику, возврат в кампанию или исправление зависания после «Готово».
Причина аппаратного freeze остаётся ROOT CAUSE UNCONFIRMED. Кандидат 3772031
и основной checkout сохранены. Следующий функциональный шаг — lifecycle/native
cache и buffers CVectorObject с подключением настоящих texture/skin данных.
