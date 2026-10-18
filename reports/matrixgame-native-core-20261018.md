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
