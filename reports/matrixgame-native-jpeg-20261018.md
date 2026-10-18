# MatrixGame: JPEG textures и полный набор robots.pkg — 18.10.2026

Статус: TextureAsset и TextureRequest действительно декодируют JPEG в straight
RGBA8. Все 1507 PNG/JPEG/DDS/STRG ресурсов исходного robots.pkg проверяются общей
native asset pipeline. Это asset-format coverage конкретного пакета, не полный
MatrixGame, skin/cache lifecycle, Horizon execution или бой. Причина freeze после
«Готово» остаётся ROOT CAUSE UNCONFIRMED.

## Реализация

jpeg_texture.cpp использует настоящий libjpeg API, без Windows/D3D stubs.
Decode поддерживает 8-bit grayscale/RGB/YCbCr baseline и progressive JPEG,
ISLOW IDCT, fancy upsampling и opaque alpha 255. CMYK/YCCK, 12-bit precision,
неподдерживаемые JPEG processes, truncation/warnings и trailing bytes отвергаются.
ICC color transforms и EXIF orientation не применяются: загружаются исходные
texture pixels без автоматического поворота. Новые mip levels не генерируются.

Source buffer заимствуется только до конца синхронного decode. Dimensions/pitch
проверяются до RGBA allocation: max dimension 8192, owned RGBA до 64 MiB.
Encoded input ограничен 128 MiB. Progress callback ограничивает число JPEG scans
256. Codec memory policy отдельно ограничивает cumulative requested data до
64 MiB через alloc_small/large, alloc_sarray/barray и request_virt_* hooks.

Virtual progressive coefficient arrays учитываются при request, до их
realization. max_memory_to_use тоже задаётся, но не используется как единственное
доказательство лимита: внутренний allocator может иметь собственные pools.
Это логические budgets данных; internal pool overhead/initial manager metadata,
encoded buffers, RGBA, CStorage и GPU staging не являются общим RSS cap.
Cumulative charging консервативно не вычитает освобождённые pools внутри decoder.

C error callback использует setjmp/longjmp. Context и все нетривиальные объекты,
живые при codec calls, создаются до jump boundary; callbacks меняют только heap
state и не бросают C++ exceptions через C frames. Decode failure, codec budget
и allocator OOM переводятся в разные C++ exceptions после возврата через handler.
Context destructor освобождает codec и C++ buffers. Новый context используется
для каждой загрузки; отказ не объявляется успешным изображением.
API basis: [libjpeg-turbo 2.1.2 documentation](https://github.com/libjpeg-turbo/libjpeg-turbo/blob/2.1.2/libjpeg.txt).

## Зависимости

libjpeg-turbo 2.1.2 выбран по уже установленному Switch SDK; pin:
ecf021bc0d6f435daacff7c35ccaeef0145df1b9.
MSVC x64 и Linux ARM64 собирают этот pin в отдельном ignored build directory,
WITH_SIMD=OFF, ENABLE_STATIC=ON, ENABLE_SHARED=OFF, WITH_TURBOJPEG=OFF.
Source pin и clean state проверяются. Upstream game submodules не меняются.

MSVC использует official jpeglib.h/jmorecfg.h/jerror.h плюс generated jconfig.h
из того же build в build/matrixgame/libjpeg-msvc-x64/stage/include, с actual
Release/jpeg-static.lib. Стандартные /EHsc flags сохраняются. Host ASan использует
установленный Linux libjpeg-turbo 2.1.5; Switch links SDK libjpeg 2.1.2.
CMake требует JPEG::JPEG; deko build report записывает header/library SHA и требует
реальные jpeg_read_header/start_decompress/read_scanlines/destroy symbols.

## Независимый original D3DX decode

Win32 reference вызывает настоящие D3DXGetImageInfoFromFileInMemory и
D3DXLoadSurfaceFromFileInMemory с D3DX_FILTER_NONE, как original Texture.cpp.
Результат читается из настоящей D3DPOOL_SYSTEMMEM A8R8G8B8 surface и преобразуется
BGRA→RGBA. Для поверхности создаётся невидимое окно и actual D3D9 NULLREF device;
Draw/Present не вызываются. Это CPU image decode, не Switch/GPU sampling proof.

tools/matrixgame_jpeg_oracle.py сверил 24 controlled JPEG и все 77 настоящих JPEG
из robots.pkg: 101 case, 7799996 RGBA bytes. Pillow 12.3.0/libjpeg-turbo 3.1.4.1
и actual original D3DX совпали побайтово: max_abs_error=0, differing_bytes=0.
Pillow сам использует ту же algorithm family, поэтому он отдельно не называется
независимой реализацией; независимая сверка здесь — actual D3DX library.
Native декодирование затем сравнивается с проверенными canonical bytes.

Private source images/decoded pixels находятся только в ignored build output.
В репозитории есть только контролируемые сгенерированные JPEG и tiny RGBA oracles:
tests/fixtures/matrixgame/jpeg-control.json. Regeneration tool использует Pillow,
но CI decode contracts не требуют Pillow или закрытых игровых ресурсов.

## Проверки

Controlled texture suite расширен с 92 до 125 cases: 24 valid JPEG (RGB/gray,
1x1/odd sizes, 4:4:4/4:2:2/4:2:0, baseline/progressive) и 9 rejection cases,
включая CMYK policy, oversized header, truncation, missing EOI и trailing data.
125-case trace: 173 mips, 64804 bytes, SHA-256
62fc93d3367b3ec01fc6dff833a3454c9e5c5c0e5ec0f385d2bc9dc5870ed841.

На каждом valid direct JPEG дополнительно вызывается настоящий decoder с codec
budget 8 bytes: должен произойти controlled budget rejection без повреждения input.
Отдельный negative mode использует progressive RGB header 4096x4096: RGBA extent
ещё допустим, но coefficient request превышает codec budget до realization.
Проверяется std::length_error с cleanup. Это controlled budget test, не аппаратный
OOM, не реальная memory exhaustion и не доказательство recovery основной игры.

Actual package coverage: 617 PNG + 77 JPEG + 803 DDS + 10 STRG = 1507 files,
6566 supplied mip levels. Canonical stream включает metadata, payload и GPU staging
bytes/padding: 486055868 bytes. Native MSVC и ARM64/QEMU совпали с oracle:
SHA-256 92c2c1a99803814721df0f4909d4c698a3ec760a3c67001bf31105c8849aee0f.
PNG проверяются independent Python decoder, DDS blocks сохраняются побайтово,
JPEG bytes проверены actual D3DX/Pillow comparison. Это не BC GPU pixel decode.

Для полного локального replay сначала экспортировать JPEG oracle:

```powershell
# Python с Pillow; output должен быть новым или уже отмеченным oracle directory.
python tools/matrixgame_jpeg_oracle.py --package <robots.pkg> --output build/matrixgame/jpeg-oracle --d3dx-binary <Win32-reference.exe>
```

Затем `bash tools/matrixgame_file_arm64.sh --texture --package <robots.pkg>
--jpeg-oracle-map build/matrixgame/jpeg-oracle/jpeg-oracle.json`.
Без private args тот же script проверяет asset-free fixtures, включая JPEG budgets.
Логи: build/matrixgame/native-core/jpeg-*.

## Открытые ворота ТЗ

BMP input и CMYK/12-bit JPEG не поддерживаются и в проверенном наборе не обнаружены. Остаются
auto mip generation, общий asset/GPU cache budget/eviction, original skin/combiners,
CVectorObject/backend и Init/Run с настоящим боем/возвратом результата.
Hardware freeze/OOM, frame time/battery, master downloadable artifact и релиз
не закрываются CPU image parity. Новая NRO не создаётся; кандидат 3772031 сохранён.

## Финальная локальная фиксация

- MSVC x64: полный native suite 16/16; controlled JPEG и progressive coefficient
  budget mode прошли. Real replay 1507 files имеет приведённый SHA.
- ASan/UBSan: полный native suite 17/17, затем отдельный real replay всех 1507 files
  с detect_leaks=1 и halt_on_error=1. Получен тот же SHA и 6566 mips.
- ARM64/QEMU: финальный controlled + resource-budget + 1507-file replay дал тот же
  SHA; pinned JPEG source остался clean.
- Все 8829 filename/query/extensionless requests теперь успешно Load в native MSVC.
  Metadata paths/flags по-прежнему совпадают с actual original x86/oracle:
  43f94e59eb99c501b955e23f6d4463d788cff7614b338f0cd4edf12e707a0755.
  Это исправляет ранее явно неподдерживаемые 459 JPEG requests.
- Win32 actual D3DX comparison 101 cases остаётся exact (0 differing bytes);
  текущие fixture encoded/pixel hashes дополнительно сверены с этой reference map.
- devkitA64/deko ELF compile/link PASS, nm -u пуст. ELF 5500728 bytes, SHA-256
  6741a9c6a4ee000e78de71faa12efefe53bc095fa3e386904852c04daf35445e.
  Hardware NOT TESTED, nro_packaged false.
- SDK jpeglib.h SHA-256 b51cc0cb5ac082465004dc89422ebda439b053fdffbcf6baf8a6336a3288759f;
  libjpeg.a SHA-256 5af356a170257d1999a86e1412534237635787fa3ecad793b5baa1847fc0b70f.
- Static baseline guard new_unclassified_count=0; это не закрытие всех старых
  pointer findings. Все три upstream submodule pins сохранены.

Оригинальный dirty checkout не изменялся. Код и результаты изолированы в worktree;
master/release и hardware candidate 3772031 не заменялись.
