# E2E-5.63 — native wireframe для исходного selection pass

26.10.2026. **SOFTWARE INTEGRATION PASS; SWITCH / FULL OPTIONAL MAP PATH NOT RUN.**

Реализован настоящий polygon wireframe mode для оригинального прохода
выделения объектов. Предыдущий отказ `native draw requires FILLMODE`
воспроизведён до изменения production code. Значение FILLMODE=2 ранее
сохранялось, но ValidateDraw разрешал только SOLID=3.
[Evidence](matrixgame-e2e563-wireframe-evidence.json).

## Исходный вызов

`CMatrixMap::Draw` при `GFLAG_STENCILAVAILABLE && g_Config.m_IzvratMS`
вызывает `CMultiSelection::DrawAllPass1Begin`; когда есть mask, затем идут
`DrawAllPass2Begin → DrawObjects → DrawAllPassEnd`.
Оригинальный `CMultiSelection::DrawPass2` выставляет stencil EQUAL/ZERO и
FILLMODE=WIREFRAME. `DrawPassEnd` возвращает SOLID и выключает stencil.

`m_IzvratMS` по исходным defaults выключен. Настройка не изменялась ради
проверки; это реализация дополнительной исходной игровой функции, а не
объявление её следующим обязательным first-Init blocker. Ни один из этих
оригинальных алгоритмов или игровых ресурсов не заменён и не отключён.

## Изменения

* NativeRenderState разрешает WIREFRAME и SOLID; POINT fill остаётся
  неподдержанным. Wireframe использует те же ограничения aliased line и
  LASTPIXEL=TRUE, что и существующие native линии.
* Новый `ApplyLegacyPolygonMode` вызывает настоящий SDK
  `dk::RasterizerState::setPolygonMode` с DkPolygonMode_Line или Fill.
  Оба faces задаются при каждом draw: последующий solid pass не наследует
  wireframe. Неверное значение отвергается до изменения SDK state.
* DekoDevice::Record действительно привязывает этот RasterizerState и задаёт
  line width=1, stipple=false также для polygon wireframe. Геометрия, shaders,
  culling, depth/stencil, blend, viewport и retained uploads остаются исходными
  настоящими путями; triangle edges не заменяются пустой работой или CPU image.
* Host reference получил явный fill mode для HAL и GL. Default=SOLID сохраняет
  прежние проверки. Перед stencil-to-color resolve HAL helper возвращает
  SOLID, чтобы сам readback quad не наследовал wireframe.

## Проверки и пределы

| Gate | Наблюдение |
| --- | --- |
| Baseline HOST ASan/UBSan | PASS: Render(FILLMODE,WIREFRAME) → ValidateDraw воспроизвёл отказ до исправления. Executable и точные прежние source snapshots сохранены в baseline/. |
| HOST ASan/UBSan | PASS: wireframe/solid, capture/restore, stencil sequence, invalid value rollback, AA/LASTPIXEL/point/MSAA/scissor guards. Использованы реальные установленные SDK headers и inline RasterizerState setters; это исполнение CPU fields, не deko commands/GPU. |
| Linux ARM64/QEMU | PASS: та же native state проверка и inline SDK setters. Front/back polygon fields совпали с Line/Fill; cull/depth-clamp не изменены. |
| Оригинальный CDeviceState HOST | PASS: полный прежний scalar contract, 103 render registers, sampler/light/material, три restore/reset. Сохраняемая конфигурация теперь включает WIREFRAME, enable=TRUE и mask=0. |
| Windows D3D9 HAL vs production GLSL | PASS: 576 случаев SOLID/WIREFRAME, TRIANGLELIST/STRIP/FAN, culling, winding, fractional coordinates, viewport clipping, degenerate и полностью внешняя геометрия. Все 256 pixels каждого случая сравниваются; mismatches=0, max_delta=0. |
| FLAT/GOURAUD и RHW | PASS: ещё 96 wireframe случаев, variable RHW и отдельные vertex colors; coverage сравнивается строго, colors допускают до 2 согласно существующему host shader oracle. Фактически max_delta=0 и coverage_mismatches=0. |
| Selection stencil sequence | PASS: восемь controlled sequences EQUAL/ZERO/WIREFRAME → SOLID/stencil off. Цвет сравнивается строго после обоих проходов, остаточный stencil — побайтно через независимые HAL resolve и GL readback. Полные CMatrixMap/CMultiSelection bodies этим probe не исполнялись. |
| SDK compile/link/NRO | PASS: полный devkitA64/libnx/deko3d build и штатная отдельная NRO; strong undefined=0. |

Windows GPU: NVIDIA GeForce RTX 5060, GL 4.6 NVIDIA 617.42. Это настоящий HAL
и исполнение production vertex/fragment GLSL через существующий host harness
на том же адаптере. Это не запуск deko3d или сравнение изображения со Switch.
680 GPU cases не означают полное исполнение optional selection на настоящей
карте; геометрия этих адресных GPU проверок контролируемая.

Расширение stencil probe сначала дало восемь ошибок: HAL ReadStencil выполняет
destructive stencil-to-color resolve, а первоначальный probe затем продолжал
draw с preserve=true. Чтение перенесено после solid continuation, у которого
stencil выключен и результат предыдущего ZERO сохраняется. Все сравнения
сохранены, assertions не ослаблены. Неудачный runtime log сохранён как
`windows/runtime-stencil-resolve-order-failure.log`; первый отдельный успешный
576-case log — `windows/runtime-first-576.log`.

Большие полные Windows Init/PrepareMap/frame-loop и прежние line suites
повторно не запускались. Default SOLID отдельно проверен в новой матрице, и
native restore/guards закрыты адресными тестами.

## Отдельная сборка

`build/matrixgame/e2e563-wireframe/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro`

Размер: **6 965 445 байт**. SHA-256:
`d88b01d85c0c977fcbdfa68bdf9dbe15343f7b886dd9390ea1122583035d5364`.

NRO: три корректных сегмента, встроенные icon/NACP, обязательного внешнего
RomFS нет. Build использует стандартную цель
`matrixgame-native-diagnostic-nro` с nacptool/elf2nro. ELF SHA, команды,
source/library provenance сохранены в SDK result.json и evidence.

```powershell
C:/Users/Sklyarov/AppData/Local/Programs/Python/Python313/python.exe tools/matrixgame_wireframe_checks.py windows
C:/Users/Sklyarov/AppData/Local/Programs/Python/Python313/python.exe tools/matrixgame_wireframe_checks.py host
C:/Users/Sklyarov/AppData/Local/Programs/Python/Python313/python.exe tools/matrixgame_wireframe_checks.py arm64
C:/Users/Sklyarov/AppData/Local/Programs/Python/Python313/python.exe tools/matrixgame_wireframe_sdk.py
```

Это воспроизводимость текущего подготовленного workspace с сохранёнными
local prerequisites; точные compiler/include/library paths есть в results.
Baseline mode предназначался для исходного pre-fix запуска; его source
snapshots и бинарник сохранены отдельно и не перезаписывались candidate build.

## Init, сохранность и оставшаяся работа

Последний реальный Switch run остаётся в
`CGame::Init → ReadParams → ApplyGammaRamp`, до создания карты. E558 gamma
implementation, E559 full ReadParams CPU evidence, E561 bitmap integration и
E562 single-sample state fix сохранены. Этот этап расширяет настоящий renderer,
но не является новым native Init PASS.

Открыты реальная проверка Init/PrepareMap/first map frame и optional selection
на native GPU; image/gamma/VRAM, queue-loss recovery и дальнейшая игра.
POINT fill/topology, AA lines, LASTPIXEL=FALSE, scissor, multisample targets/
resolve и другие ранее перечисленные unsupported modes по-прежнему явно
отвергаются. Их необходимость для first Init не устанавливается одним наличием
guard. Дальнейшая исходная разработка возможна без устройства.

Все generated original source files совпали с E562. Upstream clean на прежнем
pin 218115a72fa9c5d43269f4ca84a8596a0bcb82f1. Основная NRO, E556/E558/E561/E562,
прежние dist NRO, frozen/resource/save fingerprints и пользовательский logs2.7z
сверены. Изменений upstream, ресурсов или saves, commit/push и взаимодействия
с устройством не было.
