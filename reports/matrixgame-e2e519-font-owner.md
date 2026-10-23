# E2E-5.19 — настоящий Windows font owner и общие renderer ops

Продолжение `4beea4b`, 23.10.2026, master, без push. Основная игра не менялась.

## Два подтверждённых интеграционных дефекта

1. Windows `DrawText` macro менял имя функции SDK backend после включения
   windows.h. SDK-free DebugInfo объявлял другое имя, и общая MSVC линковка
   завершалась LNK2019. Отдельный SDK-only тест этого не замечал: caller и
   callee получали одинаковую macro-подстановку. Адаптер переименован в
   **DrawTextWide**; оригинальный вызов ID3DXFont::DrawTextW сохранён.
   Новый отдельный SDK-free caller линкуется с настоящим Windows backend и
   выполняет измерение glyph text через реальную D3DX DLL.
2. Оригинальные глобальные SetColorOp/SetAlphaOp и select/disable/any-order
   helpers были объявлены для map/debug, но их определения находились только
   внутри namespace original_renderer_ops в renderer TU. Реальный Windows
   DebugInfo owner link остановился на двух select symbols. Все восемь
   оригинальных ordinary-ветвей извлечены в **единственный OriginalRendererOps.cpp**.
   Их тела и порядок device calls сохранены; renderer использует те же globals
   через namespace using aliases из renderer_ops.hpp. Новых алгоритмов или
   успешных заглушек нет. DO_SMART_COLOROPS по-прежнему явно не поддерживается.

## Реально выполнено

На настоящем Windows D3D9 NULLREF: точный исходный D3DXCreateFontW запрос,
проверка возвращённого descriptor и device identity, измерение текста из
SDK и SDK-free TUs, отказ повторного Create в непустой output, lost/reset
notifications, Release/recreate в двух циклах. Использована app-local DLL из
официального Microsoft.DXSDK.D3DX 9.29.952.8, без замены системных DLL.

Через **настоящую подготовленную InitAsDLL** и borrowed Window/Device выполнены
оригинальные CMatrixDebugInfo ctor, T, Takt, OnLostDevice, OnResetDevice и dtor
в двух циклах. Готовность устройства не выставлялась тестом вручную.
В этой же проверке оригинальные shared color/alpha select routines меняют
шесть настоящих texture-stage registers на ожидаемые значения и lease
восстанавливает прежние значения. Windows COM освобождения исполнены, но
отдельного Windows heap/COM leak audit не было.

В существующем Windows SDK helper выполнен настоящий DrawPrimitiveUP.
После него GetStreamSource показывает NULL; существующий state block
восстанавливает исходные VB, offset4 и stride12 в двух циклах. Baseline
уже проходил — дополнительного гипотетического исправления lease не делалось.
Такой сброс stream0 соответствует
[контракту Microsoft](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-drawprimitiveup).

Это **не** исполнение CMatrixDebugInfo::Draw, не доказательство нарисованных
пикселей, физического GPU или настоящего IDirect3DDevice9::Reset. Font callbacks
lost/reset не подменяют проверку сброса устройства. На Switch font backend
по-прежнему явно Unsupported; аппаратных подтверждений не добавилось.

## Общая интеграция и точные границы

- Общий host ASan/UBSan/leak-check и ARM64/QEMU bootstrap: exit0, четыре
  настоящих входа в базовый CMatrixMap ctor до ожидаемого отсутствующего font
  backend. Прежние ограничения Console ASan globals и Logic/Map vptr сохранены.
- Отдельный devkitA64 ELF COMPILE/LINK PASS, strong undefined=0:
  `688b8b3f039084a0bc80ec59da67ee64e5ba202f0a6c1c623ee2be80c961cee6`.
  Новая NRO не собиралась, runtime HARDWARE PENDING.
- Полный общий Windows bootstrap: все требуемые source units скомпилированы,
  **LINK FAIL**, runtime NOT RUN. Сначала159 symbols; после ABI fix158,
  после общих ops152. Ошибки не маскировались linker flags или заглушками.
  Выполнение original font owner доказано отдельным адресным Windows driver,
  использующим те же реальные compiled DebugInfo/InitAsDLL/core objects;
  это не полный game bootstrap PASS.
- В общий bootstrap добавлен тот же original font-owner helper для будущей
  negotiated-device сессии; этот путь полного Windows bootstrap ещё не исполнен.
- Два финальных prepared source view сверены по всем **242** файлам.
  От предыдущего принятого view изменились DebugInfo.cpp (имя adapter call),
  MatrixRenderPipeline.cpp (общие ops вместо private definitions) и добавился
  OriginalRendererOps.cpp. Upstream и игровые ресурсы не редактировались.

Отдельный настоящий CMatrixMapLogic ctor/dtor force-retain link выявил20 ссылок
на19 уникальных недостающих symbols. Это очистка реального мира:
StaticDelete/StaticClear → map objects/robots/cannon/building/flyer destructors;
ClearSide/WaterClear → side/water destructors; Clear → minimap/effects/groups/
terrain/bottom textures/skins; LeaveDialogMode → hint sound/destructor и UI
buttons. Точный список и исходный failed log сохранены в evidence. Ранние
runtime проверки базового ctor не доказывают этот полный owner lifecycle.

В полном MSVC link также доказано ABI-расхождение: Math3D::CalcPick ожидает
глобальные **int** g_ScreenX/Y, а bootstrap предоставляет **int&** aliases.
Это следующий конкретный технический блокер. Не следует механически менять
core ABI без учёта остальных его consumers или считать совпадение имён ELF
доказательством правильного доступа к этим значениям.

**CMatrixMapLogic ещё не создан и не освобождён; CGame::Init, RunGameLoop,
CFormMatrixGame и бой NOT EXECUTED.** Продолжение должно закрывать реальные
screen ABI и world ownership dependencies, а не создавать фиктивную карту.

Команды, SHA-256 исходников/objects/бинарников, успешные и неуспешные логи:
[evidence](matrixgame-e2e519-font-owner-evidence.json). build_dirty=true —
проверки до локального коммита. Повторно совпали13 доступных protected inputs,
107 аппаратных файлов, frozen main NRO/ELF и оба ZIP. G:/pack.zip недоступен.
Причины аппаратного intro raster, двух manual-publish ENOENT, freeze карты и
полного зависания консоли при выходе остаются OPEN; новых данных по ним нет.
