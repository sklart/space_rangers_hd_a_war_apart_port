# MatrixGame: original texture requests и sidecar flags — 18.10.2026

Статус: TextureRequest разрешает имена через настоящий CFile/PKG, выполняет
извлечённый original CBaseTexture::ParseFlags и загружает native TextureAsset.
Это следующий участок asset pipeline, а не CCache factory, skin rendering,
MatrixGame Init/Run или бой. Причина Switch freeze остаётся ROOT CAUSE UNCONFIRMED.

## Исходный код и перенос

Core preparation теперь проверяет 32 LF-normalized pinned inputs. Добавлены:

- Texture.hpp: 2eaf5916cc9a2b406e886617a156a33ab39682ed6196011bb0ec873295f575a1
- Texture.cpp: 11535b4750ea94de8b97831636b044a781e785be8ecb9bffbe16358df72b0901
- Cache.cpp: 568f108f4578e8fb53a5b08bf2561189d4d3426402275deabeccd17a1e82fa9a

matrixgame_texture_request_overrides.py извлекает именно ParseFlags body, меняет
только квалификацию/return boundary, local uint32_t flags и убирает DTRACE.
Не создаются фиктивные CBaseTexture/D3D headers или методы GPU. Значения TF_*
извлекаются из original Texture.hpp. Original Texture.cpp и Cache.cpp в source
view остаются неизменёнными; native core компилирует отдельный TextureFlags.cpp.
Upstream pin 218115a72fa9c5d43269f4ca84a8596a0bcb82f1 не меняется.

Сохранены case-sensitive query tokens `Trans` и `Alpha`, trim whitespace,
приоритет соседнего .txt над query, значения `0`/nonzero text, а также исключения
pinguin.txt/robotarget.txt. Последние содержат другие игровые данные и не должны
попадать в parser texture flags. Sidecar читается настоящим CBlockPar, включая
поддержку UTF-16LE BOM. Compressed=1 устанавливает TF_COMPRESSED.

Portable extension helper сохраняет Unicode и Windows separators без host locale.
ParseFlags передаёт ему plain name до `?`. Общий helper сохраняет query один раз;
глобальная эквивалентность исходного CacheReplaceFileExt для malformed/query paths
не заявляется. Resolve отклоняет пустое имя, embedded NUL и длину более 32768.

Resolution использует исходную последовательность extensions png~jpg~bmp~dds~strg
в CFile. Сначала проверяется explicit path. PKG fallback выполняется по extension
list. Для loose files с несколькими одинаковыми basename выбор остаётся за
filesystem enumeration CFile, как в original, и может отличаться между ОС.
Эта неоднозначность не объявляется исправленной: контролируемая сверка использует
explicit filenames при коллизии; реальные PKG запросы сверяются отдельно.

## Реальная загрузка

TextureRequest::Load не только сохраняет flags: Compressed обязателен для STRG
payload, и Compressed без STRG также отклоняется. После проверки выполняется
настоящий TextureAsset::Decode с CStorage/PNG/DDS/mip data. Missing file, ошибочная
конфигурация и неверный payload не превращаются в успешную загрузку или white
texture. Ошибки выходят вызывающему коду; request остаётся обычным value object.

Входной буфер ограничен 128 MiB, остальные лимиты TextureAsset сохраняются.
Это не общий RSS или законченный cache budget. CFile/BlockPar/FilePNG механизмы
используются из предыдущих переносов, без альтернативного fake parser в runtime.

В deko ELF подключён future probe `--texture-request <name>`: Resolve → Load →
SetTextureAsset, с выводом actual flags/mip count. Он сообщает upload completion,
не PASS изображения. Применение alpha-test/blend в оригинальных skin passes ещё
не подключено; draw_executed=false и battle_executed=false. Probe не запускался
на Switch, NRO не создаётся.

## Сверка с original x86

tests/test_matrixgame_native_texture_request.cpp собирается в двух вариантах:

- native использует перенесённый ParseFlags/TextureRequest и настоящий loader;
- Win32 reference линкует настоящую MatrixGameInternal/MatrixLib. Test-only subclass
  CTexture читает protected flags после actual ParseFlags. GPU Load не вызывается,
  никакие игровые virtual functions не подменяются.

Python oracle независимо определяет flags, sidecar overrides, paths и ожидаемую
поддержку payload. Каноническая metadata trace включает resolution status, flags
и normalized UTF-8 path. Native дополнительно должен действительно Load mip data;
успех metadata не засчитывается как успешное декодирование.

Controlled: 68 requests, 56 successful loads; query case/whitespace/empty tokens,
sidecar precedence, UTF-16 BOM, специальные исключения, missing file, explicit
extension collision и несовместимый Compressed PNG. Metadata SHA-256:
87a7fcd01bfe43cdc40e4c4ed469179318ca10c97a206b2aeee5d3791fd624f2.
MSVC native и actual original x86 совпали с oracle.

Настоящий robots.pkg: 1507 texture filenames и 1436 unique extensionless names,
каждый без query, с Trans и с Alpha: 8829 requests. MSVC native и actual x86
совпали по paths/flags с oracle. Metadata SHA-256:
43f94e59eb99c501b955e23f6d4463d788cff7614b338f0cd4edf12e707a0755.
Native выполнил 8370 загрузок. Остальные 459 соответствуют JPEG filename/basename
requests (77 exact JPEG и 76 extensionless, по три варианта): JPEG остаётся явно
неподдерживаемым decoder. Это не пропущенные успехи и не полный asset-format PASS.

```sh
bash tools/matrixgame_file_arm64.sh --texture-request
# Для actual package добавить --package /absolute/path/robots.pkg
```

CI использует controlled fixtures. Игровые ресурсы не добавлены в репозиторий.
Логи/oracles: build/matrixgame/native-core/texture-request-*.

## Открытые ворота

Остаются JPEG/BMP decode, auto-generated mips, CCache factory/eviction, полный
CVectorObject/skin/material backend, renderer integration в Init/Run и настоящий
бой с возвращением результата. Hardware freeze, OOM, frame times/battery и master
artifact gate не закрываются этой CPU-проверкой. Кандидат 3772031 сохранён.

## Финальная локальная проверка

- MSVC x64: полный native suite 15/15; controlled и 8829 real requests прошли
  native/x86/oracle comparison, включая 8370 actual native loads.
- ASan/UBSan: полный native suite 16/16; отдельный real package replay дал
  тот же metadata SHA и число loads (detect_leaks=1, halt_on_error=1).
- ARM64/QEMU: controlled и real 8829 requests дали те же SHA/loads.
- Actual x86 reference Texture.hpp/Texture.cpp/Cache.cpp имеют ровно приведённые
  pinned hashes; reference не использует native extraction.
- devkitA64/deko ELF compile/link PASS, nm -u не показывает unresolved symbols.
  ELF 5263528 bytes, SHA-256
  984ddc3fc4d9e9accf6e3bbca3ca4ebfa9738d880ead8b700404b3d74ab86098.
  hardware NOT TESTED, nro_packaged false.
- Static baseline guard new_unclassified_count=0; старые unresolved entries
  остаются в inventory. Этот guard не закрывает весь P0.3.

Работа выполнена в отдельном worktree. Исходный checkout и кандидат 3772031
не заменялись; master merge и релиз не выполнялись.
