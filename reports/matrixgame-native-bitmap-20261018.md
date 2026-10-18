# MatrixGame: CBitmap, PNG и загрузка текстуры — 18.10.2026

Статус: оригинальный CPU CBitmap включён в native core; FilePNG реализован через
настоящий libpng. Путь CFile/PKG → RGBA → deko3d скомпилирован и связан для Switch.
Полный движок, планетарный бой и GPU readback на консоли не проверены.
Аппаратное зависание после «Готово»: ROOT CAUSE UNCONFIRMED.

## Исходники и разрядность

Подготовка проверяет 29 pinned inputs, включая четыре LF-normalized SHA:

- CBitmap.hpp: 7638f74743d26c427ec5f0f17626aaca6f1dd6879877370db8c3d4c7fe71d6c5
- CBitmap.cpp: ee8e9f8da05306c7b33a3bc706b4953daf0cdf4181121f7819a3a003912a3437
- FilePNG.hpp: 390d96acc19fbc1522751bbf3b4e909ecbb96a81fdb829ad031b25811b330cd3
- FilePNG.cpp: 8af367f028492df51f3089aafa5eab5ea732cfea0b45f5de9318dfb5cb543397

Upstream MatrixGame остаётся 218115a72fa9c5d43269f4ca84a8596a0bcb82f1.
Original ReadStart_Buf производит uintptr_t, CBitmap сохранял его в uint32_t,
затем передавал в Read как uintptr_t. Native owner сохраняет полный адрес.
Read потребляет handle один раз; ReadCancel освобождает ещё не переданный handle
при исключении выделения изображения/палитры. Буфер PNG заимствуется до Read/Cancel.
Повторное использование потреблённого handle не поддерживается.

Original файловый LoadFromPNG игнорировал false из memory overload и возвращал true.
Это подтверждено запуском неизменённой x86 MatrixLib на PNG gray+alpha. Native
возвращает результат декодирования. Связь этих дефектов с freeze основной игры
не установлена: это отдельный перенос MatrixGame.

## Поведение и границы

Сохранены classic gray/RGB/RGBA/palette форматы. Дополнительный LoadRGBAFromPNG
преобразует palette/tRNS/gray/gray+alpha/16-bit/Adam7 в straight RGBA8 для GPU,
без premultiply. Classic gray+alpha остаётся неподдерживаемым, как в original.
GDI WinBitmap оставлен только forward declaration; реализации захвата экрана нет.

Проверяются dimensions 1..8192, pitch, rectangles и overflow. Лимит изображения
64 MiB, encoded PNG 128 MiB, отдельно запрошенная libpng память до 64 MiB.
Это локальные логические лимиты, не общий RSS или бюджет всех ресурсов движка.
libpng callbacks используют C error/longjmp без C++ exception через C frames;
cleanup хранится в heap context. Выравнивание allocator учитывает 16-byte jmp_buf
на Windows x64. Недопустимые PNG возвращают false; allocation exceptions CBitmap
распространяются после cleanup.

Copy поддерживает overlap и RGB↔RGBA; unaligned scalar accesses используют memcpy.
Downsample уважает crop/pitch и заимствованную память, не освобождает external buffer.
Поведение для нечётных/однопиксельных размеров определено native реализацией;
глобальная эквивалентность всех raster операций original не заявляется.
BMP/DDS сериализуются с явной раскладкой little-endian для целевых x86/ARM64.
DDS decode, JPEG, STRG-wrapped textures, material flags, skins и GPU cache ещё
не подключены. Полный CVectorObject backend по-прежнему требует переноса D3D9.

UploadPngTexture декодирует через реальный CFile/PKG и передаёт bytes в
SetTextureRGBA8. Staging copy и queue wait завершаются до уничтожения CBitmap.
Probe --png проверяет nearest readback при будущем запуске; сейчас это compile/link
proof, не GPU PASS. Новая NRO для этого блока не создаётся.

## Проверки

- Independent Python PNG decoder: 98 cases (90 valid + 8 invalid), 168 decoded
  variants, 29880 pixel bytes. Trace SHA-256:
  a6cf15ad4f733b61a19c2c26fb6f6c2f8289b3f15b78dbed34d5d1ba74c07ca3.
- Original x86 MatrixLib: 90 classic cases, включая rejected gray+alpha.
  Trace SHA-256: 3d2c8cc613a76eb594872172cf41b3d66c299538f3962461e40e621c48bb179a.
- Настоящий robots.pkg: все 617 PNG, 1233 decoded variants (617 RGBA + 616 classic),
  320059104 pixel bytes. ASan host и ARM64/QEMU совпали побайтово с oracle.
  Trace SHA-256: 0d3584139346f829c09d2b3cb350c348dc8cf9cb7643f9fb72543a828712271e.
- Полный native suite: ASan/UBSan 14/14; MSVC x64 13/13.
- GNU linker wraps calloc/realloc: два реальных отказа CBitmap allocations после
  создания PNG context (pixels и palette); exception cleanup и повторная загрузка
  проходят под ASan. Это контролируемый fault test, не аппаратный OOM.
- devkitA64 bitmap ELF и deko PNG upload ELF связаны с libnx/libpng/deko3d.
  Выполнение на Horizon/GPU не проверено.

Воспроизведение controlled ARM64 без закрытых игровых ресурсов:

```sh
bash tools/matrixgame_file_arm64.sh --bitmap
```

Для проверки всего пакета добавить `--package /absolute/path/to/robots.pkg`.
Host oracle: tests/probe_matrixgame_native_bitmap.py --binary <native-test>
--output <scratch-directory> [--package <robots.pkg>] [--reference-binary <x86-test>].
Reference требует настоящей неизменённой Win32 MatrixLib и libpng; CI не получает
игровые ресурсы. Локальные логи находятся в build/matrixgame/native-core/bitmap-*.

## Открытые ворота ТЗ

Этот блок обеспечивает часть MG asset pipeline и P0.3, но не доказывает Init/Run,
загрузку карты, настоящий бой, возвращение результата в Rangers, отсутствие freeze,
плавность или энергопотребление Switch. Кандидат 3772031 сохранён; аппаратная
проверка отложена пользователем. Полный объём ТЗ остаётся открытым.

## Финальная локальная фиксация доказательств

Финальный MSVC x64 replay выполнен с восстановленными стандартными C++ flags
`/DWIN32 /D_WINDOWS /EHsc` и официально установленными заголовками libpng 1.6.37.
Его real trace совпал с приведённым выше ASan/ARM64 SHA для всех 617 PNG.
DDS одного экспортированного RGBA изображения побайтово совпал с original x86:
SHA-256 cb2c58639a2ed4fdcd15d139269dc6bf4c1f0e3510e84cf2942f0f5f37e314c8.
Это проверка данного DDS control, не всех возможных файлов DDS.

Switch compile/link artifacts (не release и не выполнение):

- matrixgame-native-bitmap-link-probe.elf:
  1fb5de14e51a479402d11a8c98dc279e8842592de75f094fddb32f3657edd4ce;
  nm -u не выводит unresolved symbols.
- matrixgame-deko-mesh-probe.elf:
  2111652a969ad9a824a4788bca2036f3cc706fcab4fa28e995159be9328ef5e5;
  build-report status PASS, hardware NOT TESTED, nro_packaged false.

Повторный effective-source аудит основной игры: 380 translation units,
1387 scanned files, fixed_site_regression_failures пуст. Одна запись
REVIEW_NATIVE_SEMANTICS и пять UNSUPPORTED_WIN32_DLL_ABI остаются открытыми;
это не доказательство завершения всего P0.3. Pinned submodules не изменены.
