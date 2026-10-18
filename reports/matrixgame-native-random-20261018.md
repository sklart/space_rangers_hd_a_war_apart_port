# MatrixGame: Windows-compatible RNG — 18.10.2026

Статус: original random.cpp/random.hpp подключены к native CPU library.
Это реальная реализация игрового генератора, не CGame::Init/Run и не игровой бой.

## Контракт и перенос

Проверяются 19 MatrixLib inputs. Original LF-normalized SHA:

- random.cpp: 9c5ef914ad896f214be468babd49304548613743b7c533c200fb64cbf0b1cdfd
- random.hpp: b169672fd41650a3d8d090e34a31a64aa5fd7607bb22406f496808fb704901a8

Windows контракт проверен по установленному UCRT SDK source
C:/Program Files (x86)/Windows Kits/10/Source/10.0.26100.0/ucrt/stdlib/rand.cpp
и internal/per_thread_data.cpp. Алгоритм/state подтверждены запуском original x86
MatrixLib random.cpp; сам SDK source в репозиторий не копируется.

Native stream: uint32 state, seed задаёт state, следующий state вычисляется modulo
2^32, raw result — верхние 15 bits после сдвига 16. State thread_local, initial=1.
Системные srand/rand вызывающего процесса не используются и не изменяются.

Исходные правила wrappers сохранены:

- RndFloat делит float(raw) на float(2147483647-2), а не на RAND_MAX. Диапазон этой
  функции остаётся маленьким, как на original Windows; баланс игры не нормализуется.
- Global RND использует original Windows RAND_MAX=32767 и abs span + from.
- Equal/reversed integer ranges потребляют тот же draw и сохраняют old defined outputs.
- FRND/FSRND сохраняют integer signatures и float rounding.
- IRND сохраняет усечение double(n)-0.55 в integer перед RND и round результата.

Native namespace matrixgame_random устраняет конфликт POSIX global ::random().
Global wrappers сохраняют имена. Math3D.hpp вновь включает random.hpp. Дальнейшая
подготовка остальных engine source должна переименовать их random:: calls; полностью
скомпилированный CGame::Init этим ещё не объявляется.

Широкий integer span вычисляется в 64 bits без overflow/modulo-zero. FSRND и IRND
отвергают результаты вне допустимого float-to-int преобразования. На old undefined
inputs это native safety extension, а не обещание воспроизвести UB.

## Проверки

Один C++ contract выполняется на unchanged original MSVC x86/UCRT и native core.
В нём: initial stream, 260 seed, 1024 raw draws на seed, 128 смешанных наборов
каждого wrapper, equal/reversed/negative ranges и четыре потока с default state,
индивидуальным reseed и контролем неизменности main-thread stream.

Independent Python oracle воспроизводит bit-level IEEE float32 boundaries, double
арифметику, Windows stream и порядок consumption. Каждый byte сравнивается с oracle,
а recorded original SHA дополнительно закреплён, чтобы совместная ошибка формул
native/oracle не выдавала новый stream за original.

**603173 значения, 2812052 bytes, 260 seed, 4 threads**:
2005e75f84d8a3ed429f34ceb38da48b4be9ae47250b009bd4690cf6bedef1e5.

Этот результат побайтно совпал на original x86, ASan/UBSan host, MSVC x64 и Linux
ARM64/QEMU. Native-only checks покрывают full int32 intervals, независимость от
libc srand/rand и отказ FSRND на невалидном conversion argument.

Host ASan/UBSan и MSVC x64: **7/7 CPU contracts PASS**.
ThreadSanitizer: RNG contract PASS, реальные четыре std::thread, -fsanitize=thread
подтверждён в compile flags. Это host proof, не hardware thread test.
SDK devkitA64/libnx: actual RNG/TLS contract ELF linked, undefined symbols **0**.
Build ID **dd60e014628d605be08d9db6b3296ecb6c5d9aef**. SDK link probe исключает std::thread
часть теста; сам production thread_local state присутствует. ELF на Switch не запускался.

## Воспроизведение

```sh
cmake -S port/matrixgame/native-core -B build/mg-core-asan \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCORE_SANITIZERS=ON
cmake --build build/mg-core-asan --parallel 4
ctest --test-dir build/mg-core-asan --output-on-failure
bash tools/matrixgame_file_arm64.sh --random
```

Original target: matrixgame-random-x86-reference из native-probe.
Local evidence: build/matrixgame/native-core/random-x86-fixtures/oracle.json,
build/mg-core-storage-asan/random-fixtures/oracle.json,
build/mg-core-msvc-x64/random-fixtures/oracle.json,
build/mg-core-arm64/random-fixtures/oracle.json и random-tsan-ctest.log.
Private assets для RNG tests не требуются; binary traces остаются в ignored build.
В CI добавлены ARM64 RNG execution и RNG thread check в TSan job; результаты
remote CI рассматриваются отдельно.

## Открытые границы

Совпадение RNG stream не доказывает одинаковый порядок вызовов в полном движке,
весь battle simulation, Game::Init/Run, карту, AI, rendering или campaign handoff.
Публичная нативная библиотека изолирует libc state; arbitrary external libc rand
interleavings original процесса не воспроизводятся. На hardware пока не проверены
ни TLS state game-worker, ни FPS/battery. MG0–MG5, freeze после «Готово» и публикация
готовой комбинированной NRO остаются незавершёнными. Новая NRO не создавалась.
