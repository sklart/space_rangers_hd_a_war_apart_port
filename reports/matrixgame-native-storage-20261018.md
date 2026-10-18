# MatrixGame: CStorage CPU-перенос — 18.10.2026

Статус: реальный STRG/ZL03 loader/exporter подключён к native CPU library.
Это не запуск боя и не подтверждение исправления зависания после «Готово».

## Реализация

- Подготовка исходного CStorage.cpp/CStorage.hpp с проверкой SHA; исходный submodule
  не изменён. Всего 14 закреплённых inputs CPU-слоя.
- 32-битные DWORD, packed 12-byte metadata, explicit UTF-16LE и исходный CalcUniqID.
  Все шесть исходных типов поддерживаются, включая double без выровненного указателя.
- Вместо realloc/memcpy owning C++ объектов — placement construction и noexcept move.
  При ошибке Load ранее загруженные записи сохраняются, освобождаются только новые.
- Проверка счётчиков, границ, порядка массивов, типов и deflate framing; отдельный
  Python parser независимо проверяет схему, UID, payload и байтовый roundtrip.
- Восстановлены корректные type/cursor для compressed columns. Compact использует
  memmove при перекрытии. Encoder резервирует compressBound последнего блока.
- ZL03 decoder выделяет bounded output один раз. File Load проверяет SizeFull до
  чтения; root budget учитывает Capacity входа и декодированного буфера.
  Framing всех блоков проверяется до крупного выделения decoded output.

## Проверено локально

| Среда | Результат и область |
|---|---|
| Исходный MSVC x86 | Контроль исходной сериализации и настоящий b0.vo |
| Linux host ASan/UBSan | Все 4 CPU-контракта PASS; настоящий b0.vo PASS |
| MSVC x64 | Все 4 CPU-контракта PASS; настоящий b0.vo PASS |
| Linux ARM64/QEMU | CStorage contract + настоящий b0.vo PASS |
| devkitA64/libnx | Storage contract ELF linked, undefined symbols: 0; не запускался |

Контрольный fixture: 79 записей, 6 типов, 133532 bytes, FNV32 `4212fbe8`,
UID `f5ef41bb`, SHA256
`725985df2aafb76d7eccdd634e7c183c2fae5957497bcd280d475f0da2e208ee`.
Все поля oracle совпали у исходного x86, host ASan, MSVC x64 и ARM64/QEMU.

Настоящий `Matrix/Building/ruins/b0.vo`: 8 таблиц, UID `d0e6e092`, 140642 bytes,
FNV32 `b98588fc`. Вход и экспорт побайтно совпали на всех четырёх средах:
`de47bf4b533f2e179d85dae860740d63923f8e339ad15ca43b4cb4fa27e88ed6`.
Package SHA256: `8a22f856a27a8937ef28ac61274ffdb85cc2ff865492bd4d3ddeb2a6054011f0`.

Host corpus: 30 ресурсов (по одному из 27 каталогов + три крупнейших VO),
21843490 исходных bytes. ASan/UBSan + independent parser PASS, все roundtrip
побайтно точные, включая Terron.vo (10110440 bytes) и Chassis5.vo (6810354 bytes).
Полный прогон окончательного кода: **431 VO, 41136838 bytes, PASS** с ASan/UBSan
и independent parser. Во всех 431 случаях input/export побайтно точны.
Локальное evidence: build/matrixgame/native-core/storage-all-vo-final/summary.json.

SDK ELF Build ID: `d715297cd843b19097643bc1a319990d0f6eed1f`.
NRO не создавался, опубликованный аппаратный кандидат 3772031 не заменялся.

## Воспроизведение

```sh
cmake -S port/matrixgame/native-core -B build/mg-core-storage-asan \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCORE_SANITIZERS=ON
cmake --build build/mg-core-storage-asan --parallel 4
ctest --test-dir build/mg-core-storage-asan --output-on-failure
bash tools/matrixgame_file_arm64.sh --storage
python3 tests/probe_matrixgame_storage_corpus.py \
  --binary build/mg-core-storage-asan/matrixgame-native-storage-test \
  --output build/matrixgame/native-core/storage-all-vo \
  --package /path/to/game/DATA/robots.pkg
```

Для репрезентативного прогона: `--representative`. Для ARM64 corpus к runner
передаются `--binary /usr/bin/qemu-aarch64 --runner-binary build/mg-core-arm64/storage-contract
--qemu-sysroot /usr/aarch64-linux-gnu`. Ресурсы игры остаются только в локальном
установленном дереве/ignored build outputs и не включаются в Git или CI.

ASan/UBSan включает invalid/truncated payloads, forged counts, rollback,
compressed columns, empty compressed root, CBuf move и sparse oversized disk input.
CI теперь выполняет storage contract под ARM64, ASan/UBSan и SDK link; результат
удалённого CI этого изменения ещё не является локальным PASS.

## Незакрытые границы

256 MiB — бюджет входа, распаковки и оценки новых структур загрузки, а не глобальный
RSS limit. Существующее содержимое CStorage при транзакционной замене, overhead
allocator, память вызывающей стороны не входят. Строковая оценка консервативна;
общий allocator accounting и fault-injection allocation failures остаются работой.
Typed GetFirst/GetEnd требуют выравнивания; GetValue/SetValue работают через memcpy.
Исходный unsafe double pointer не используется реальным engine (ST_DOUBLE calls
не найдены), но нельзя считать проверенным произвольного внешнего потребителя API.

StoreBlockPar/RestoreBlockPar сохранены в подготовленном CStorage_blockpar.cpp,
пока исключённом из сборки: CBlockPar/ParamParser definitions ещё не перенесены.
Остальные engine units, загрузка настоящей карты целиком, VO geometry submission,
анимация, engine Run, GPU runtime, звук/шрифты и campaign handoff остаются открыты.
MG0–MG5, физическая Switch, производительность и freeze после Ready этим не закрыты.

Родительский коммит 1682d61: CI run 39109990282, все пять заданий PASS.
Static pointer inventory текущего изменения: new_unclassified_count=0;
существующие 2401 unresolved sites этим переносом не закрываются.
