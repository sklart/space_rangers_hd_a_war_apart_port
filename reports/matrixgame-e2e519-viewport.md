# E2E-5.19 — исправление ABI размеров viewport в CalcPick

Продолжение `f05108d`, 23.10.2026, локальный master, без push.

## Подтверждённая причина

Подготовка Math3D.cpp заменяла оригинальный include 3g.hpp на отдельное
`extern int g_ScreenX, g_ScreenY`. В общем bootstrap эти же имена определены
как `int&` aliases к platform3g::Current().configured_width/height.
На MSVC это два разных decorated symbols и два LNK2019. На ELF имя символа
совпадает, но это не делает value/reference storage совместимым: core читает
storage ссылки как целое число вместо обращения к исходному размеру.

**До исправления воспроизведён runtime failure** на прежнем core/cache archive:
настоящие bootstrap globals показывают1280x720, исходный CalcPick вызывается
для центра экрана с обратимой матрицей вида и ненулевой проекцией, но выбрасывает
Base::CException. Процесс завершился SIGABRT/exit-6. Это отдельный адресный
baseline; его нельзя представлять как успешный тест или связывать с основной
игрой/аппаратным зависанием карты.

## Исправление

SDK-free pick_viewport.hpp объявляет единственный ReadPickViewport accessor.
Настоящий platform3g.cpp возвращает текущие configured_width/height владельца.
Math3D::CalcPick читает их один раз при вызове и использует те же формулы
проекции, инверсии view, нормализации направления и получения позиции.
Нет копии глобальных размеров, отдельного реестра, фиктивного viewport или
необходимости GPU ReadyDevice для CPU-геометрии.

Существующий standalone native math consumer предоставляет этот же accessor
из своих контрольных int globals. Original x86/D3DX reference-ветка теста
не изменена и новый адаптер не включает. Raw int aliases игрового bootstrap
и их исходный жизненный цикл сохраняются.

## Реальные проверки

- Общий host ASan/UBSan/leak-check bootstrap exit0: после настоящего
  InitializeMapCamera выполнен оригинальный CalcPick. Центр/верхний левый угол
  на1280x720 и1920x1080 сверены с независимой аналитической формулой;
  позиция луча сверена с обратным переносом view. Нулевая ширина вызывает
  исходную ошибку и сохраняет оба выходных вектора. Проверки повторены
  в двух сессиях с восстановлением настоящих глобальных размеров.
- ARM64/QEMU: тот же общий bootstrap и те же проверки exit0.
- Standalone native math host ASan/UBSan: существующий asset-free trace
  совпал с сохранённым оригинальным x86/D3DX oracle по51105 значениям,
  максимальная scaled error `2.8610000000206526e-06` при допуске0.00002.
  Новый оригинальный x86 executable не собирался; коммерческие ресурсы
  этим отдельным regression не проверялись.
- devkitA64: отдельный ELF COMPILE/LINK PASS, strong undefined=0;
  retained CalcPick и ReadPickViewport. SHA-256
  `80e5c9c84cd9386af084e7f2266324f75e4356faac8185eed2af16533d8d0fcf`.
  Исполнение на Switch HARDWARE PENDING, NRO не создавалась.
- Полный Windows bootstrap: компиляция PASS, обе прежние ошибки int/reference
  g_ScreenX/Y исчезли. **LINK всё ещё FAIL150** прочих symbols вместо152.
  CalcPick в Windows bootstrap не исполнялся; это не Windows runtime PASS.

В host sanitizer gate сохраняются прежние исключения: Console TU без ASan
global redzones, Logic/Map без UBSan vptr. Существующие standalone CList,
BigVB/BigIB и RoadNetwork suites не повторялись.

## Состав и оставшиеся границы

Сверены содержимое всех242 engine-manifest файлов в двух prepared views
и отдельно все66 core outputs в трёх views. Engine manifest не покрывает
все portable-core CPP, поэтому отдельная проверка core обязательна.
Math3D.cpp в core, portable-core, engine clone и Windows core совпадает
побайтово. От прошлого engine manifest изменился только MatrixLib/3G/Math3D.cpp;
соответствующий core output проверен отдельно, не пропущен из-за области manifest.
Исходники и команды сборок зафиксированы в
[evidence](matrixgame-e2e519-viewport-evidence.json), build_dirty=true до коммита.

Этот дефект MatrixGame устранён офлайн. Настоящий CMatrixMapLogic ctor/dtor
всё ещё требует завершения исходного world cleanup; ранее выявлены19 уникальных
отсутствующих symbols объектов, сторон, воды, миникарты, terrain/effects/skins
и UI hints. Их реализации этим изменением не подменены. CGame::Init,
RunGameLoop, CFormMatrixGame, создание мира и бой NOT EXECUTED.

Повторно совпали13 доступных protected inputs,107 аппаратных файлов,
frozen main NRO/ELF и оба аппаратных ZIP. G:/pack.zip недоступен. Сейвы,
игровые ресурсы и основная NRO не менялись. Причины аппаратных повторных
фонов, двух исторических manual-publish ENOENT, полного freeze карты и консоли
при выходе остаются OPEN; исправление MatrixGame их не объясняет.
