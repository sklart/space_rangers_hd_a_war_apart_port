# MatrixGame E610 r13 — однократная подготовка draw

После r12 в production Record найдена повторная CPU работа: ValidateDraw
вычислял и отбрасывал PixelFog, Stencil и combiner.Prepare, затем Record/материал
готовили их заново. Vertex.Prepare уже вызывался один раз. Новых аппаратных
профилей нет: это адресная CPU оптимизация, не доказательство крупнейшего
узкого места на Switch.

PrepareDraw сохраняет порядок validation и возвращает fog/stencil/combiner
для одного draw. ValidateDraw остаётся совместимым wrapper. Все 103 чтения
logical state, missing-word/unsupported/sampler/MSAA/line/point checks остаются.
Между draws состояние не кешируется. Texture residency/SDK upload не изменяют
render state. PrepareUniforms применяет alpha-target и сохраняет shade/shader/
block-size/allocation/packet проверки; старый Prepare сохраняет свой контракт.
Uniform layout, shaders, raster/attachments, draw order, физические лимиты и
completion ownership не меняются. R10 transition/r11 reuse/r12 lookup остаются.

## CPU доказательства

Reference — полный production CPP из r12 source
2bba518c902cea71df7320277f4380a88ffa7996 без правок. Frozen/candidate компилируются
отдельно с общей fixture, воспроизводящей CPU порядок Record. Это CPU probe,
не SDK GPU эмуляция. Header меняется только добавлением отдельного value type
и метода; поля/порядок NativeRenderState не меняются.

4173 случая: getters IDs 0..511, все 103 слова/восемь значений, setters/getters,
повреждение/удаление слов, samplers, point/line/MSAA/quality, 128 последовательных
изменений fog/stencil/factor/shade одного объекта, одновременно ошибочные поля.
На успехе хешируются все 4096 bytes, включая padding и alpha-target, и stencil;
на отказе категория и what(). Stdout сравнивается попарно на каждой платформе;
различающиеся std::exception сообщения разных платформ не смешиваются.

Host O2 ASan/UBSan/LSan, ARM64/QEMU O2 UBSan и Windows O2: результаты ниже.
Local type_info linker workaround сохраняет UBSan/vptr. Assertions on,
ffp-contract=off, без fast-math. Короткие COW/OOM/restore/ownership/payload/
packet/batch/journal проверки выполняются отдельно; большие бои не повторяются.

Benchmark: 100000 подготовок, включая vertex/fog/stencil/combiner/alpha-target
и запись реальных 4096 CPU bytes. Host O2 без sanitizers/LTO/fast-math, шесть
последовательных запусков ABBAAB. Итоговая медиана 126991 → 108941 мкс,
−14.2% времени участка. Checksum 4000058935132545121 совпал во всех запусках.
Это не прогноз FPS/Init Switch.

## Границы

HARDWARE PENDING: Init→transition→4 настоящих map frames, GPU/HID/звук/FPS
требуют пользовательского исполнения. Standalone entry не вызывает единую
Campaign/Gov, естественную победу и возврат Galaxy. Последние реальные r9 Draw
около 0.56 с не являются измерением r13. Ресурсы, saves, upstream, frozen,
основная NRO и старые пакеты сохраняются.

[Инструкция двух запусков](../docs/matrixgame-e610-prepare-r13-readme.ru.md).
Итоговые SDK/package/SHA сведения приведены ниже.

Итоговые semantic outcomes: 1774 отказа; host/ARM64 digest
10075058543430076524, Windows 2626683544331849612. Полный stdout совпал попарно.
Все три платформы и восемь коротких regression cases на каждой — PASS.
Evidence: build/matrixgame/e610-prepare-r13-checks/{host,arm64,windows,benchmark}/2/result.json;
build/matrixgame/e610-prepare-r13-regression/{host,arm64,windows}/1/result.json.
Benchmark завершён без параллельных тестов. Frozen 127881/126876/126991 мкс,
candidate 108941/109213/108142 мкс. Первичная разведочная серия дала -9.3%;
она сохранена отдельно в benchmark/1, итоговая с окончательной fixture — /2.

## Проверенная доставка r13

Source commit: `a6ec23bbf0b0a5d5a651816dc92adeebc01af5f0`.
Build ID: `a6ec23bbf0b0-a31acd8c06fd944a1dac`.

Стандартные SDK targets Quick Smoke и Interactive собраны заново в
`build/matrixgame/e610-prepare-r13/1`. devkitA64 compile/link — PASS;
неразрешённых strong symbols нет. NRO/ELF build IDs, три segments, icon/NACP,
встроенные shaders/fonts и source hashes проверены. В обоих ELF есть настоящие
PrepareDraw/PrepareUniforms; standalone entry не подменяется Campaign/Gov.

RelWithDebInfo, O2 + assertions + exceptions, без fast-math. Проверены
321 configured compile commands; неизменённый prebuilt Rangers/UI dependency
не считается заново оптимизированным. Link maps содержат по 23 объекта deko3dd,
без release deko3d; debug SetContext/RaiseError retained. Generated original
полностью совпадает с r12: относительно E601 изменены только прежние три файла
OriginalManagedTexture.cpp, Texture.cpp и OriginalTransition.cpp.

Проверены 702 own source hashes и 227 сохранённых старых файлов: прежние
ZIP/NRO/resources/frozen/основная NRO не изменены. Все три upstream pins и
подмодули сохраняются. [Машинное evidence](matrixgame-e610-prepare-r13-evidence.json).

ZIP: `dist/matrixgame-e610-switch-test-r13-prepare.zip` (10160769 bytes).
SHA256: `9961c7997bf796f2040360a09fa3809dcf9c4454503cf088cb5705baa9ae400c`.

| NRO | SHA256 | ELF build ID |
| --- | --- | --- |
| matrixgame-e610-quick-smoke.nro | `bee4d536a58990b0642754391f5755104613c62ce92d84680bc1c9302bd3c197` | `ec030dd352e3af8def396b61fd447dcf2aa40413` |
| matrixgame-e610-interactive.nro | `ae664654011f2cb0559cdf9f523770c8a3db8307bd2ce46028664fcb66a6fd1f` | `5bacaf061d8f296df8a0406ba59a80576692e2f8` |

В ZIP ровно шесть файлов: две NRO, README.ru.md, manifest.json,
SHA256SUMS.txt и build-info.json. Ресурсов/сохранений нет. На SD — отдельная
папка `/switch/matrixgame-e610-r13-test/`; прежние DATA/CFG roots остаются.
Запустить Quick, затем Interactive в Application Mode; выход Minus+Plus.
Собрать два новых каталога `/switch/matrixgame-diag/logs/<run-id>/` и фото.

**SOFTWARE PASS / HARDWARE PENDING**. Разработка этого адресного изменения
остановлена; дальнейшие исправления — по новому аппаратному профилю.
