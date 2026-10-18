# Оригинальная CPU-часть MatrixLib

Текущая библиотека содержит оригинальные CRC32.cpp, память/позицию CBuf и
нативную реализацию UTF-16 методов CBuf и исключений. Это ещё не полный
MatrixLib/MatrixGame. Подготовка проверяет SHA256 шести закреплённых upstream
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

LoadFromFile/SaveInFile сохранены неизменёнными в generated CBuf_file.cpp, который
пока не включён в CPU library: CFile/pack backend ещё нужно перенести. Подставной
файловой реализации нет. CStorage::CalcUniqID всё ещё требует явного UTF-16
хеширования; прочие потребители wchar_t, heap/trace, D3D, настоящие assets, engine
Run, rendering, campaign handoff и физические MG gates остаются незавершёнными.
