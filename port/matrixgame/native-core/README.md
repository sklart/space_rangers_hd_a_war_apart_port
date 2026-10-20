# Оригинальная CPU-часть MatrixLib

Текущая библиотека содержит оригинальные CRC32.cpp, память/позицию и файловые
методы CBuf, native UTF-16 методы, исключения, CFile, CStorage и BlockPar с настоящим
файловым backend, а также оригинальный Math3D.cpp с настоящими DirectXMath CPU operations.
Подключён original RNG с Windows-compatible потоком. Это ещё не полный MatrixLib/MatrixGame.
Подготовка проверяет SHA256 всех входных upstream-файлов из `INPUTS` в
`tools/matrixgame_core_prepare.py` после CRLF→LF и создаёт копии в build directory;
upstream не изменяется.

E2E-5.15 также включает оригинальные CMain.cpp и Form.cpp: BaseInit/BaseDeInit,
список форм, StaticInit и FormChange. Снята только зависимость от пустой release
трассировки; Windows debug/trace режимы явно отвергаются. Это ещё не CGame Init/Run.
100 quiescent lifecycle циклов проверены на sanitizer host, Windows x86 reference
и Linux ARM64/QEMU; SDK ELF compile/link отдельно. `matrixgame-native-lifecycle-test`
зарегистрирован в CTest. Навигационный тест настоящей Boarding.cmap теперь начинает
ресурсную цепочку через BaseInit. Артефакты и ограничения — в
`reports/matrixgame-e2e515-status.md`, metadata lifecycle/matrixlib JSON рядом.

Обе декларации dword используют uint32_t. Исходный нестандартный алгоритм CRC
сохранён. wchar_t и стандартная библиотека сохраняют платформенный ABI: строки
на диске преобразуются через явный UTF-16LE codec. Корректные surrogate-пары и
отдельные surrogate code units проходят байтовый round-trip без замены символов.
CBuf::WStr/WStrNZ/WStrText и методы длины подключены к этому codec. Отсутствующий
терминатор и неполная кодовая единица отвергаются до продвижения позиции.

Скалярные Get/Add/WordLoop используют memcpy для адресов без выравнивания.
TestGet/TestAdd/Expand/WordLoop проверяют размеры без переполнения; исходный
алгоритм роста буфера сохранён. Нативные CException/CExceptionStr сохраняют тип
и сообщение/file/line. Win32/x86 Tracer не подключён: stack trace здесь не обещан.

```sh
cmake -S port/matrixgame/native-core -B build/mg-core-asan \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCORE_SANITIZERS=ON
cmake --build build/mg-core-asan --parallel 2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
  ctest --test-dir build/mg-core-asan --output-on-failure

cmake -S port/matrixgame/native-core -B build/mg-core-switch \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/port/matrixgame/native-core/switch-toolchain.cmake" \
  -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build/mg-core-switch --parallel 2
```

Для SDK в нестандартном месте передайте -DMATRIXGAME_DEVKITPRO=/path/to/devkitpro.
Switch target создаёт статическую библиотеку, без дополнительной NRO. Тесты
исполнялись на host ASan/UBSan, MSVC x64 и ARM64/QEMU; Switch archive только собран.

Оригинальные MSVC x86 reference targets в native-probe:
- matrixgame-crc-x86-reference: 4097 длин, 16388 split checks, transcript 7a4520ff;
- matrixgame-buffer-x86-reference: 514 наборов, включая все 65536 UTF-16 units,
  байтовый transcript 89542cb4. Валидные bytes/позиции совпали с native port.

LoadFromFile/SaveInFile сохранены неизменёнными в generated CBuf_file.cpp и
подключены к native CFile backend (см. ниже). Подставной файловой реализации нет. CStorage::CalcUniqID теперь явно хеширует UTF-16LE; прочие потребители wchar_t,
Win32 trace, D3D, engine
Run, rendering, campaign handoff и физические MG gates остаются незавершёнными.


## Нативный файловый backend

CFile и оригинальные LoadFromFile/SaveInFile CBuf теперь подключены к stdio и
существующему bounded PKG reader. Девятнадцать pinned inputs проверяются перед
подготовкой. Внутренний pimpl не включает Win32/D3D; shareMode на portable backend
advisory. FileExist сохраняет исходное написание разделителей. Package::OpenUtf8
нужен для явной Unicode границы; прежний Package::Open не меняет narrow-path режим.

```sh
bash tools/matrixgame_file_arm64.sh
# Тот же контракт с настоящим установленным ресурсом:
bash tools/matrixgame_file_arm64.sh \
  --package "/path/to/game/DATA/robots.pkg" --entry Matrix/Building/ruins/b0.vo
```

Runner использует закреплённый официальный zlib 1.3.2 commit, не меняя системные
пакеты. Проверки включают disk/Unicode/large-offset IO, corrupted stream recovery,
16-entry limit и четыре reader threads. Build option CORE_BUILD_LINK_PROBE=ON
добавляет только dependency-proof ELF к Switch static library; ELF не запускается
и дополнительная NRO не создаётся.

Подробности и незакрытые границы: reports/matrixgame-native-file-20261018.md.
CStorage подключён к CPU-библиотеке; остальные MatrixLib/engine units и реальный бой ещё не подключены.


## CStorage: настоящий формат STRG и ZL03

Подготавливается оригинальный CStorage.cpp с проверкой SHA исходников. Изменения
касаются переносимости, владения памятью и проверки границ; заглушек загрузчика нет.
Дисковая структура остаётся little-endian с 12-байтными packed-заголовками;
ST_INT32/ST_DWORD/ST_FLOAT/ST_DOUBLE/ST_BYTE/ST_WCHAR и CalcUniqID сохраняются.
WCHAR-данные и имена кодируются явно в UTF-16LE при обычном ABI wchar_t платформы.

Перемещение записей заменяет realloc/memcpy объектов со std::wstring. Неудачная
загрузка не заменяет прежние записи. Проверяются счётчики, смещения, типы, массивы,
полнота deflate-блоков и отсутствие хвостовых байтов. Исправлены очистка compressed
флага колонки и продвижение входного курсора. ZL03 encoder использует compressBound
для короткого последнего блока. Decoder выделяет выход один раз, избегая удвоения
capacity при последовательном расширении vector. Размер файла проверяется до чтения.

Лимит 256 MiB ограничивает вход, распаковку и оценку новых структур загрузки;
учитывается Capacity входного/распакованного CBuf. Это не общий лимит RSS: существующее
состояние CStorage при транзакционной замене, allocator overhead и память вызывающей
стороны в этот бюджет не входят. Оценка строк консервативна, но не заменяет глобальный
учёт allocator. В typed-pointer API требуется выравнивание; GetValue/SetValue используют
memcpy и поддерживают double по исходному смещению 12. Сериализованный формат не меняется.

```sh
bash tools/matrixgame_file_arm64.sh --storage
bash tools/matrixgame_file_arm64.sh --storage \
  --package "/path/to/game/DATA/robots.pkg" --entry Matrix/Building/ruins/b0.vo
```

Доказательства и оставшиеся границы: reports/matrixgame-native-storage-20261018.md.
CStorage_blockpar.cpp с исходными StoreBlockPar/RestoreBlockPar теперь компилируется
вместе с CBlockPar.cpp; подробности следующего этапа ниже. Ни настоящий
engine Run, ни GPU execution, ни MG0–MG5/возврат в кампанию этим контрактом не доказаны.


## BlockPar и конфигурация движка

Оригинальный CBlockPar.cpp подключён с explicit UTF-16LE/UTF-8 boundary и без Win32
Tracer/DTRACE instrumentation. Грамматика, комментарии, повторы имён, пути, include
и правила числового разбора сохранены. GetInt/GetDword/GetHexUnsigned используют
исходные UTF-16 units и 16/32-bit wrap, без signed-overflow UB и зависимости от
знаковости wchar_t. Проверка всех 65536 units + составных значений совпадает с
исходным MSVC x86: transcript fbe5b145ba008067.

LoadFromTextFile различает UTF-16 BOM и прежний UTF-8 путь, ограничивает размер
перед чтением и сохраняет поведение пустых/BOM-only файлов. SaveInTextFile выдаёт
тот же UTF-16LE/UTF-8, что оригинал. Рекурсивный разбор/Store/Restore ограничен
128 активными уровнями; Restore проверяет наличие и парность колонок. Это защита
от stack overflow, а не полная валидация грамматики или глобальный memory budget.

Настоящие CFG/Rus/robots.dat и CFG/Eng/robots.dat восстановлены в деревья da/if.
Независимый Python traversal исходного STRG и оригинальный x86 parser подтвердили
все имена, значения, порядок и счётчики на host ASan/UBSan, MSVC x64 и ARM64/QEMU.
Полный engine CGame::Init/Run на ARM64 и рендер этим не подтверждены.

```sh
bash tools/matrixgame_file_arm64.sh --blockpar
bash tools/matrixgame_file_arm64.sh --blockpar --config /path/to/game/CFG/Rus/robots.dat
```

Подробные digests и незакрытые границы: reports/matrixgame-native-blockpar-20261018.md.


## Math3D: CPU-математика вместо D3DX9

Оригинальный Math3D.cpp теперь компилируется на host/ARM64/SDK с математическими
D3DXVECTOR2/3, D3DXMATRIX и D3DXPLANE types, сохраняющими фиксированную раскладку.
Операции dot/cross/normalize/inverse/plane реализованы через официальный DirectXMath;
никаких D3D device/resource заглушек этот backend не предоставляет. SIMD selection
проверен как SSE на host и NEON при реальном Linux ARM64 execution под QEMU.
DirectXMath/SAL headers, MIT licenses и exact commits/hashes находятся в third_party.

Убраны LP64/strict-aliasing нарушения битовых float helpers. Синхронизированы
раздельные plane fields, исправлены инициализация heap, кэш повторного Init и
перекрывающийся copy/cut траекторий. Native-only инварианты этих исправлений не
выдаются за совпадение с ошибочным legacy behavior. Viewport globals остаются
внешним состоянием настоящего движка, а не реализованным здесь окном.

```sh
bash tools/matrixgame_file_arm64.sh --math
bash tools/matrixgame_file_arm64.sh --math \
  --package /path/to/game/DATA/robots.pkg --entry Matrix/Building/ruins/b0.vo
```

Asset-free fixture содержит original x86/D3DX9 numeric trace и metadata; игровых
ресурсов в нём нет. CI сравнивает 48281 float + 2824 integer values с этим эталоном.
Float comparison сохраняет исходные алгоритмы с ограниченной численной погрешностью,
а не требует побитного совпадения разных реализаций D3DX/DirectXMath. Чувствительные
real-geometry ray cases проверяются отдельно аналитически и с original trace.

Подробности и границы: reports/matrixgame-native-math3d-20261018.md.
Native RNG подключён следующим этапом (см. ниже); cache/VO loader, CGame::Init/Run,
rendering и campaign handoff ещё не подключены. Scalar/SIMD-настройки не являются измеренным FPS/battery gain.


## RNG: последовательность оригинального Windows CRT

Подключены закреплённые random.cpp/random.hpp. Native namespace matrixgame_random
избегает конфликта с POSIX ::random(); global RND/FRND/FSRND/IRND сохраняют имена.
Для будущих prepared engine units вызовы random:: требуют явного переименования.

Seed/state/result воспроизводят UCRT per-thread stream: uint32 modulo arithmetic,
15-bit raw output и начальное состояние 1 на каждом потоке. Нативный stream
изолирован от srand/rand libc вызывающего процесса. В pinned engine прямые вызовы
rand/srand находятся только внутри original random.cpp. Подмена формул качества
или распределения не выполняется: сохранены маленький RndFloat и неявное integer
усечение аргумента IRND, включая consumption при equal/reversed ranges.

Оригинальный x86, native ASan/UBSan host, MSVC x64 и ARM64/QEMU выдали один
побайтный transcript для 260 seed, 603173 значений и четырёх потоков:
2005e75f84d8a3ed429f34ceb38da48b4be9ae47250b009bd4690cf6bedef1e5.
Independent Python oracle проверяет каждый byte, а SHA original trace закреплён в CI.

```sh
bash tools/matrixgame_file_arm64.sh --random
```

Большие integer intervals вычисляются без signed-overflow/modulo-zero; undefined
FSRND/IRND float-to-int conversion отвергается. Это определённое native поведение
на границах, где original source не задаёт безопасного результата. Эквивалентность
произвольных external libc interleavings или всего игрового состояния не заявляется.
Подробности: reports/matrixgame-native-random-20261018.md.

## VectorObject: нативные данные и draw ranges

`vector_geometry.hpp` владеет данными реального VO: vertices/indices, frames/groups,
edges, animation metadata, matrices, surfaces, properties и simple geometry.
Payload layouts извлекаются из pinned VectorObject.hpp. Загрузка использует настоящий
CStorage/CFile, проверяет связанные диапазоны и бюджетирует owned data до выделения.
Строки сохраняются как length-delimited UTF-16LE.

FrameRanges сохраняет surface-first порядок оригинального Draw, включая повторные
group IDs. `deko/vector_submission.hpp` подключает данные к GPU API с явно заданными
материалами одного pass. Старый CVectorObject, cache, texture/skin state, проигрыватель
анимаций и полный движок ещё требуют интеграции. Оригинальные 431 VO проверяются
локально; CI использует только контролируемые неигровые fixtures.

`bash tools/matrixgame_file_arm64.sh --vector` выполняет ARM64/QEMU contract.
Отчёт: [native VectorObject](../../../reports/matrixgame-native-vector-20261018.md).

## Ресурсный SRemindCore

Оригинальные CReminder.hpp/CReminder.cpp включены в native core. Порядок проверок,
Use/Down и idle clock совпадают с x86 на 5000 операциях. Внутренние часы используют
int64. Callback owner отслеживается отдельно от global current: собственный Down
или delete не снимает следующий core. Исключения не маскируются; reentry/live reset
отвергаются. Scheduler предназначен для одного engine/render thread.

`bash tools/matrixgame_file_arm64.sh --reminder` проверяет ARM64/QEMU contract.
Границы и original negative controls описаны в
[отчёте](../../../reports/matrixgame-native-reminder-20261018.md).

## CBitmap и PNG

Native core компилирует оригинальный CPU CBitmap и FilePNG API через libpng.
LoadRGBAFromPNG создаёт straight RGBA8 для native GPU upload, сохраняя classic
LoadFromPNG formats. Handles имеют uintptr_t; cleanup выполняется при decode
и allocation errors. Проверяются размеры, pitch и per-buffer memory budgets.
WinBitmap/GDI, JPEG/DDS decode и texture/material cache ещё не перенесены.

`bash tools/matrixgame_file_arm64.sh --bitmap` проверяет independent pixel oracle.
Все 617 PNG оригинального robots.pkg проверены отдельно на host/ARM64; CI
использует controlled fixtures. Подробности и границы:
[отчёт CBitmap](../../../reports/matrixgame-native-bitmap-20261018.md).

## DDS/STRG texture assets

`texture_asset.hpp` загружает exact CFile/PKG path и декодирует PNG, legacy DDS или
STRG 0/0 byte payload. DDS BC1/2/3 сохраняют исходные blocks и все supplied mips;
alpha/luminance/RGB masks преобразуются в RGBA8. Loader не подменяет неизвестные
форматы. Это ещё не original CBaseTexture/CCache/material semantics.

`bash tools/matrixgame_file_arm64.sh --texture` проверяет mip payload и staging
oracle. Реальные 813 DDS/STRG проверяются отдельно от CI. См.
[отчёт texture assets](../../../reports/matrixgame-native-texture-20261018.md).

## Original texture requests

TextureRequest разрешает имя через CFile, выполняет извлечённый original
CBaseTexture::ParseFlags (query + sidecar CBlockPar) и действительно загружает
TextureAsset. Compressed semantics проверяются до STRG decode. Original x86 и
native сверяются на реальных filename/extensionless requests; JPEG остаётся
неподдерживаемым. Это ещё не CCache factory и skin shader state.

`bash tools/matrixgame_file_arm64.sh --texture-request` выполняет controlled
contract. См. [отчёт](../../../reports/matrixgame-native-texture-request-20261018.md).

## JPEG textures

TextureAsset декодирует реальные JPEG через bounded libjpeg в RGBA8; TextureRequest
теперь загружает JPEG после original filename/sidecar resolution. Baseline/progressive
RGB/gray поддерживаются, CMYK/12-bit/ошибочные входы отвергаются. Codec/RGBA/input
budgets локальны и не означают общий RSS cap. Progressive virtual arrays учитываются
до realization. Controlled JPEG fixtures проверяются без Pillow в CI.

Все 77 игровых JPEG и 24 controls сверены с actual D3DX decode. Общий real asset
replay теперь охватывает все 1507 PNG/JPEG/DDS/STRG. См.
[отчёт JPEG](../../../reports/matrixgame-native-jpeg-20261018.md).
