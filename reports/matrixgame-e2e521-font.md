# E2E-5.21: native debug font

25.10.2026. **Debug font программно реализован; E2E-5.21 в целом PARTIAL. SWITCH HARDWARE PENDING.** Native Init, загрузка карты и первый кадр не исполнялись.

## Исходный шрифт и реализация

Цепочка: CMatrixMap::m_DI → CMatrixDebugInfo::CMatrixDebugInfo → df::Create → debug_font::CreateFor → native::Font::Create → DekoDevice::DebugFontSession → native_texture::Resident → настоящий deko image upload. Рисование CMatrixDebugInfo использует исходные строки, цвета/fade, прямоугольники и flags; DrawTextWide проходит native layout и deko material draw. Release удаляет зарегистрированный font token/owned atlas. Никакого фиктивного COM объекта или пустой текстуры нет.

Оригинальный запрос: D3DXCreateFontW(height=25,width=0,FW_NORMAL,mips=1,italic=FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,PROOF_QUALITY,DEFAULT_PITCH,MS Sans Serif). На контрольном Windows HAL он выбирает исходный raster face, charset204: высота25, ascent20/descent5, average10, maximum21, internal leading5. Это не произвольный системный TTF. Исходная эвристика DI_AVERAGE_CHAR_WIDTH=9 не менялась.

Получены все **222 Unicode glyphs исходного face** посредством настоящего ID3DXFont::DrawTextW. Атлас512×448, 12152 ink pixels, coverage0/255, RGB255. Binary1053948bytes — новый build artifact, не изменение игровых ресурсов. Подробности D3DX API: [DrawText](https://learn.microsoft.com/en-us/windows/win32/direct3d9/id3dxfont--drawtext). Фактическая семантика проверена нашим HAL, не перенесена предположением из GDI DrawText.

Binary asset явно передаётся CMake через MATRIXGAME_DEBUG_FONT_ASSET; без него Switch configure завершается ошибкой. SDK objcopy встраивает readonly .rodata. Loader валидирует signature/метрики/map/границы/payload/nonempty coverage. Build asset: build/matrixgame/e2e521/font/windows/original-font.bin; его SHA и исходники закреплены в evidence JSON.

Поддержаны именно используемые debug-font форматы0x124/0x120/0x420, count=-1/явный count, empty/space/Cyrillic/Latin. Амперсанд literal; tab/CR/LF игнорируются для этих SINGLELINE flags, trailing spaces исключаются из CALCRECT ширины, empty VCENTER возвращает половину rect height. Это подтверждено D3DX. **Font linking вне покрытия исходного face и non-BMP не реализованы**: явный Unsupported вместо произвольной подмены; U+4E2D/U+0378 не выдаются за поддержанные. Общие игровые Text::CFont и другие fonts этим не закрыты.

Deko draw строит glyph triangles с исходными advance/metrics, UV и half-pixel correction; modulate texture×diffuse, SrcAlpha/InvSrcAlpha, nearest/no-mips/clamp, без lighting/depth/cull. Overlay имеет отдельный NativeRenderState и не переписывает захваченное состояние оригинала. DrawPacket удерживает resident images до завершения очереди.

Lost освобождает atlas reference; reset сначала делает настоящий upload и лишь затем публикует ресурс. Failed upload не выдаёт успеха. Новый device/session допустим только с идентичными метриками/map/glyphs/pixels; старая session закрывает только принадлежащие ей fonts. Device teardown завершает queue и закрывает callbacks до уничтожения texture arena. Корректность font lifecycle проверена; полное native presentation recreation пока зависит от отдельного provider.

## Что действительно исполнилось

| Уровень | Результат и предел |
|---|---|
| HOST | PASS: ASan+UBSan+leaks; production layout/loader/lifecycle, 699 pixel/metric goldens, исходный CMatrixDebugInfo ctor/T/Takt/lost/reset/dtor. Upload/draw в этом тесте — явно CPU model, без GPU/Init. |
| WINDOWS HAL | PASS: настоящий D3D9 HAL, 699 оригинал/native-layout draw сравнений с точным совпадением RGB/return/rect, все222glyphs; ещё4alpha cases и пиксельное сравнение после original+native lost/reset. Это font oracle, не deko execution. |
| ARM64/QEMU | PASS: те же699 CPU goldens и original owner/lifecycle, без аппаратного GPU/HID. |
| COMPILE/LINK | PASS devkitA64: native-entry ELF с оригинальными CGame::Init/PrepareMap/CMatrixDebugInfo, реальным font binary и deko backend, strong undefined=0. CMake configure/generate и font object target PASS; при object build выполнились43 dependency nodes, полного CMake engine rebuild не было. NRO не создавалась. |
| SWITCH HARDWARE | **NOT RUN / HARDWARE PENDING**: создание GPU/font upload, draw, device recreate, реальная карта/кадр и teardown на устройстве ещё не подтверждены. |

Проверены invalid/truncated/empty atlas, отказ первоначального upload, повторный lost, отказ/повторный reset, session/device rebind, запрет смены данных шрифта, закрытие прежнего/текущего owner, null/opaque registry lifetime. Полная Windows регрессия Init/Draw не повторялась: Windows branch backend и исходные общие Init/Draw алгоритмы не менялись; текущая смена font implementation относится к native branch. Контроль оригинального шрифта проведён адресно на HAL.

Команды воспроизведения: explicit Python313 + tools/matrixgame_font_checks.py windows, затем host/arm64/sdk; итог tools/matrixgame_font_audit.py. Для boundary: tools/matrixgame_font_map_inventory.py и tools/matrixgame_font_boundary_checks.py. Профили сохраняют command/exit/log/source/input SHA в build/matrixgame/e2e521/font/*/result.json. [Итоговый машинный аудит](matrixgame-e2e521-font-evidence.json).

## Следующий реальный блокер

[Полная последовательность Init, ownership, cleanup и оставшиеся providers](matrixgame-e2e521-init-sequence.md).

Первый известный source guard выбранного default map: **CGame::Init → CMatrixMap::PrepareMap → BuildTexUnions → CBottomTextureUnion::MakeFromBitmap → CTextureManaged::LoadFromBitmap(DXT1,6) → native_texture::Decode: native texture formatted decode conversion**. В реальном desert1_3e.cmap —6unions, 1534bottom records,23bitmaps; исходные составные unions1024×1024. Нужны настоящий BC1 encoder и ровно6исходных box mip levels. Подмена несжатым форматом не принята.

Негативный ASan/UBSan probe на настоящем исходный PNG из package подтвердил guard до upload/publication (0 uploads), но не исполнял полный BuildTexUnions/PrepareMap. До успеха на устройстве возможны более ранние ресурсные/IPC failures. Затем остаются конкретные original shadow/offscreen/stored-state/text/video/presentation paths; ReadCreationParameters в ApplyVideoParams находится уже после PrepareMap. Init успешным по линковке не объявлен.

Аудит повторно проверил15доступных protected inputs и107аппаратных файлов, frozen artifacts, NRO fingerprints, robots.pkg/CFG/frozen-cache source hashes. Один исторический внешний input G: остаётся UNAVAILABLE и не объявлен проверенным. Upstream не редактировался; основная/прочие NRO, frozen-комплекты, ресурсы и сейвы не изменялись. Коммит/push не выполнялись. P0 основной игры, причинность повторяющегося текста GI, manual-publish ENOENT и зависания Switch остаются OPEN; новых аппаратных данных нет.
