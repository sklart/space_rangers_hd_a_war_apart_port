# MatrixGame: BlockPar / ParamParser CPU-перенос — 18.10.2026

Статус: исходные CBlockPar.cpp и CStorage_blockpar.cpp включены в native CPU library.
Это следующий этап после CStorage a5152f8, а не запуск всего MatrixGame.

## Изменение

- Проверяются 15 pinned inputs. SHA256 LF-normalized CBlockPar.cpp:
  bc2c55bddd62709bad774569f267c34b9e0c08b910d1be5a40924a62fa83ce0a.
  Такой же SHA подтверждён у parser в исходном MSVC x86 reference snapshot.
- Сохранены исходные грамматика BlockPar, комментарии, повторяющиеся имена,
  unnamed параметры, пути, include-файлы и StoreBlockPar/RestoreBlockPar.
- GetInt/GetDword/GetHexUnsigned явно работают с UTF-16 units и Windows 16/32-bit
  wrap. GetInt больше не требует signed-overflow UB; wchar_t остаётся обычным
  платформенным типом. GetDouble/IsOnlyInt сохраняют исходные правила.
- utils UTF-8 boundary проходит через char16_t, а текстовые файлы UTF-16LE с BOM
  декодируются явно. Неполная кодовая единица отвергается. Empty/BOM-only file
  оставляет прежние параметры как оригинал; UTF-8 C-string путь сохраняется.
- Широкое форматирование сохраняет intended limit 10240 символов и переносит
  локальный 40-KiB buffer в bounded heap storage. Короткие/9000-символьные строки
  и формат %.8f проверены с исходной реализацией.
- Guard 128 активных уровней для рекурсивного parse/Store/Restore; при Restore
  проверяются наличие и парность колонок. Native-only проверки: missing root,
  циклическая ссылка, чрезмерная вложенность, malformed UTF-8 и odd UTF-16.
- Win32 Tracer/DTRACE instrumentation не перенесена; вызовы DTRACE исключены из
  подготовленных копий. Загрузчик, parser и storage bridge настоящие, без заглушек.

## Контроль и настоящие настройки

Один C++ контракт выполняется на исходной MSVC x86 реализации и native CPU core.
Все 65536 UTF-16 units, supplementary-plane value, overflow/знаки/нецифровые
символы: numeric FNV64 **fbe5b145ba008067**. Этот x86 transcript закреплён в CI.

Controlled tree: 4 блока, 5 параметров, FNV64 **469a674f3ada5d45**.
Plain STRG SHA256: de7d430c4778b4a6fa590d163af55498641393d419fd63028a359db3e527d85f.
UTF-16 text SHA256: 199f8b432c47f2612cc3e6d877487ca5fbbdf294ec1f82872303d6abc4fad6e5.
UTF-8 export после decode точно совпал с UTF-16 export. Store/Restore, file roundtrip,
CopyFrom, paths и include contract PASS.

| Реальный config | Root | Блоков | Параметров | FNV64 дерева |
|---|---|---:|---:|---|
| CFG/Rus/robots.dat | da | 289 | 2172 | 279cc38ca9000a88 |
| CFG/Rus/robots.dat | if | 453 | 5888 | 26b8579c84f4f97f |
| CFG/Eng/robots.dat | da | 289 | 2122 | f6ec3eb6cb002d07 |
| CFG/Eng/robots.dat | if | 453 | 5888 | 9ff8827d75bf87cc |

RU config SHA256: 1f4e0c525aa663c110295940bcfb58d8f950c813b78689c5df2072538fd54296.
EN config SHA256: 035204e5a05c309e040083bb751eb41c7b4db144eae071645ae81ef748437238.

Имена/значения/порядок и счётчики обоих деревьев совпали на исходном x86,
ASan/UBSan host, MSVC x64 и Linux ARM64/QEMU. Python независимо разбирает исходный
STRG, проверяет схему/ссылки и рассчитывает traversal digest, не вызывая C++ parser.
Только совпадение количества блоков не считается доказательством.

Host ASan/UBSan и MSVC x64: **5/5 CPU contracts PASS**.
SDK devkitA64/libnx: BlockPar contract ELF linked, undefined symbols **0**,
Build ID **6192a4daa63f944021678ce3a298b81dcdc9cbc0**. ELF не запускался и не паковался.

## Воспроизведение

```sh
cmake -S port/matrixgame/native-core -B build/mg-core-asan \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCORE_SANITIZERS=ON
cmake --build build/mg-core-asan --parallel 4
ctest --test-dir build/mg-core-asan --output-on-failure
bash tools/matrixgame_file_arm64.sh --blockpar
bash tools/matrixgame_file_arm64.sh --blockpar \
  --config /path/to/game/CFG/Rus/robots.dat
```

Для сравнения с original reference: runner принимает --reference /path/to/oracle.json.
Локальные evidence folders: build/matrixgame/native-core/blockpar-{x86,host,msvc,arm64}-{Rus,Eng}.
Настоящие ресурсы и экспортированные конфигурации не включены в Git/CI artifacts.
CI получает asset-free BlockPar contract, original numeric transcript, ASan/UBSan,
Linux ARM64 execution и SDK linkage. Результаты remote CI фиксируются отдельно.

## Оставшиеся границы

Это CPU resource/configuration proof. ARM64 CGame::Init/Run, настоящая карта целиком,
геометрия/анимация, GPU execution/swapchain, звук, шрифты, MG0–MG5 и возврат в кампанию
ещё не подключены. Не объявляется ни исправление freeze после «Готово», ни ускорение.
128 уровней — защита от чрезмерной рекурсии, не полный parser validator и не общий
RSS limit. Restore сохраняет исходную append-семантику; rollback частично добавленного
дерева при некорректных ссылках не обещан. Бюджет CStorage остаётся описанным на
предыдущем этапе; отдельный allocation-failure/global memory accounting ещё открыт.
