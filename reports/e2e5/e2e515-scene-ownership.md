# E2E-5.15: владельцы снимков фона и реальные переходы

20.10.2026. **PARTIAL / HARDWARE PENDING**. Исправлен подтверждённый host-дефект
передачи подготовленной подложки. Это не подтверждение первопричины аппаратного
фона генерации и не закрытие всех экранов C1.

## Дефект и исправление

Исходный `CaptureScreenBackground(true, ...)` подготавливает Aux, включая
BackgroundShade/BackgroundGrayscale. `SceneCanvas` создавал новый Aux из raw
framebuffer родителя и терял подготовленное изображение. Кроме того, OnClose
карты очищает Aux, а переход к меню сначала вызывает вложенную миниатюру галактики.
Один глобальный одноразовый снимок расходуется этим preview раньше самого меню.

| Файл / функция | Изменение |
|---|---|
| tools/e2e_source_overrides.py / GR_Main::CaptureScreenBackground | После исходных эффектов передаёт результат переносимому владельцу; hook есть в host и Save ON effective sources |
| renderer_platform::PreserveCapturedBackground | Копирует подготовленный Aux в собственный snapshot активного scene scope; исходный Aux можно безопасно очистить |
| SceneCanvas constructor | Вложенное окно получает копию снимка своего родителя, если тот подготовлен; самостоятельная сцена не наследует предыдущий кадр |
| SceneCanvas destructor | Дочерний snapshot освобождается; snapshot родителя остаётся. При последовательном выходе готовый snapshot передаётся следующему overlay однократно |
| renderer shutdown | Освобождает оставшийся переносимый snapshot вместе со стабильными backdrop buffers |

Снимок намеренно фиксирует момент Capture. Последующие движения курсора и
отрисовка save preview в той же сцене не меняют его. Поэтому равенство последнему
raw-frame hash не является условием использования; проверяется владелец scope.
На переходе к самостоятельной сцене старый переносимый snapshot инвалидируется.
BGBuf по-прежнему заимствует стабильный отдельный Aux; собственный snapshot ему
не передаётся. Дополнительная память — 1843200 байт на снимок 1280×720 RGB565
при pitch 2560, только после явного Capture. Неиспользованный буфер переноса
переиспользуется; снимки дочерних scopes освобождаются при выходе. Это не замер RSS.

## Проверки

- До правки реальный Capture с затемнением → nested SceneCanvas дал assertion FAIL
  на равенстве подготовленного Aux (`scene-backdrop-baseline.json` в build).
- После: 10 циклов с Capture → OnClose/Clear → последовательный overlay,
  вложенными preview, новым Capture в ребёнке, последующей отрисовкой родителя,
  одноразовым последовательным переносом, независимой сценой и исключением PASS.
- Пять engine tests PASS: runtime-loop, scene-canvas, screen-context, native-focus,
  sanitized-scene. ASan/UBSan и leak detection включены для renderer_platform,
  GR_Main, GR_GraphBuf, GI_MessageLoop и теста. Остальные game objects не
  инструментированы: это **не full-game sanitizer PASS**.
- Реальный raw-pad маршрут: Unicode OSK → generation → четыре character choices
  → hangar → takeoff/map → пять циклов ship/map/menu/exit-confirmation/Cancel/map.
  Run `a8d8875e123a4395891731ef6a22a9d2`, host ELF SHA-256
  `d310e45ad19fd32ad5ca47fb9dfbc6ff34d342b4023bf9560b25606228a9fbdd`.
- Во всех пяти ship и пяти menu entries записано `prepared=1`. Вложенный exit
  dialog имеет depth=2 и незарегистрированного отдельного owner, после Cancel
  восстановлен зарегистрированный menu owner depth=1.
- Все 34 PPM сверены с hash RGB565 из реальной презентации. В двух боковых
  областях меню после каждого Cancel **0 изменённых пикселей**. Контактный лист
  просмотрен. Артефакты остаются только в ignored build, игровые изображения
  не добавляются в Git.
- Не найдены одноимённые фрагменты генерации 64×64 в проверяемой внутренней
  области (32:1248, 32:608): raw, shade −50%, grayscale и shade+grayscale.
  Четыре положительных контроля обнаруживаются. Это проверка точных фрагментов
  в тех же координатах; мелкие, сдвинутые и другие преобразованные остатки она
  не исключает. Сравнение не основано только на разных полных hashes.
- Preflight ordinary/combined/Save ON PASS; devkitA64 COMPILE PASS девяти units,
  включая current GR_Main/renderer. Хеши renderer/header сверены. LINK/NRO и GPU
  в этом пакете не проверялись. Win32 imports/classifications/implementations
  прежние; обновлены расположения callsites и число просканированных файлов.
- Восемь исходных файлов данных и исходные CFG не изменены; pins сохранены.

Ранние маршруты `52d675d1c0ac4c4a90784f1387cd832b` и
`4a917843c9a1431eb6ccd252b6cbb96f` завершились FAIL тестового драйвера:
удержание D-pad допускало repeat, а статический план фокуса устаревал при
анимации панелей. Теперь навигационный press короткий, план пересчитывается
после каждого действительного шага; неожиданный шаг при неизменной геометрии
по-прежнему не принимается. Ошибки не названы дефектами framebuffer.

Промежуточные runs `6bc9835a9bf64c90a81334e3fd15d2d5` и
`5930f2b551654ccdab193373ad882cd2` прошли тот же маршрут/анализ, но menu entries
ещё не использовали подготовленный snapshot. Финальный run проверяет исправленное
владение через вложенную миниатюру, а не только успешную навигацию.

## Воспроизведение и остаток

```sh
python3 tools/e2e515_engine_tests.py --sanitize-scene
python3 tools/e2e515_controller_route.py --campaign --scene-cycles 5 --label scene-owned --game-root /path/to/game --config-root /path/to/copied/config
python3 tools/e2e515_scene_frames.py build/e2e515/scene-owned/result.json
```

Последняя команда требует Pillow и NumPy; локально использован bundled Python
Codex. CI host suite включает sanitizer test; CI не запускался. Route runner
останавливает процесс после проверок, normal shutdown не подтверждён. ASan unit
имеет собственный нормальный teardown, однако это не teardown полной кампании.

OPEN: реальные переходы planet/shop/treatment/ruins/arcade и остальные ветвления
всех framebuffer/texture lifetimes; полная игра под sanitizer; финальный NRO+ELF;
проверка исходной жалобы на Switch. Фон генерации в выполненных host-маршрутах
не воспроизведён. Ни шаблонный детектор, ни исправление Shade не закрывают этот
аппаратный пункт. Метаданные без ресурсов: `e2e515-scene-validation.json`.
