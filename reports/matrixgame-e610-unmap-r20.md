# MatrixGame E610 r20 — covered buffer publication

Следующий подтверждённый CPU расход после r19: Resource::Unmap повторно
push/sort/merge уже покрытого диапазона при normal/no-overwrite updates.
Добавлена проверка покрытия первым инициализированным интервалом с прежними
unlock/version++. Остальные интервалы и расширения идут по прежнему пути.
Discard очищает initialized_ в Map и не может попасть в этот fast path.
Map, reserve до выдачи памяти, version overflow, guards, Storage, refs и
порядок upload/render не меняются. Нового кеша и состояния нет.
Оригинальные клиенты через big_buffer_bridge.hpp по-прежнему вызывают настоящий
Map/Unmap. Выигрыш относится только к повторным covered updates, не к первому
заполнению/Discard. Число таких операций на Switch пока неизвестно.

CPU probe сравнивает frozen r19 a2f87f3a40d72153a10b76a0fb4193535c935a9f
и текущий полный buffer_resources.cpp с независимой побайтовой моделью.
1033 обновления: disjoint/holes/adjacent/merge, subsets первого/второго
интервала, Discard/NoOverwrite, data/version, guards/errors, OOM-before-exposure,
allocation-free Unmap и retained binding. 4499748 coverage queries.
Host ASan/UBSan/LSan, ARM64/QEMU UBSan, Windows O2: semantic PASS,
checksum 1775817787135797556 одинаков на всех платформах и у frozen/current.
Первая версия fixture не привязывала Scope перед Restore; frozen тоже падал.
Исправлен тест, assertions сохранены; failing evidence в checks */1.

Предыдущий bitmap proxy кандидат дал менее 1% и отклонён. Его исходники,
результат и решение сохранены в build/matrixgame/e610-bitmap-r20-checks.
Production indexed body побайтово эквивалентен r19 после нормализации EOL.

Benchmark: 10 млн Map/write/Unmap на full initialized buffer и 10 млн на
частично initialized buffer, ABBAAB, host O2 UNDEBUG без sanitizers/LTO.
Это synthetic CPU publication, не frame/FPS/GPU/Init. Коэффициенты r10–r20
не перемножаются. Числа итогового замера, SDK/package и SHA добавлены ниже.

Большие Windows боевые сценарии не повторяются. Ресурсы/saves/upstream/frozen,
основная NRO и прежние пакеты сохраняются. HARDWARE PENDING. Standalone entry
не охватывает Campaign/Gov, естественную победу и возврат Galaxy.
[Два ручных запуска](../docs/matrixgame-e610-unmap-r20-readme.ru.md).

## Итоговый CPU замер

Benchmark/2 ABBAAB: full frozen 118255/117177/118671 мкс,
candidate 45139/46704/46221; median 118255 → 46221 мкс (−60.9%, 2.56×).
Partial frozen 122544/116117/114783; candidate 44761/47058/44784;
median 116117 → 44784 мкс (−61.4%, 2.59×). Checksum
4264432143182133122 совпадает во всех шести runs. Предварительная серия /1
дала близкий результат; runner затем сократил link inputs до actual Resource,
финальная серия /2 использует неизменённые tested source/tool hashes.
Это экономия порядка 7 нс на covered update в этом host probe; весь кадр
таким коэффициентом не ускоряется. Доля Map/Unmap на Switch ещё не измерена.

Semantic /2 и ownership /1 — PASS на трёх платформах. Неуспешные /1 и исходная
fixture сохранены в checks/rejected-fixture; причина — отсутствовавший Scope.
Ошибки не подавлялись, assertions не удалялись. Map/version overflow guard
сохранён побайтово; искусственный перебор 2^64 versions не выполнялся.

## SDK и пакет

Source commit: `33356be5cf30e47513ec66bb8ce7a885c3bcfa8d`.
Build ID: `33356be5cf30-1755ce5ff8d0dfeaf5a5`.
`devkitA64 compile/link` обеих NRO — PASS, strong undefined = 0.
321 configured translation units, O2/UNDEBUG, exceptions, без fast-math;
prebuilt Rangers archive сохранён и не выдаётся за пересобранный O2.
23 настоящих debug deko3dd object members / 0 release deko3d, mixed=false.
Встроенные fonts/shaders, NRO assets/segments/NACP/ELF→NRO build IDs проверены.
Generated original files идентичны r19 (всего 3 исторических delta vs E601).
710 own source hashes проверены; единственное поведенческое production
изменение — 5 строк guard в Unmap. Ещё изменены две r20 NACP titles и добавлен
CPU runner. Остальные build inputs r19 сохранены.

В каждой ELF 34 настоящих call sites Resource::Unmap. ARM64 disassembly
подтвердил covered comparison → strb unlock/add+str version/ret без calls
и sort; r19 indexed input hoist и direct DecodeTo сохранены в actual ELF.
276 прежних защищённых артефактов проверены по SHA, unchanged.
Основная NRO, game/frozen hashes проверены SDK/package gate.

ZIP: `dist/matrixgame-e610-switch-test-r20-unmap.zip` (10162444 bytes).
SHA256: `3394e28fd2f4506aa2d01b78d11fc2b5cc71660dcf1ce718dd09d6f8897dd14f`.
В архиве ровно две NRO + README.ru.md + manifest.json + SHA256SUMS.txt +
build-info.json. Ресурсов и saves нет. Manifest связывает source commit,
ELF/NRO/build IDs, inputs и runtime HARDWARE PENDING.

| NRO | SHA256 | ELF Build ID |
| --- | --- | --- |
| matrixgame-e610-quick-smoke.nro | `aa2193cdf2aa7ead5aa76fdf694fdb92fb8bf061aded4ef46ca4a1e70aca8a6e` | `9f7867fed0330c22da0df035270f1199f315cced` |
| matrixgame-e610-interactive.nro | `d737a59f7cfd1d8992431955f6e6064bfa36b5dd263bded23cca7b1e6257494a` | `ad404ada2d1d2ae5ca546948e995d131f281980f` |

## Воспроизведение

CPU: `python tools/matrixgame_unmap_checks.py host` (также arm64/windows,
отдельно `--ownership`); `python tools/matrixgame_unmap_checks.py benchmark`.
Требуются те же установленные WSL g++, ARM64 GCC15/QEMU и MSVC2022.
SDK: `python tools/matrixgame_current_diagnostic_sdk.py --output-root
build/matrixgame/e610-unmap-r20` (новый numbered каталог, прежний не заменяется).
Package: `python tools/matrixgame_e610_freeze.py package --build
build/matrixgame/e610-unmap-r20/1 --source-commit 33356be5cf30e47513ec66bb8ce7a885c3bcfa8d
--output dist/matrixgame-e610-switch-test-r20-unmap.zip --readme
docs/matrixgame-e610-unmap-r20-readme.ru.md --rangers-root
sdmc:/switch/space-rangers-hd-a-war-apart/game` (для нового output использовать новое имя).
Не запускать сборку/профилирование параллельно с timing probe.
Локальный повторный gate: build/matrixgame/e610-unmap-r20-checks/verify_delivery.py
с source commit как аргументом; evidence хранит commands/source hashes/disassembly.
[Полный evidence](matrixgame-e610-unmap-r20-evidence.json).

SOFTWARE PASS / HARDWARE PENDING. Следующий пользовательский замер должен
проверить Quick → 4 реальные map frames и Interactive → управление/выход.
Init, Draw, физические HID/GPU/audio и переход на карту пока не приняты.
Единая Campaign/Gov, естественная победа и возврат результата в Galaxy этим
standalone пакетом не проверяются. Новая разработка на этом шаге остановлена.
