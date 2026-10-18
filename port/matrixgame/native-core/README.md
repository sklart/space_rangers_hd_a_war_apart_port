# Оригинальная CPU-часть MatrixLib

Текущая библиотека содержит оригинальные CRC32.cpp, память/позицию и файловые
методы CBuf, native UTF-16 методы, исключения, CFile, CStorage и BlockPar с настоящим
файловым backend. Это ещё не полный MatrixLib/MatrixGame. Подготовка проверяет SHA256 пятнадцати закреплённых upstream
файлов после CRLF→LF и создаёт копии в build directory; upstream не изменяется.

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
существующему bounded PKG reader. Пятнадцать pinned inputs проверяются перед
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
