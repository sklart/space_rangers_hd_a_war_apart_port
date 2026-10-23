# E2E-5.19 — original input.cpp и следующие зависимости CMatrixMap

23.10.2026. Локальный master, без push/fetch. Предыдущий этап: полный
MatrixLogic.cpp (`9f5763f`). Полная карта и CGame::Init **ещё не исполняются**.

У Map.cpp за первым windows.h обнаружился второй такой include, в input.hpp.
Карта действительно вызывает `Input::isKeyPressed(KA_AUTO)`; это не лишний
неиспользуемый header. Теперь весь исходный input.cpp включён в существующий
matrixgame-native-cache и используется общим bootstrap вместе с настоящим
g_Config. Новый input manager, OS-клавиатура или фиктивные игровые события
не создавались.

Все четыре исходные функции onKeyDown/onKeyUp/isVKeyPressed/isKeyPressed и
исходный static bitset256 **не переписывались**. Canonical SHA256 input.cpp
`fa237ded3e940dac71f68531f5d3ed8fbbdfd071bdae713f8ff286c540330e0b`
совпадает с pinned upstream после только CRLF→LF. Изменены input.hpp и
include-пролог MatrixMap.cpp, без изменения тела методов карты.

В header шесть SDK key constants представлены типизированными исходными
virtual-key значениями. Это прежний протокол, не SDL scancodes. Сохранена
оригинальная VKey2Char, включая отображение OEM_2 в обратную косую черту.
Все scan-key и ASCII aliases сохранены; preprocessor-условий, использующих
перенесённые aliases как числа, в исходниках не обнаружено.

Первый вариант оставлял windows.h под _WIN32 и успешно проходил GCC/runtime,
но **MSVC выявил реальный conflict**: SDK HWND/HINSTANCE и established opaque
идентичности MatrixGameDll.hpp. Финальный input header SDK-free на всех
платформах. SDK key assertions выполняются в отдельной TU с windows.h и
input.hpp, не смешивающей их с opaque game/DLL declarations. Эта коррекция
проверена реальным MSVC x86, а не Python assert по имени файла.

## Фактические gates

| Проверка | Результат |
|---|---|
| Host ASan/UBSan/leak, общий bootstrap | PASS двух сессий с настоящим SetDefaults → configured KA_AUTO → down/held/up |
| ARM64/QEMU, тот же driver | PASS двух сессий и исходной VKey2Char |
| MSVC x86 | Полная original input.cpp COMPILE PASS; отдельная SDK key-protocol TU COMPILE PASS; Windows game link/runtime NOT RUN |
| devkitA64 | Input + изменённая game-check TU COMPILE/LINK PASS; все4 original Input symbols retained; strong undefined=0 |
| Физический Switch, CFormMatrixGame input, карта/бой | HARDWARE_PENDING / NOT EXECUTED |

Проверка использует настоящую m_KeyActions[KA_AUTO] после исходного SetDefaults,
не присваивает ей тестовый binding. Нажатие/удержание/отпускание вводятся через
оригинальный API и проверяются обоими original predicates. Исходный процессный
bitset и его жизненный цикл не менялись; held state явно отпускается в fixture.
Это **не доказательство** физического HID, клавиатурной раскладки, потери фокуса
или доставки через полноценный CFormMatrixGame. Основной ввод/OSK не менялся.

После MSVC failure сохранён прежний candidate; финальный пересобран только
для двух затронутых TU. Для host/ARM64/SDK переиспользованы принятые Logic
архивы и неизменённый main driver, с отдельными outputs/archives. Старые CList,
BigVB/IB, RoadNetwork standalone suites не запускались. В общем bootstrap
прежние prerequisites остаются необходимой частью интеграционного маршрута.
В Logic object сохраняется ранее оговорённое исключение UBSan vptr; новый
input и изменённый game-check на host инструментированы address/undefined.

Hashes финальных кандидатов и команды находятся в
`matrixgame-e2e519-map-input-evidence.json`. Сборка выполнена до коммита с dirty
исходниками, embedded Git SHA не заявляется. SDK ELF совпал по байтам с первым
SDK-вариантом, поскольку Windows-only header correction не меняет его код;
финальные input/header source hashes всё равно записаны отдельно.

## Точная следующая граница

Final full MatrixMap compile доходит до `MatrixMap.cpp:789` prepared source:
не объявлена **D3DXPlaneIntersectLine** в оригинальном UnitPickGrid. Следующий
вызов — UnitPickWorld; далее неперенесённые sampler/device operations, включая
MipMap LOD bias readback и fixed-function render states. Диагностический
compile этой полной TU остаётся **FAIL**, он не объявляется проверкой карты.
После снятия этих compile dependencies всё ещё нужны original member owners,
полный Clear/object factories и настоящий device lifecycle до CGame::Init.
g_MatrixMap не публикуется. Fake map, cursor/provider или успешный Init не вводились.

Артефакты: `build/matrixgame/e2e519/map-native-baseline`,
`map-input-candidate-audit`, `map-input-final-map-audit`,
`map-input-integration-final/{host,arm64}`, `map-input-msvc-final`,
`map-input-sdk-final`. Входы прежние: model-1.vo/plain.png/pinned CFG/robots/data.txt.
Source view содержит240 файлов; относительно предыдущего принятого Logic view
изменены только MatrixMap.cpp и input.hpp.

Основная NRO/matching ELF, оба frozen ZIP, пользовательские saves и ресурсы
сохранены. Новая NRO не создавалась. Причины аппаратного повторного intro-растра,
двух ENOENT и полного freeze карты остаются отдельными OPEN; установленный
host shutdown defect исправлен ранее и этим этапом не пересматривается.
