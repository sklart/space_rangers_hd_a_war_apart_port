# MatrixGame: DDS/STRG и mip upload — 18.10.2026

Статус: native TextureAsset загружает настоящие PNG/DDS/STRG через CFile/PKG.
DDS сохраняет все предоставленные mip levels и BC1/BC2/BC3 blocks; deko3d получает
сжатую цепочку без CPU decompression. Это asset/backend boundary, не полный
CBaseTexture/CCache/skin manager и не завершённый MatrixGame. Freeze после «Готово»:
ROOT CAUSE UNCONFIRMED, аппаратный повтор отложен пользователем.

## Форматы и исходная семантика

Оригинальный Texture.cpp в LoadTextureFromFile использует CCacheData::LoadFromFile,
а при TF_COMPRESSED — настоящий CStorage, table `0`, column `0`, ST_BYTE, array 0.
Native loader повторяет извлечение payload через уже перенесённый CStorage.
В самостоятельном exact-path API STRG определяется по magic; material Compressed
flag и extension search ещё не интегрированы. Вложенный STRG не декодируется.
Original pinned sources/submodules не меняются.

DDS legacy header проверяется перед чтением payload. Поддерживаются DXT1/3/5,
RGBA/RGB 8/16/24/32-bit masks, luminance и alpha-only. Cubemaps, volumes, DX10,
DXT2/4 и неизвестные layouts отвергаются. Все заявленные mip levels проверяются;
полная цепочка до 1x1 не требуется, но лишние уровни, truncation и trailing bytes
отвергаются. Для uncompressed mip 0 учитывается DDSD_PITCH; следующие уровни
плотно упакованы. Конкретная альтернативная padded layout не угадывается.

BC1/2/3 payload остаётся побайтово исходным. Несжатые форматы преобразуются в
straight RGBA8 по channel masks: luminance повторяется в RGB, alpha-only даёт
white RGB и исходную alpha. Color normalization округляется к ближайшему байту.
PNG сохраняет RGBA semantics предыдущего переноса. Автоматическое создание новых
mips для PNG и неполных DDS ещё не реализовано; существующие DDS mips сохраняются.
Справочник legacy masks: [DDS_PIXELFORMAT](https://learn.microsoft.com/en-us/windows/win32/direct3ddds/dds-pixelformat).

Encoded input ограничен 128 MiB, retained texture data 64 MiB, dimensions 8192.
GPU uploader принимает dimensions до 2048; общий texture GPU budget остаётся
64 MiB с учётом retained old slots + new image + staging. CPU limits не являются
общим RSS: STRG/PNG временные буферы и allocator overhead учитываются отдельно.

## GPU boundary и исправление pitch

SetTextureAsset создаёт настоящий deko3d image нужного BC/RGBA format с полным
заявленным mip count. Каждая mip copy использует отдельный ImageView level,
стaging rows 32-byte aligned и offsets 256-byte aligned. Неиспользованные padding
bytes обнуляются. Проверяются source extent, shape chain и alias до записи.
Descriptor публикуется после queue completion; при pre-submit allocation/budget
failure прежний slot сохраняется. Это реализация lifecycle, не аппаратный PASS.

В раннем compile-only RGBA upload/readback rowLength ошибочно задавался в texels
(pitch/4). Реальный deko3d copy API использует этот параметр как byte pitch.
Оба вызова исправлены; imageHeight=0 оставляет default layer stride для 2D image.
Основание: [dkCmdBufCopyBufferToImage / copyImageToBuffer implementation](https://github.com/devkitPro/deko3d/blob/master/source/dk_image.cpp).
Предыдущая линковка ELF не могла обнаружить ошибку единиц. Физическое изображение
этого backend ранее не проверялось; его успешность задним числом не заявляется.

Добавлены явные mip sampler варианты nearest/linear + repeat/clamp. Старые четыре
no-mip sampler значения сохранены. Material flags и выбор sampler игровым skin
manager остаются открытыми. Texture cache и renderer по-прежнему однопоточные.

Future probe `--texture <source> <64x64.rgba>` загружает actual asset и сравнивает
GPU base-level nearest readback с предоставленным независимым RGBA reference.
Нельзя считать его запуском боя или проверкой всех mip LOD. Probe не запускался;
новой NRO нет.

## Локальные доказательства

В robots.pkg обнаружены 803 DDS: DXT1 517, DXT3 191, DXT5 85, luminance+alpha16 8,
alpha8 2. Ещё 10 STRG содержат DDS DXT1. Все 813 файлов проходят CFile/PKG,
настоящий CStorage при STRG, native DDS parser и staging packer.

Всего 5872 mip levels. Native MSVC, ASan/UBSan и ARM64/QEMU совпали с независимым
Python разбором в 130905640 байтах канонической трассы (metadata, mip payloads,
GPU staging padding). SHA-256:
1135e274f8eee37a791dfcf4a29faf9a0038927f6e611e262b78d7926f20925a.
Это byte/payload parity, не CPU decode BC pixels, D3DX device comparison или GPU
sampling parity. Для compressed textures CPU decompression намеренно отсутствует.

Controlled fixtures покрывают BC1/2/3, RGB/RGBA masks, alpha/luminance, odd sizes,
1x1, full/partial mip chains, base row padding, raw/ZL03 STRG, wrapped PNG,
невалидные header/masks/mips/type, truncation и nested payload rejection.
Проверяются memory/file equality, source immutability, staging padding, tail guard,
short staging, alias и malformed upload plans.

```sh
bash tools/matrixgame_file_arm64.sh --texture
# Для локального закрытого пакета добавить --package /absolute/path/robots.pkg
```

CI получает только контролируемые fixtures. Реальные игровые данные не включены
в репозиторий. Полные логи и oracle JSON: build/matrixgame/native-core/texture-*.

## Открытые ворота ТЗ

Остаются JPEG/BMP input, resolution/flags из original Texture.cpp, CCache factory,
полный CVectorObject/skin/material/shader pipeline, game Init/Run и возврат результата.
Данный блок не закрывает MG0–MG5, P0 freeze, hardware OOM, frame time/battery,
master downloadable artifact gate или релиз. Кандидат 3772031 не менялся.

## Финальный прогон

После последней проверки отсутствующей alpha mask и отдельного alias control:

- Controlled: 92 cases (73 valid, 19 rejected), 149 mips, 49144 trace bytes;
  SHA-256 69acd9ab4518e9f7bd9046f2737e4d7224c8abd29f17739467effb32b59ee37f.
- MSVC x64: полный native suite 14/14, затем повтор всех 813 файлов.
- ASan/UBSan: полный native suite 15/15, затем повтор всех 813 файлов,
  detect_leaks=1, halt_on_error=1. Real trace SHA совпал с MSVC.
- ARM64/QEMU: controlled + все 813 файлов, тот же controlled и real SHA.
- devkitA64/deko ELF: compile/link PASS, unresolved symbols отсутствуют (nm -u).
  SHA-256 4ae320755b8116aae0c15b9b92871a98a2175c27fae6d689c5cc5388088a9474;
  размер 5215208 bytes; hardware NOT TESTED, nro_packaged false.
- Static baseline guard: new_unclassified_count=0. Общий inventory содержит старые
  unresolved findings и не является доказательством устранения всех pointer sites.

Исходный dirty checkout не изменялся. Сборки и исходники находятся в отдельном
worktree; master/release артефакты не заменялись.
