# MatrixGame E2E-5.24 — оригинальный UI text

Блокер Text::Render → DeviceIdentity снят для исходного Top/mm / Font.2Ranger.
Общий native Init остаётся PARTIAL: Switch map/frame не исполнялись.
Аппаратных проверок и новой NRO нет.

## Реализация

Сохранены оригинальные Text/Font.cpp, Parser.cpp, Render.cpp, eager cache пяти
font instances, цветовые токены, отступы, layout и bitmap copy. Generated source
не менялся: 252 файла совпадают с предыдущим pin.

DeviceIdentity использует стабильную identity настоящей texture session только
как ключ исходного font cache. Это зарегистрированные native owners без COM
vtable; legacy D3D handles не выдумываются. DekoDevice предоставляет immutable
font assets через явный provider.

Экспортёр приватно регистрирует оригинальный Rangers.ttf в процессе Windows,
проверяет лицо и побайтное равенство GetFontData исходному TTF. Verdana 12/13/14
— именно исходные запросы BuildFonts. SFNT, маски, map/metrics/kerning и SHA256
сохранены в evidence. Rangers10: 156 уникальных глифов; каждый размер Verdana:
894. Четыре проверенных бинарных набора занимают 743598 байт.

native_text.cpp читает и проверяет реальные glyph coverage и метрики, учитывает
GDI kerning, исходный CALCRECT, перенос слов/символов/числовых границ,
выравнивание блока/строк, return values D3DX и SRCALPHA blending всех BGRA
каналов. Prefix sums исключают квадратичное измерение длинных строк.

native_text_backend.cpp владеет зарегистрированными font/surface/target.
Surface удерживает texture после удаления её token; renderer удерживает surface.
Draw пишет настоящий CPU raster target. EndScene выполняет существующий native
texture upload в кандидат и лишь затем публикует bytes/image. OOM сохраняет
старые данные, допускает retry. Исходный AutoRelease освобождает владельцев
при исключении. Результат возвращается через исходные Lock/bitmap Copy и далее
MergeWithAlpha/Copy UI.

Оригинал сразу читает промежуточный текст обратно в bitmap, поэтому здесь
используется адресная CPU растеризация с настоящим native texture upload.
Общие shadow/minimap surfaces этим backend не подменены. Screen render state
не меняется. Glyph data сохраняются в CPU при reset; managed image invalidation
и повторный preload проверены. Новая device session меняет cache identity и
вызывает исходную очистку/пересоздание font cache.

Файлы: native_text.hpp/.cpp, native_text_backend.cpp, native_text_asset.cpp,
text_backend.cpp, platform_switch.hpp/.cpp, CMake; тесты и инструменты
test_matrixgame_native_text.cpp, matrixgame_text_font_export.cpp,
tools/matrixgame_text_*.py.

## Что действительно исполнялось

| Проверка | Доказательство |
| --- | --- |
| WINDOWS HAL | 17940 точных сравнений BGRA, rect и return с настоящим D3DX9: 1164 Rangers10 + 3×5592 Verdana. Все исходные glyphs, кириллица, kerning, пустая строка, пробелы, tab/CRLF, wrap, длинное слово, числа, align, opaque/half-alpha/alpha=0. Windows text_backend отдельно скомпилирован. |
| HOST | ASan/UBSan и assertions: goldens и исходный Text::Render «Меню» 75×20 до bitmap; cleanup, EndScene upload exception, retry, удержание surface/texture, неизменность прежнего image при отказе, managed reupload, cache/device rebind. Upload — CPU fault model, не deko3d execution. |
| ARM64/QEMU | Те же production backend, исходные Text TUs, goldens и ownership/failure/rebind проверки с assertions. Не Switch GPU. |
| CMake/CTest | Чистая конфигурация отдельного native-text target, явные original include dependencies, ASan/UBSan/-UNDEBUG; один адресный CTest. |
| COMPILE/LINK | Полная отдельная native-entry ELF devkitA64 с CGame::Init, PrepareMap, Text::Render, DeviceIdentity, native Draw/EndScene/assets: 176 link inputs, 0 strong undefined. Не runtime. |
| SWITCH HARDWARE | NOT RUN / HARDWARE_PENDING. Native Init, GPU upload текста, UI, карта и Present на консоли не подтверждены. |

Полный Windows Init/frame повторно не запускался: изменения поведения относятся
к native ветке, Windows вызовы и общие Init/Draw/resources contracts сохранены.
Windows HAL здесь — адресный эталон шрифта, не новый Windows продукт.

ELF: build/matrixgame/e2e524-text/sdk/matrixgame-native-entry.elf.
SHA256: 9315b9351005d5a10117097e57d2eed8058de178fbca7c38ed6e7c3230f7ed57.
Команды, exit codes, времена процессов и hashes:
build/matrixgame/e2e524-text/{windows,host,arm64,sdk,cmake-host}/result.json.
Параллельные времена процессов нельзя складывать в рабочие часы; отдельный
таймер активной работы в этом продолжении не вёлся.

Воспроизведение: tools/matrixgame_text_checks.py windows, затем host, arm64,
sdk; tools/matrixgame_text_cmake.py и tools/matrixgame_text_audit.py.
SDK CMake требует MATRIXGAME_TEXT_FONT_ASSETS — папку проверенных
font0.bin … font3.bin. Windows экспортёр создаёт их, только читая оригинальный
TTF. Эти targets не создают NRO.

## Следующая настоящая граница Init

Исходный порядок CGame::Init:

1. static_init, heap/package/CFG, CacheInit, EnterPlatform;
2. render/config/map constructors, LoadSide, CIFaceList;
3. RobotPreload, stor.Load, PrepareMap;
4. cannon/AI/hint/menu preparation, ApplyVideoParams;
5. CInterface::Load Top/MiniMap/Radar/Base/Main/Hints, menu graphics;
6. transition BeginScene/Render/EndScene/Present;
7. InitEffects, CreatePoolDefaultResources(true), lights/selection/dialog.

Top/mm прошёл адресное исполнение через исходный Text::Render. Это не означает
исполнения всех перечисленных шагов на Switch.

Следующая source-traced ветка при исходной настройке projected shadows=1:

CGame::Init → CMatrixMap::CreatePoolDefaultResources(true) → StaticPrepare2 →
object::RNeed(MR_ShadowProjTex) → CVectorObject::CalcShadowTextureWOMat →
offscreen::GetTarget(0,&oldTarget).

Точное исключение: **offscreen target query unavailable**. До него
GetViewport/SetViewport у DekoDevice подключены. Нужны настоящие owners
color/depth targets, создание shadow surface/texture, переключение attachments,
shadow indexed staging draw, синхронизация, readback/Lock и восстановление
targets/viewport. Конкретный первый объект зависит от карты; граница
установлена по исходникам, полного native Init runtime trace пока нет.

Дальше CMinimap::RenderBackground требует render texture usage=1/XRGB8/default
pool, surface bindings и readback/cache; native_texture::Create честно
отклоняет render-target usage. StoredStateBackend нужен
CMatrixMap::ReleasePoolDefaultResources для map release/reset/recreate,
отдельно от уже существующего внешнего lease. Эти guards не заменены пустыми
успешными обработчиками.

Воспроизведено покрытие исходных font faces и форматы, вызываемые Text.
Font linking вне лица и сложный script shaping не реализованы. Например
Rangers не содержит U+0451; native backend явно отклоняет такой glyph.
Combining/RTL shaping в многосимвольных строках также отклоняется явно.
В защищённом iface.txt U+0451 не встречается; это не доказывает покрытие
динамических строк.
Произвольный Unicode/UI этим не закрыт. Проверенный Top/mm не затронут,
но последующим captions/hints нужна проверка применимости.

## Сохранность и открытые проблемы

Финальный audit проверяет защищённые входы по прежним fingerprints: основную
NRO, frozen, пользовательские saves, robots.pkg/CFG, GI frozen source,
107 аппаратных входов; upstream pin/чистоту и родительский HEAD.
Изменения только в рабочем port/tests/tools/docs/reports и новых build outputs.
Commit/push, новая аппаратная NRO и тест Switch не выполнялись.

P0 повторяющегося текста, причинная роль GI cache, manual-publish ENOENT
и исторические Switch hangs остаются OPEN. Шрифтовой host PASS их не закрывает.
