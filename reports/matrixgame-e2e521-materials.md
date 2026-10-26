# E2E-5.21: оригинальные texture stages и deko shader candidate

Исторический срез. [Продолжение: закрытие alpha/null, ReadCaps и FVF/TCI](matrixgame-e2e521-cascade.md)
заменяет приведённые ниже сведения об оставшихся 209 случаях и исходном отказе caps.

25.10.2026. **PARTIAL. Блокер DekoDevice::ReadCaps НЕ ЗАКРЫТ.**
Нативный Init до первой загрузки карты на Switch этим продолжением не продвинут.
Полный material backend не заменён успешным caps-ответом или ordinal shader.

## Реализовано

- Отдельный восьмистадийный texture combiner: независимые RGB/alpha operations,
  CURRENT/TEMP, texture factor, complement/alpha replicate, насыщение результатов
  после каждого stage. Операции SELECTARG1/2, MODULATE/2X/4X, ADD/ADDSIGNED/2X,
  SUBTRACT, ADDSMOOTH и BLENDDIFFUSE/TEXTURE/FACTOR/CURRENTALPHA, premultiplied
  texture-alpha blend имеют явные shader equations.
- GLSL fragment program с восемью sampler bindings и std140 uniform layout;
  ветка, не использующая texture argument, не читает несуществующий descriptor.
  Color DISABLE завершает cascade. Последний результат идёт в framebuffer и
  при конечном RESULTARG=TEMP; предыдущие записи TEMP сохраняют CURRENT.
- DekoCombinerProgram принимает настоящий device и DKSH, владеет shader code,
  создаёт отдельный immutable uniform packet для draw и записывает реальные
  bindShaders/bindUniformBuffer/bindTextures. Queue, descriptors и texture owners
  остаются у вызывающего renderer и должны жить до completion. Это fragment
  cascade, без подмены vertex lighting, raster state или устройства.
- Валидация отказывает при неподдержанном operation/register/argument,
  активном RGB с отключённой alpha и использовании пустой texture binding.
  Отказ происходит до GPU allocation/publication. Существующий DekoDevice
  по-прежнему явно отказывает в ReadCaps; сообщение уточняет незакрытые пути.
- Отдельные CMake targets `matrixgame-material-combiner`,
  `matrixgame-material-shaders`, `matrixgame-material-contract` подключены к
  native-engine; основной bootstrap и полная игровая цель не объединялись.

Оригинальные Init/Draw, skin Prepare/pass selection и renderer callbacks не
переписаны. Проверочный генератор извлекает исходные функции, адаптируя только
типы и вызовы API для trace device. Resource/geometry inputs в этом тесте —
явные контролируемые fixture; они не выдаются за загрузку карты или GPU backend.

## Что действительно исполнилось

| Проверка | Результат и граница |
| --- | --- |
| Windows D3D9 HAL | 160 scalar-combiner vs actual GPU draw/readback сравнений, несовпадений 0; RTX 5060, nvldumd.dll |
| Оригинальные callback в этих сравнениях | 115 проходов skin/terrain/water; ещё 45 — operation/modifier cases |
| Остальные оригинальные material cases | 209 явных native refusals: 163 disabled-alpha, 46 null texture; их D3D9 pixels прочитаны, native parity не доказана |
| Host ASan/UBSan/leaks | 369 material cases, TEMP/конечный TEMP, границы/неподдержанный op/null binding — PASS для проверяемого subset и правильного отказа |
| ARM64/QEMU | Тот же адресный CPU-набор — PASS; не GPU runtime |
| UAM | material.frag.glsl скомпилирован в настоящий DKSH — PASS |
| devkitA64 | Combiner/program, оригинальные callback fixtures и текущий platform_switch.cpp compile/link с deko3d/libnx — PASS; strong undefined=0 |
| CMake | Windows contract target и SDK material/shader target configure/build — PASS |
| Полный Windows Init/frame | В этом продолжении не повторялся: алгоритм и Windows provider не менялись |
| Switch | NOT RUN; DKSH execution, allocation, queue binding, кадр и map load не подтверждены |

В Windows reference используются prelit vertices и постоянные 1×1 textures.
Blend и alpha test выключены, texture transforms/lighting не сравниваются.
Поэтому 160 совпадений доказывают scalar equations и routing в этой области,
но не визуальную эквивалентность полного материала и не исполнение deko shader.

Дифференциальная проверка сначала выявила ошибку кандидата для конечного TEMP
destination; её исправили в scalar evaluator и GLSL. Итоговый набор проходит.
Проверены четыре ветви исходного skin Prepare по texture caps (2/3/4/6), три
группы, комбинации gloss/mask/back, alpha-test flag и продолжение pass. Burned
object branch, анимация UV и полный shadow rendering этим fixture не доказаны.

Прежние Path/full-engine большие наборы не повторялись. Ошибки запуска WSL в
песочнице преодолены адресным разрешённым запуском; Windows CMake использует
явный Python313 вместо неработоспособного MSYS2 Python из PATH.

## Почему ReadCaps остаётся закрытым

Оригинальные `obj_side2`, `obj_side6`, `TerSurf`, `TerBotM` используют состояния,
которые текущий native validator ещё не может сохранить. Например, `obj_side6`
отключает alpha на stage 3 при активной RGB operation, `TerSurf` — на stage 1,
`TerBotM` — уже на stage 0. Отдельные back-texture ветви включают stage при
нулевом binding. Подставлять белую текстуру или произвольную alpha нельзя.

Для active RGB + disabled alpha D3D9 не определяет результат переносимым
контрактом: [Microsoft Texture Blending Operations and Arguments](https://learn.microsoft.com/en-us/windows/win32/direct3d9/texture-blending-operations-and-arguments).
Windows readbacks подтверждают, что простое правило «сохранить CURRENT alpha»
не покрывает все эти состояния. Варианты нормализации не прошли полную сверку и
не включены в рабочий shader. Это незавершённая совместимость исходного
поведения, без утверждения о дефекте исходной игры.

Даже после этой сверки нужны оригинальные FVF lighting и TCI/texture matrices,
полный raster/stencil/shadows contract и подключённый original draw submission.
Новый fragment module пока не подключён к DekoDevice как полный renderer.
Успех компиляции этого модуля не разрешает рекламировать MaxTextureStages или
MaxSimultaneousTextures для полного CGame.

Фактическая известная программная граница полной native-цепочки остаётся:

`CGame::Init → original_game::EnterPlatform → NativeInitRuntime::EnterNativePlatform
→ platform3g::InitAsNative → DekoDevice::ReadCaps`.

Этот отказ по-прежнему установлен по исходникам; на Switch он не наблюдался.
Реальные device/queue/image/swapchain allocation могут остановиться раньше.
После material contract понадобятся texture residency/allocation и исходный
texture backend, state capture/lifecycle/video, gamma/color/font/cursor/input,
а затем реальный native Init с map load и acquire/submit/present/readback.

## Воспроизведение и сохранность

```text
python tools/matrixgame_material_checks.py host
python tools/matrixgame_material_checks.py arm64
python tools/matrixgame_material_checks.py windows
python tools/matrixgame_material_checks.py sdk
python tools/matrixgame_material_checks.py cmake-windows
python tools/matrixgame_material_checks.py cmake-sdk
python tools/matrixgame_material_audit.py
```

Каждый runner записывает команды, exit codes, extraction/source hashes и логи
в отдельный `build/matrixgame/e2e521/materials/<kind>`. Итоговый
[аудит](matrixgame-e2e521-materials-evidence.json) проверяет актуальность исходников,
все результаты и хэши защищённых входов. Сверены 15 доступных protected files,
107 hardware input files, оригинальные robots.pkg/CFG и frozen GI cache source.
Основная NRO, frozen-комплекты, ресурсы и сейвы не менялись. Новая NRO не
создавалась; коммитов и push нет.

P0 основной игры, причинность повторяющегося GI-текста, manual-publish ENOENT и
Switch hangs остаются открытыми. Новых аппаратных доказательств нет.
