# MatrixGame: нативный перенос и Windows x86 reference

## Текущий статус на 23.10.2026

E2E-5.19: единый bootstrap исполняет оригинальные CMatrixConfig::SetDefaults/Clear,
проверяет владение ресурсами и повторный вход: host ASan/UBSan и ARM64/QEMU PASS.
Теперь из полной original TU исполняется Release `static_init`: CMain/Base,
VO/shadow/form и game static initialization перед созданием game heap/cache.
54 исходных определения данных перенесены в общую TU с прежними типами/значениями
effective sources; stale EXITLOOP, отказ при живой чужой форме и reentry проверены.
В тот же bootstrap включена полная оригинальная MatrixLoadProgress.cpp:
исполнены constructor, SetCurLP/SetCurLPPos и исходные веса 70/300/150/30.
Callback проверен через RangersInterface fixture; настоящая UI хозяина не запускалась.
Destructor снимает только собственный g_LoadProgress: проверены stack unwind,
чужой publisher, cleanup и повторный вход под ASan/UBSan и ARM64/QEMU.
Полная оригинальная MatrixGame.cpp перешла COMPILE_FAIL → COMPILE_PASS и входит
в bootstrap вместо выбранного GameVideoNative.cpp. Host/QEMU вызывают её
ApplyVideoParams и проверяют отказ первой операции без устройства, cleanup/reentry.
devkitA64 compile/link PASS; Init/Run/map и физический GPU draw ещё не выполнялись.
Полная оригинальная MatrixConfig.cpp также компилируется и включена в bootstrap.
ApplySettings исполняется; ReadParams на настоящем pinned data.txt разбирает
Config и пять Cursors, затем останавливается на обязательном gamma/device call.
Host ASan/UBSan и ARM64/QEMU подтверждают реальные ресурсы, retry и cleanup.

Legacy L3GInitAsDLL/L3GDeinit теперь определены через тот же NativeInitRuntime.
Зарегистрированные providers должны подтвердить реальные SDK identities;
без SDK проверены явный отказ, repeated deinit и сохранность CPU/cache/data.
Legacy flags/window/module/configured dimensions ссылаются на общий platform state.
Положительный SDK runtime нового legacy entry пока NOT RUN.

Текущие Windows x86/x64 сборки полного объекта — **LINK_BLOCKED: 35 методов**
Init/Deinit/static_init. Это временная регрессия bootstrap после включения полной
TU; новые WinMM/gamma runtime-проверки NOT RUN. Полный legacy SDK context, карта и
reset-safe presentation lifecycle остаются незамкнутыми.
Полный Init ещё нельзя вызывать поверх уже живого native owner: требуется
согласовать его повторные static_init/heap/cache/data действия с одним lifecycle.

В предыдущем Windows x86 срезе подтверждены настоящие DLL/window/caps calls,
sampler/S3D_Default и полная DeviceState.cpp с восстановлением 207 state values.
Effect/map buffers используют существующий native staging context; это не GPU upload.
Полные 3G/Texture/Camera declarations компилируются, SDK compile/link PASS.
Там же исправлен memset живого std::wstring в исходном cursor constructor;
на Windows настоящие SetCursor/GetCursor подтверждают Clear/detach/reentry restore.
Отдельный game heap принадлежит тому же owner; host/QEMU проверяют безопасный
cursor-prefix Unsupported, а не положительную работу системного курсора.
Предыдущий выбранный CGame::ApplyVideoParams выполнял настоящие SDK-запросы
параметров устройства, режима адаптера и размеров окна; оконная/полноэкранная
ветви останавливаются до изменения окна с явным Unsupported. Отказ, rollback
общих параметров и повторный вызов проверены на Windows NULLREF.
Timer begin/end теперь используют lease общего platform state с настоящим WinMM
provider на Windows и явным Unsupported без него. Его положительный Windows
runtime остаётся NOT RUN до восстановления полной компоновки.
GPU/Switch HARDWARE_PENDING; production NRO не менялась.
[Актуальная граница bootstrap и evidence](../../reports/matrixgame-e2e519-engine-bootstrap.md).

Предыдущий срез: исходный frame/timing-путь CVectorObjectAnim подключён к skinned Draw.
Явный no-billboard вариант сохраняет Takt/NextFrame/BeforeDraw/Draw; проверен
реальный цикл Terron из 41 кадра, 4100 мс, с выдачей четырёх draw-групп.
Host ASan/UBSan и ARM64/QEMU PASS, SDK link PASS; GPU HARDWARE_PENDING.
Это не полный animator с lights/shadows и не запуск CGame::Init.
[Анимация: выполненные методы и граница](../../reports/matrixgame-native-animation-20261022.md).

Предыдущий срез: исполняются оригинальные skinned LoadSpecial → BeforeDraw → Draw через
настоящую таблицу SSkin callbacks и native indexed submission. Host ASan/UBSan
и ARM64/QEMU PASS, включая частичный отказ второго материала и bad_alloc.
SDK link с настоящим deko draw PASS; GPU execution HARDWARE_PENDING.
Поддерживается native ordinal/base pass, полный CGame::Init ещё не запущен.
[SSkin callbacks: методы, ownership и граница](../../reports/matrixgame-native-callbacks-20261022.md).

Предыдущий шаг: перенесены исходный разбор skin resources и UV lifecycle Takt,
подключена загрузка текстур по именам поверхностей VO. Host ASan/UBSan и
ARM64/QEMU PASS, включая отказы ресурсов/аллокации и очистку. SDK link PASS;
GPU execution HARDWARE_PENDING. Render path пока ordinal single-pass, без
полных SSkin callbacks, side/gloss/mask effects и CGame::Init.
[Skin resources: новый срез и граница](../../reports/matrixgame-native-skin-20261022.md).

Продолжение E2E-5.18: кэш и оригинальный VO теперь соединены с подготовкой
кадра и реальным deko draw через общий owning lifecycle. Новый сценарий с двумя
объектами в общих буферах прошёл host ASan/UBSan и ARM64/QEMU. SDK ELF включает
создание устройства, загрузку текстур и draw; GPU entry offline не вызывался.
Это явные single-pass материалы, без полного CSkinManager и CGame::Init.
[Новая граница и оставшиеся блокеры](../../reports/matrixgame-native-frame-20261022.md).

Обновление 22.10, E2E-5.18: полный release Cache.cpp перенесён на явные portable
factories; compile inventory — 14 PASS / 92 FAIL из того же набора 106 units.
Новый Init/cache/CPU-3G-prefix → resource load → Deinit/reentry прошёл host
ASan/UBSan и ARM64/QEMU, включая два настоящих calloc failure. SDK связал
Cache/material lease с настоящим deko upload, GPU execution PENDING.
Managed Unload сохраняет storage до уничтожения; default освобождает его сразу.
Полные L3GInitAsDLL/CGame Init/Run не выполнялись: первый compile blocker —
VectorObject.hpp/d3d9.h, следующий runtime contract — platform/device binding.
[Отчёт и точная граница](../../reports/matrixgame-e2e518-native-init.md).

Ниже — предыдущая граница E2E-5.17:

Перенос остаётся частичным. Реальные CPU-компоненты MatrixLib, ресурсная
цепочка, навигация, Base/Form lifecycle и BigVB/BigIB проверяются отдельно
от полной игры. Новый шаг E2E-5.17 — явный CPU-срез исходного CVectorObject:
StaticInit → конструктор → Load/LoadSpecial(OLF_NO_TEX) → обход вершин,
анимаций/матриц и GetBound → Unload/повторная загрузка → деструктор.
На трёх настоящих моделях проверены 208 кадров за два lifecycle-цикла,
ASan/UBSan и ARM64/QEMU PASS; devkitA64 ELF скомпонован без undefined strong
symbols, на Switch не запускался. Найден и исправлен в CPU overlay null shared
buffer при повторной загрузке. Полный CGame Init/Run блокируется cache/material
и L3G device lifecycle. Предыдущий CList и его ограничение Insert не менялись.

- [Сводка MatrixGame и открытые барьеры](../../reports/matrixgame-e2e515-status.md)
- [E2E-5.17: оригинальный CVectorObject CPU runtime и следующий blocker](../../reports/matrixgame-e2e517-engine-slice.md)
- [Вечерний тест основной игры и отдельного GPU smoke](../../docs/e2e517-evening-switch-check.md)
- [BigVB/BigIB: CPU staging и отдельный deko adapter](../../reports/matrixgame-e2e515-buffers.md)
- [CList: выполненные проверки и ограничения](../../reports/matrixgame-e2e5161-list.md)
- [CPU-библиотека и команды сборки](native-core/README.md)
- [Отдельный deko3d smoke test](deko/README.md)

MatrixGame не входит в основную NRO E2E-5.16. Полный CGame Init/Run,
графический backend и возврат результата боя в кампанию остаются открытыми.
Компиляция ARM64, QEMU и GPU smoke test не подтверждают планетарный бой.

## Отдельная Windows x86 проверка

Switch backend и возврат результата боя в кампанию ещё не реализованы.
Pinned submodule не редактируется. Адресная адаптация головы находится в
`patches/ai-head-module-type.patch`; первоисточник — murgesku/MatrixGame,
commit 06e4d327a050490eb7f52213c5fcfa7479464394.

`tests/probe_matrixgame_ai_templates_x86.cpp` вызывает настоящий CGame::Init,
создаёт D3D device/карту, читает реальную конфигурацию и проверяет загруженные
AI-шаблоны, тип/цену головы и общую стоимость ресурсов. Затем освобождает движок.
Проба требует рабочий Windows desktop и D3D9; headless CI, ARM64/QEMU и layout-only
ABI checks не заменяют её. Она не выполняет L3GRun, не имитирует победу и не
проверяет возврат результата в Space Rangers.

## Подготовка исходников

PowerShell из корня изолированного checkout. Нужны MSVC 2022 x86, Windows SDK,
CMake >=3.24 и DirectX June 2010 SDK Include/Lib/x86. SDK установщик запускать
не требуется: допустим отдельный локально извлечённый SDK.

```powershell
$gitExe = 'C:\Program Files\Git\cmd\git.exe'
$matrixPin = '218115a72fa9c5d43269f4ca84a8596a0bcb82f1'
if ((& $gitExe -C upstream/matrixgame rev-parse HEAD) -ne $matrixPin) { throw 'Wrong MatrixGame pin' }
if (& $gitExe -C upstream/matrixgame status --porcelain) { throw 'Pinned MatrixGame is dirty' }
$matrixSource = Join-Path (Get-Location) 'build/matrixgame/head-fixed-source'
$matrixArchive = Join-Path (Get-Location) 'build/matrixgame/head-fixed-source.zip'
if ((Test-Path -LiteralPath $matrixSource) -or (Test-Path -LiteralPath $matrixArchive)) { throw 'Choose fresh staging paths' }
New-Item -ItemType Directory -Path (Split-Path $matrixSource) -Force | Out-Null
& $gitExe -C upstream/matrixgame archive --format=zip "--output=$matrixArchive" $matrixPin
if ($LASTEXITCODE) { throw 'Archive failed' }
Expand-Archive -LiteralPath $matrixArchive -DestinationPath $matrixSource
& $gitExe apply --check --ignore-space-change --directory=build/matrixgame/head-fixed-source port/matrixgame/patches/ai-head-module-type.patch
if ($LASTEXITCODE) { throw 'Patch check failed' }
& $gitExe apply --ignore-space-change --directory=build/matrixgame/head-fixed-source port/matrixgame/patches/ai-head-module-type.patch
if ($LASTEXITCODE) { throw 'Patch failed' }
```

## Сборка и запуск

Используйте короткий build path: старые MSBuild FileTracker операции не поддерживают
слишком длинные пути вложенных ExternalProject/TryCompile (FTK1011).

Укажите свой SDK Include. Поддерживаемый wrapper — `native-probe/CMakeLists.txt`.
Зависимости zlib/libpng использует pinned MatrixGame CMake; при первом configure/build
он обращается к их upstream репозиториям. Частные игровые ресурсы туда не передаются.

```powershell
$dxInclude = Join-Path (Get-Location) 'build/matrixgame/directx-sdk/DXSDK/Include'
cmake -S port/matrixgame/native-probe -B build/mg-native -G 'Visual Studio 17 2022' -A Win32 "-DMATRIXGAME_SOURCE_DIR=$matrixSource" "-DDIRECTX9_INCLUDE_DIRS=$dxInclude"
if ($LASTEXITCODE) { throw 'Configure failed' }
cmake --build build/mg-native --config Release --target matrixgame-ai-template-probe --parallel 3
if ($LASTEXITCODE) { throw 'Build failed' }
$matrixProbe = Join-Path (Get-Location) 'build/mg-native/Release/matrixgame-ai-template-probe.exe'
$matrixRuntime = Join-Path (Get-Location) 'build/matrixgame/native-runtime'
$originalGame = 'D:\repos\space_rangers_hd_a_war_apart_port\windows\Space Rangers HD A War Apart'
if (Test-Path -LiteralPath $matrixRuntime) { throw 'Use a fresh runtime directory' }
New-Item -ItemType Directory -Path "$matrixRuntime/DATA" -Force | Out-Null
Copy-Item -LiteralPath "$matrixSource/MatrixGame/CFG" -Destination "$matrixRuntime/CFG" -Recurse
Copy-Item -LiteralPath "$originalGame/DATA/robots.pkg" -Destination "$matrixRuntime/DATA/robots.pkg"
Push-Location $matrixRuntime
try {
    # Python waits for the legacy WINDOWS-subsystem executable and captures its exit code.
    py -3 -c "import subprocess,sys; p=subprocess.run([sys.argv[1]],capture_output=True); sys.stdout.buffer.write(p.stdout); sys.stderr.buffer.write(p.stderr); sys.exit(p.returncode)" $matrixProbe
    if ($LASTEXITCODE) { throw 'Native AI template verification failed' }
} finally { Pop-Location }
```

Ожидается ненулевое число templates/with_head и нули invalid_types, invalid_prices,
invalid_totals. Exit 0 означает только указанную область проверки. Проверка без
head patch обязана завершиться ненулевым кодом. Исторические результаты/хеши,
GUI-сценарий настоящего боя и ограничения: `reports/matrixgame-20261018.md`.

CFG, robots.pkg, SDK, DLL/EXE и screenshots остаются в ignored build. Они не входят
в публичные артефакты. Исходные GPL headers/NOTICE сохраняются.

## MG-2: нативный callback adapter

`native/rangers_bridge.hpp` не включает Win32/D3D9/MatrixGameDll.hpp. Реализация
вызывает все 16 родных `Robot::TRobotCallbacks` через их реальные типы. В sound/image ID
хранятся отдельные порядковые номера, а указатели остаются у владельца. ID не повторяются
между экземплярами или после Reset; неизвестный/освобождённый ID отвергается.
UTF-16 views копируются в изменяемые NUL-terminated временные строки; embedded NUL
отвергается. Pixels заимствованы до FreeText/Reset/destructor. Game services и код
callbacks должны пережить bridge. Release callbacks должны завершаться без исключений,
как подразумевает исходный lifecycle. Этот адаптер не диспетчеризует callbacks на UI thread:
будущий backend обязан соблюдать threading contract основной игры.

Настройки переносятся по полям, включая borrowed legacy graphics pointers без усечения;
это не Direct3D backend Switch. Шесть полей статистики сохраняют signed time. Адаптер
не создаёт результат win/loss, не загружает DLL и пока не подключён к engine Run/кампании.
MG-2 остаётся PARTIAL; MG-3…MG-5 открыты. NRO стабилизации 3772031 этот код не содержит.

Локальная воспроизводимая проверка без закрытых ресурсов (Linux):

```sh
cmake -S port/matrixgame/native -B build/mg-bridge-asan -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBRIDGE_SANITIZER=address
cmake --build build/mg-bridge-asan --parallel 2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ctest --test-dir build/mg-bridge-asan --output-on-failure
cmake -S port/matrixgame/native -B build/mg-bridge-tsan -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBRIDGE_SANITIZER=thread
cmake --build build/mg-bridge-tsan --parallel 2
TSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/mg-bridge-tsan --output-on-failure
```

Тест использует настоящие объявления game callbacks с контролируемыми producers.
Он проверяет ownership/forwarding, 800 конкурентных sound cycles, stale/foreign IDs,
Unicode, частичный RenderText failure и поля settings/statistics. Это не реальный бой,
не работа main-game audio/text renderer и не проверка GPU/физической Switch.

## MG-3: GPU geometry prototype

Первый deko3d pass описан в [deko/README.md](deko/README.md).
Offline shaders и ARM64 ELF link/audit проверены; GPU main не выполнялся,
NRO не выпускалась. Это не полный renderer или планетарный бой.

## Нативный engine API и полная компиляция

`native/engine_callbacks.hpp` соединяет настоящую таблицу `SMGDRangersInterface`
с `RangersBridge`. Все 16 slots сохраняют исходные типы; sound/image IDs остаются
32-битными порядковыми номерами, pixels и borrowed handles сохраняют указатели.
Контекст удерживается до возврата каждого вызова. Подключать таблицу следует при
остановленном engine; game services должны пережить выполняющиеся callbacks.
UI thread dispatch здесь не выполняется. ARM64 contract:
`bash tools/matrixgame_engine_api_arm64.sh`.

`native-engine/CMakeLists.txt` подготавливает копию полного pinned source tree.
Для проверки оставшихся зависимостей явно собирайте target `matrixgame-native-engine`.
По состоянию на 18.10.2026 этот target **не компилируется**: `VectorObject.hpp`
всё ещё требует `d3d9.h`. Успех default CPU targets не подтверждает сборку движка.
Подробности, SDK ELF provenance и границы проверок:
[отчёт engine API](../../reports/matrixgame-native-engine-api-20261018.md).
