# MatrixGame E610 r14 — упорядоченный проход logical registers

Следующий подтверждённый CPU расход после r13 — повторные std::map::at поиски
в PrepareDraw для 103 logical words. Metadata уже dense/constexpr (r12),
результаты fog/stencil/combiner переиспользуются внутри draw (r13), но raw values
оставались tree lookup для каждого слова. Hardware profile новых версий нет:
речь о CPU workload, а не крупнейшем bottleneck или FPS Switch.

Один const iterator проходит map по возрастанию ID. Для raw words читается
тот же value; raster/vertex/cull/shade/factor продолжают использовать Render.
Все 103 слова читаются в прежнем порядке, даже inactive/missing. Если слово
отсутствует, вызывается настоящий registers.at(id): сохраняются тип и what()
стандартного исключения платформы. Лишние неизвестные ключи пропускаются как
прежде. Strict ascending NativeRenderWords проверяется static_assert;
при нарушении порядка будущая сборка завершится ошибкой.

PrepareClear/PixelFog/Stencil выполняются до прохода как раньше; затем остаются
те же DrawRules/point/line/combiner/sampler проверки. Никаких результатов
валидации/values между draws не кешируется. Header/layout/API NativeRenderState,
COW/snapshot/restore, выделение памяти и владение ресурсами не меняются.
Shaders, packet layout, GPU draw/order/limits и оригинальная игровая логика
сохраняются. Ресурсы, сохранения, upstream, frozen, основная NRO и старые
пакеты не изменяются.

## Измерения и отвергнутый вариант

Сначала исследован Multiply в fixed_vertex.cpp. Перестановка циклов дала около
9% isolated gain, развёртка k — около 2x isolated gain, но оба варианта не дали
подтверждённого выигрыша полной подготовки draw. Они **отклонены**, Multiply
побайтово восстановлен до r13. Исходники эксперимента и обе ABBAAB серии
сохранены в build/matrixgame/e610-matrix-r14-checks; в NRO их нет.

Для register walk итоговая host O2 медиана полной CPU-подготовки:
111397 → 89018 мкс / 100000 draws (−20.1%). Вычисляются настоящие production
PrepareDraw, vertex uniforms и uniform_packet::Write; это CPU bytes, не GPU
исполнение. Checksum 4000058935132545121 совпал во всех шести запусках ABBAAB.
Итоговый замер выполнен без параллельной нагрузки; SDK сведения приводятся ниже.
Assertions включены; без LTO/fast-math, ffp-contract=off.

## Проверка семантики

Полный frozen native_device_state.cpp из r13 source
a6ec23bbf0b0a5d5a651816dc92adeebc01af5f0
компилируется отдельно от candidate с теми же неизменёнными headers.
4197 случаев: все 103 words/8 values, getters 0..511, setters, raw corruption,
missing/inactive words, sampled samplers, points/lines/MSAA/quality, successive
fog/stencil/shade/factor changes, simultaneous invalid fields, 24 дополнительных
случая extraneous keys до/между/после известных ID. Сравниваются exact 4096 bytes
включая padding/alpha-target, stencil fields и категория/what() исключений.
Stdout сравнивается попарно внутри каждой платформы; hashes разных платформ
не смешиваются. Отдельно тот же frozen/current comparison для native_state
COW/snapshot/OOM/frame/restore/ownership теста. Большие бои и полные suites
не повторяются, неизменённые upload/batch/journal checks остаются evidence r13.

## Границы

SOFTWARE проверки не закрывают HARDWARE PENDING. Init→transition→4 настоящих
map frames, физические GPU/HID/audio и FPS требуют пользовательского запуска.
Standalone entry не вызывает единую Campaign/Gov, естественную победу и возврат
Galaxy. Последние реальные r9 Draw около 0.56 с не являются измерением r14.
[Два ручных запуска](../docs/matrixgame-e610-register-r14-readme.ru.md).

## Итоговые CPU результаты

4197 cases / 1774 rejections: host и ARM64 digest 17500997310984091660;
Windows 6482887211588844204. Frozen/current stdout совпал на каждой платформе.
ASan/UBSan/LSan host, ARM64/QEMU UBSan, Windows O2 — PASS. Assertions retained.
COW/snapshot/ownership: restores=9, capture_OOM=2, COW_OOM=1,
restore_allocations=0; failed/expired/frame guards retained — PASS.
ARM64 local type_info version script сохраняет UBSan/vptr.

Final benchmark/2: frozen 110255/111397/113082 мкс, candidate
90744/88094/89018 мкс; медианы 111397/89018 (−20.1%). Первичная серия /1 дала
−17.2%, она сохранена. Checksum 4000058935132545121 одинаков во всех запусках.
Evidence paths: build/matrixgame/e610-register-r14-checks/{host,arm64,windows}/1,
{host-ownership,arm64-ownership,windows-ownership}/1 и benchmark/2.
Поддерживаемый runner: tools/matrixgame_register_walk_checks.py.


Проверка воспроизводимости выявила EOL-only остаток после отклонённого Multiply:
fixed_vertex.cpp был восстановлен с CRLF вместо исходного LF. Файл затем
побайтово восстановлен до r13 SHA c2394c37e96738367b7730c5dfa3927158de7aad6081304e7a8bf2f53e305d35.
Проверенный CPU snapshot CRLF сохранён в compiled-fixed-vertex-crlf.cpp, его
нормализованные bytes точно равны итоговому LF файлу (eol-proof.json).
Остальные проверенные source bytes совпадают точно. CPU логи не переписаны.
SDK build /1 успешно скомпилировался, но source hash gate правильно отклонил
изменённый input; эта попытка сохранена, NRO из неё не выдаются. Итоговый пакет
строится в /2 из окончательных исходников с новым fingerprint/build ID.

## Проверенная доставка r14

Source commit: `573cae7b5d968e5c68e9ecd643865141c295fb2b`.
Build ID: `573cae7b5d96-c2a97dc7ba02cac1ba0d`.

Итоговые стандартные SDK targets — build/matrixgame/e610-register-r14/2.
DevkitA64 compile/link — PASS; unresolved strong symbols отсутствуют.
ELF/NRO build IDs, headers/segments, icon/NACP, встроенные fonts/shaders и
source fingerprints проверены. PrepareDraw реально связан с обеими NRO;
standalone entry и границы Campaign/Gov не изменены.

Compiler provenance: 321 configured TUs, RelWithDebInfo/O2/UNDEBUG, assertions и exceptions retained, без fast-math.
Неизменённый prebuilt Rangers/UI archive не пересобирался и не объявляется
оптимизированным. В каждом link map 23 объекта deko3dd, без deko3d; реальные
SDK debug context/RaiseError retained. Generated originals побайтово совпадают
с r13; относительно E601 остаются только прежние три overlay delta.

Проверены 703 own source hashes и 234 прежних защищённых файла; ресурсы,
saves, frozen/upstream pins, основная NRO, старые NRO/ZIP не изменены.
[Полное машинное evidence](matrixgame-e610-register-r14-evidence.json).

ZIP: `dist/matrixgame-e610-switch-test-r14-register.zip`, 10161875 bytes.
SHA256: `ba6abd2e629dd023a83b1e7cd4e9d5dade8dac1cecbc9fd2832641f83df34cdb`.

| NRO | SHA256 | ELF build ID |
| --- | --- | --- |
| matrixgame-e610-quick-smoke.nro | `862ea63e1a49f0229ac0f17ce4da2e0d49934bb1082355312265cb5709d87fe3` | `7a4385fbf133b45261e9b7899fc4464a8c5241ed` |
| matrixgame-e610-interactive.nro | `e24212ca2a5e906338c61de690c339558509e97ee833a9cae7bab5580a9dbd54` | `37fc018feb7a6f86fa573055ea1d1419c26979bd` |

Архив содержит две NRO, README.ru.md, manifest.json, SHA256SUMS.txt и
build-info.json. Ресурсов и сохранений нет. Копирование NRO — в отдельную
папку `/switch/matrixgame-e610-r14-test/`, roots DATA/CFG прежние.
В hbmenu **MatrixGame r14 Quick Smoke** и **MatrixGame r14 Interactive**.
Запустить Quick, затем Interactive в Application Mode; выход Minus+Plus.
Передать два новых каталога `/switch/matrixgame-diag/logs/<run-id>/` и фото.

**SOFTWARE PASS / HARDWARE PENDING.** Адресная оптимизация завершена;
следующие изменения — по новому аппаратному профилю.
