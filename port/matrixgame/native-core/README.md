# Оригинальная CPU-часть MatrixLib

Это начало переноса настоящего MatrixLib, а не замена движка. Пока библиотека
содержит только оригинальный `Base/CRC32.cpp`; его алгоритм не изменён. Подготовка
проверяет SHA256 трёх входных файлов закреплённого MatrixGame (после CRLF→LF),
затем создаёт копии в build directory. В копии BaseDef.hpp обе декларации dword
заменяются на std::uint32_t. В pinned submodule ничего не записывается.

На Win32 unsigned long имеет 32 бита, на LP64 ARM64 — 64. Фиксированная ширина
нужна для исходной арифметики CRC и бинарных полей. Нестандартное сочетание
таблицы CRC и сдвига влево намеренно сохранено: подмена стандартным zlib CRC
нарушила бы совместимость.

```sh
cmake -S port/matrixgame/native-core -B build/mg-core-asan \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCORE_SANITIZERS=ON
cmake --build build/mg-core-asan --parallel 2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
  ctest --test-dir build/mg-core-asan --output-on-failure
```

Эталон x86: target matrixgame-crc-x86-reference проекта native-probe собирает тот
же тест с неизменённым оригинальным CRC32.cpp/BaseDef.hpp. Для 4097 длин и 16388
split checks FNV32 последовательности CRC равен 7a4520ff. ARM64/QEMU и host
ASan/UBSan совпали; devkitA64 собирает исходный CRC в объект для Switch.

Это не сборка всего MatrixLib или MatrixGame, не engine Run, не GPU/аппаратный
тест. wchar_t не меняется: UTF-16 сериализация CBuf/CStorage требует отдельного
переноса. Например, CBuf::WStrLen шагает по два байта, но читает wchar; на LP64
это четыре байта. CStorage::CalcUniqID хеширует имена с sizeof(wchar), поэтому
исправление только typedef без проверки строкового и дискового ABI недостаточно.
Также остаются Win32 file/heap/trace dependencies и Direct3D.
