# MatrixGame после E2E-5.16.1: реальный CPU-компонент CList

Дата: 21.10.2026. Отдельный этап после завершения аудита основной игры.
Исходный MatrixGame: `218115a72fa9c5d43269f4ca84a8596a0bcb82f1`.
Физическая Switch отсутствует; игровая NRO и пакет E2E-5.16 не изменялись.

## Изменение

В native-core добавлен настоящий `MatrixLib/Base/CList.cpp`. Его алгоритмы
скопированы без изменений, кроме нормализации CRLF → LF. Подготовщик
проверяет SHA-256 закреплённых входов. Engine source view получает те же
файлы через общий подготовщик; равенство проверено после CMake configure.

В `CList.hpp` устранены три ошибки компиляции шаблонного интерфейса:

- самостоятельному заголовку добавлен `<cstddef>` для `NULL`;
- `ListSub`: ошибочное `this->sub(item)` заменено на `this->Sub(item)`;
- `ListInsert`: отсутствующий `item` заменён на параметр `head`.

Исходная ошибка компиляции всех трёх мест воспроизведена до изменения.
Это устранение барьера реального компонента MatrixLib, а не замена списка
STL-контейнером или создание игрового прототипа.

## Проверки и границы

Тест `tests/test_matrixgame_native_list.cpp` включает исправленный заголовок
первым и выполняет 4000 детерминированных операций с реальным CList:
добавление, удаление головы/середины/хвоста, вставка перед головой,
перемещение в конец, разделение и соединение списков. После каждой операции
проверяются обе связи, голова, хвост и последовательность по независимой
модели `std::vector`. Отдельно проверено полное удаление 11 узлов `ListFree`.

**Существующее ограничение upstream сохранено и явно воспроизводится:**
вставка перед внутренним узлом не обновляет прямую связь предшественника.
Тест подтверждает именно исходную топологию этого случая; он не означает
корректность всех вариантов Insert. Исправление игрового алгоритма в этот
шаг не включено, вызовы движка и требуемое поведение требуют отдельного разбора.

| Проверка | Результат |
|---|---|
| Отдельно скомпилированные CList.cpp + тест, GCC, ASan/UBSan, leak detection | PASS |
| Интеграция native-core через CMake/CTest, ASan/UBSan | PASS, 1/1 |
| Linux ARM64 GCC 15, статическая сборка, запуск QEMU | PASS, те же 4000 операций |
| devkitA64 GCC 16.1.0, `__SWITCH__`, ARMv8/Cortex-A57, libnx | ELF скомпонован, undefined symbols: 0 |
| Запуск ELF на Switch, D3D/deko3d, полный движок/планетарный бой | НЕ проверено |

В интеграционном CMake фактические флаги библиотеки и теста содержат
`-fsanitize=address,undefined -fno-omit-frame-pointer -O2 -g`;
у теста после `-DNDEBUG` стоит `-UNDEBUG`. Повторная полная инвентаризация
107 engine units и весь прежний набор тестов здесь не запускались.
`CWorldObject`/полный движок всё ещё имеют зависимости D3D/D3DX;
успех CList не означает снятия этих барьеров. BigVB/BigIB этим шагом не менялись.

## Воспроизведение

Из корня репозитория в WSL (Ubuntu-26.04):

```sh
python3 tools/matrixgame_core_prepare.py --output build/matrixgame/e2e5161-list/source
g++ -std=c++20 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Ibuild/matrixgame/e2e5161-list/source tests/test_matrixgame_native_list.cpp build/matrixgame/e2e5161-list/source/CList.cpp -o build/matrixgame/e2e5161-list/list-asan
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 build/matrixgame/e2e5161-list/list-asan
cmake -S port/matrixgame/native-engine -B build/e2e515/mg-navigation-asan
cmake --build build/e2e515/mg-navigation-asan --target matrixgame-native-list-test --parallel 2
ctest --test-dir build/e2e515/mg-navigation-asan -R matrixgame-native-list-compatibility --output-on-failure
aarch64-linux-gnu-g++-15 -std=c++20 -O2 -UNDEBUG -static -Ibuild/matrixgame/e2e5161-list/source tests/test_matrixgame_native_list.cpp build/matrixgame/e2e5161-list/source/CList.cpp -o build/matrixgame/e2e5161-list/list-linux-arm64
qemu-aarch64 build/matrixgame/e2e5161-list/list-linux-arm64
```

Приведённый CMake использует уже существующий sanitizer cache. Для нового
каталога дополнительно задать `-DCMAKE_BUILD_TYPE=RelWithDebInfo`
и `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"`.

DevkitA64 ELF получен из тех же двух файлов с `-std=c++20 -O2 -g -UNDEBUG`,
`-D_RELDEBUG -D__SWITCH__ -D_POSIX_C_SOURCE=200809L`,
`-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE`,
`-ffunction-sections -fdata-sections -ffp-contract=off`, include prepared source
и libnx/include, link libnx/lib `-lnx -specs=C:/devkitPro/libnx/switch.specs`
и `-Wl,--gc-sections -Wl,--build-id=sha1`. Для вызова SDK `DEVKITPRO`
локально задан `C:/devkitPro` с восстановлением прежнего значения после команды.
Этот ELF является только link probe, NRO из него не готовилась.

## Контрольные суммы

Проверены нормализованные исходники CList.cpp:
`4e6e9cc167e6f7f996b6d77d273900ca38dea10584f93214a29769f5d5d953dd`.
Исправленный effective CList.hpp:
`e18d2411124f10d4d043a991e5c76d66ac970c1985b9274c606e1ca63dfee587`.

`build/matrixgame/e2e5161-list/list-linux-arm64` SHA-256:
`4f282b2eedcdee32057518d285732da3f507343426dfff291ee50e90224cbf11`.

`build/matrixgame/e2e5161-list/list-switch-contract.elf` SHA-256:
`16428eb4ab101cbd9bbd6b2fa09635e8496731edef54d6b234d73027cff49571`.
Build ID: `525d4e990638bbf6602db417cd2968bd3c8dc7bb`, ELF64 AArch64.

Локальные evidence, header.diff, логи ASan/CTest/QEMU находятся в
`build/matrixgame/e2e5161-list/`. Они относятся к CPU-компоненту;
HARDWARE PENDING основной игры остаётся в отчёте E2E-5.16.
Неизменность SHA-256 исходного игрового ZIP повторно проверена:
`e1a1dec5f410b983d629ae538ad9b4ba063f89a8101af43d27ea00c6ce6a08a4`.
