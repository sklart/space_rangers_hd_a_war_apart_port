# E2E-5.19: оригинальный Text и владение шрифтами

Полностью подключены исходные `Text/Font.cpp`, `Parser.cpp`, `Render.cpp`.
SDK-операции вынесены в `text_backend`: на Windows используются настоящие
D3D9/D3DX/GDI и оригинальный встроенный Rangers TTF; на остальных платформах
неподдерживаемая операция явно отклоняется. Метрики и изображения не имитируются.

## Доказанный дефект и исправление

Оригинальный статический GetFont захватывал первое устройство в lambda и
продолжал возвращать его шрифты при запросе с другим устройством. Baseline
на двух настоящих HAL devices возвращает `device_cache_match=0`, exit=1.
Кандидат возвращает шрифт запрошенного устройства, `device_cache_match=1`,
exit=0; после очистки кэша Release второго устройства возвращает ноль.

Кэш теперь принадлежит конкретному устройству, очищается перед сменой и при
DetachDevice действующего владельца. Пять исходных наборов шрифтов сохранены.
Регистрация AddFontMemResourceEx имеет парное освобождение; повторная очистка
допустима. Неудачное создание не публикует частично заполненный кэш.

В Render исправлен срок жизни readback: EndScene → LockRect → оригинальное
копирование в CBitmap → RAII UnlockRect. Раньше копирование использовало pBits
после UnlockRect. Это нарушение срока доступа, а не доказанная причина
аппаратного дефекта фонов основной игры. Контракт доступа описан в
[Microsoft Direct3D surfaces](https://learn.microsoft.com/en-us/windows/win32/direct3d9/direct3d-surfaces).

## Реальные результаты и границы

- Windows HAL: оригинальные font metrics, parser/layout и Text::Render
  исполняются. Результат 128×32 содержит 240 ненулевых RGB пикселей.
  Все RGBA bytes baseline/candidate совпадают, SHA-256
  `245bfa0e687846bf9ba6e7f2e5ae5e7acf468d686d624dcd755b422198ed86cf`.
- Host ASan/UBSan/LSan и ARM64/QEMU: compile/link/runtime PASS, повторный вход
  и cleanup; существующие native objects переиспользованы. Полные три Text TU
  собраны с ASan/UBSan; новых исключений нет. CPU-проверка доказывает отказ
  без устройства до изменения существующего bitmap, а не отрисовку текста.
- devkitA64: compile/link PASS; ELF без strong undefined. Switch runtime и
  deko3d text rendering — HARDWARE PENDING.
- Forced constructor/destructor link CMatrixMapLogic теперь PASS: последняя
  отсутствовавшая связь Text::Render закрыта. Реальное конструирование карты
  начато и корректно разматывается на оригинальном debug font Create без
  устройства. Полная карта ещё НЕ СОЗДАНА.
- Forced full CGame::Init link показывает шесть оставшихся связей:
  CMatrixMap::CreatePoolDefaultResources, InitObjectsLights, PrepareMap,
  Restart; CTransition::BuildTexture, RenderToPrimaryScreen.
  CGame::Init НЕ ВЫЗВАН; RunGameLoop и бой НЕ ЗАПУЩЕНЫ.

Parser и его header идентичны адаптированному baseline. Render целиком
идентичен после обратного применения двух описанных lifetime edits;
Font сохраняет исходные параметры и создание пяти наборов. Хэши исходников,
команды проверок и точные unresolved signatures сохранены в
[evidence](matrixgame-e2e519-text-evidence.json). Linux Text TU используют
исходные audit compile recipes без function sections; алгоритмы те же,
ASan/UBSan включены. SDK использует штатные function/data sections.

Это самостоятельный результат MatrixGame. Повторяющиеся фоны основной игры,
исторические manual-publish ENOENT и аппаратные зависания остаются OPEN.
Основная NRO, frozen комплекты, пользовательские saves/assets не изменяются.
Новой NRO и push нет.
