# MatrixLib: начало переноса оригинальной CPU-части

Выполнено в изолированном worktree. Pinned MatrixGame не изменялся.

## Исправление

В подготовленной копии BaseDef.hpp обе декларации dword теперь uint32_t.
На LP64 исходный unsigned long расширяется до восьми байт. Реальный CRC32.cpp
компилируется без изменения алгоритма; никакого подставного CRC/engine нет.
Подготовка проверяет содержимое трёх upstream files перед записью результата,
нормализуя только CRLF/LF, и сохраняет source-provenance.json.
CMake target matrixgame-native-core собирает оригинальный CRC в статическую
библиотеку. Это только одна CPU translation unit, не полный MatrixLib.

## Проверки

- Оригинальный MSVC x86 CRC: 4097 длин, 16388 split checks, transcript 7a4520ff.
- Исправленный host ASan/UBSan: те же результаты, без ошибок sanitizers.
- Исправленный ARM64 Linux/QEMU: те же результаты.
- devkitA64: оригинальный CRC собран в Switch object, не выполнен на Switch.
- CRC обновляется по независимой побитовой reference implementation и частями;
  размер пары дисковых dword статически проверен: 8 bytes.
- CI дополнен ASan/UBSan и ARM64 checks; новая версия ещё не завершила CI.

Локальные доказательства: build/matrixgame/native-core/{asan-result.json,
arm64-result.json,asan-build.log,devkit-compile.log,proof.json}; x86 результат:
build/matrixgame/deko/crc-x86-result.json. Исходники и бинарники оригинальных
ресурсов в Git не добавлялись, новая NRO не создавалась.

## Остаётся

UTF-16 ABI не закрыт. CBuf::WStrLen/WStrTextLen шагают по два байта и читают
wchar (четыре байта на ARM64); CBuf::WStr копирует std::wstring как UTF-16 bytes;
CStorage::CalcUniqID считает bytes через sizeof(wchar). Без явного UTF-16 codec
получится несовместимый формат. Глобальный -fshort-wchar нельзя считать готовым
решением совместимости со стандартной библиотекой.
Также остаются Win32 memory/file/trace dependencies, реальная сборка остальных
MatrixLib/MatrixGame units, asset/material submission, engine Run, GPU execution,
кампания и аппаратные MG gates. Причина замершего экрана после «Готово» этим
изменением не установлена; физический прогон кандидатa 3772031 пользователь отложил.


## Продолжение: настоящий CBuf и UTF-16LE

CPU library расширена оригинальным CBuf.cpp (память, размер, позиция, narrow text),
нативными UTF-16 методами того же класса и реализацией исходного exception API.
Шесть входных файлов проверяются по LF-нормализованным SHA256. dword остаётся
32-битным, wchar_t ABI не меняется; disk boundary теперь явно UTF-16LE.

PASS: 514 byte-exact наборов против оригинального MSVC x86 CBuf. Наборы включают
ASCII/кириллицу/CJK/emoji, все 65536 UTF-16 units, surrogate-пары и одиночные
surrogates, адреса без выравнивания, NUL/CR/LF и исходное продвижение позиции.
Transcript 89542cb4 совпадает на x86 original, host ASan/UBSan, ARM64 QEMU и MSVC x64
native. Повреждённые строки, чтение за границей и size overflow отвергаются без
изменения cursor/output. Scalar Get/Add/WordLoop переведены на memcpy;
TestGet/TestAdd/Expand/WordLoop проверяют переполнения без изменения обычного роста.

CMake с новым Switch toolchain собрал статическую devkitA64 library из настоящих
CRC/CBuf и native UTF-16/exception units. Тест также скомпилирован devkitA64 в object.
Это compile/archive proof, не аппаратное выполнение и не полная сборка движка.
CI дополнен ARM64 buffer contract и devkitA64 library build; новый commit ещё не
завершил удалённый CI.

LoadFromFile/SaveInFile вынесены без изменения в generated CBuf_file.cpp и ещё
не подключены к CPU library: неготовый CFile не заменялся фиктивной реализацией.
Native exceptions выводят file/line/message; Win32/x86 stack Tracer не портирован.
CStorage name hashing и другие UTF-16 потребители остаются OPEN. Стабилизационная
NRO 3772031 не менялась; freeze после «Готово» не объявляется исправленным.

Доказательства: build/matrixgame/native-core/buffer-{x86,asan,arm64}-result.json,
buffer-asan.log, buffer-msvc.log, buffer-devkit-cmake.log и buffer-proof.json.
