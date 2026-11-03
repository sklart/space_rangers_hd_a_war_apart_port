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
Итоговые SDK/package/SHA сведения добавляются после свежей сборки.

Итоговые semantic outcomes: 1774 отказа; host/ARM64 digest
10075058543430076524, Windows 2626683544331849612. Полный stdout совпал попарно.
Все три платформы и восемь коротких regression cases на каждой — PASS.
Evidence: build/matrixgame/e610-prepare-r13-checks/{host,arm64,windows,benchmark}/2/result.json;
build/matrixgame/e610-prepare-r13-regression/{host,arm64,windows}/1/result.json.
Benchmark завершён без параллельных тестов. Frozen 127881/126876/126991 мкс,
candidate 108941/109213/108142 мкс. Первичная разведочная серия дала -9.3%;
она сохранена отдельно в benchmark/1, итоговая с окончательной fixture — /2.
