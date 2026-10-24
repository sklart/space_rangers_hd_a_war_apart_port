# E2E-5.19 — Cannon, CIFaceElement, ElevatorField и Zahvat

В production native-cache включены четыре полных оригинальных CPP:
`MatrixObjectCannon.cpp`, `Interface/CIFaceElement.cpp`,
`Effects/MatrixEffectElevatorField.cpp` и `Effects/MatrixEffectZahvat.cpp`.
Настоящие compiler-generated RTTI/vtable всех четырёх классов определены в
devkitA64 archive. Оригинальные virtual methods не вырезались.

## Выявленные нарушения жизненного цикла

Первая компиляция остановилась на существующем ZeroTrivialBytes guard:

1. CIFaceElement обнулял байтами `m_Actions`, содержащий `std::function`.
   Теперь пустое состояние задаётся корректным `m_function = nullptr`.
2. Cannon дважды обнулял весь массив SMatrixCannonUnit с нетривиальным
   SEffectHandler: в constructor и UnitClear. Сброс заменён на общий typed Reset.
   Он отсоединяет обратную ссылку эффекта оригинальным Unconnect, явно задаёт
   нули полям/матрицам и начинает lifetime normal s1 union branch placement new.

SEffectHandler::Release здесь не используется: он удаляет эффект через карту.
Прежний сброс оставлял эффект карте; Unconnect сохраняет это владение и убирает
обратную ссылку на сброшенный слот. Исходные graph/shadow cleanup перед Reset и
DIP TTL/Unconnect сохранены. Guard не ослаблен, void-cast обхода нет.
Это MatrixGame lifecycle исправление, не доказанная причина дефектов основной игры.

## Реально выполненная проверка

В общий bootstrap добавлен тест actual SMatrixCannonUnit array и того же Reset,
который вызывают original constructor/UnitClear. Реальный эффект создаётся
оригинальным CreateSelection, соединяется с unit handler, затем выполняется Reset:
handler и backlink обнулены, эффект остаётся жив, поля/матрицы сброшены.
Далее вызываются original Release эффекта и повторный Reset. Два повторения на
сессию, две сессии ARM64/QEMU — **PASS**, exit 0.

Это локально принадлежащий настоящий Selection, а не зарегистрированный в мире
smoke. Проверены существующий Unconnect и typed unit reset; настоящий Cannon
constructor, полный UnitClear, smoke TTL/map registration и DIP simulation не
исполнялись. g_MatrixMap остаётся nullptr, искусственная карта не создавалась.
Сам constructor CIFaceElement и runtime std::function actions не проверены;
корректность их инициализации опирается на стандартную операцию std::function.

## Сборки и следующая граница

- **devkitA64 compile/link PASS**, final ELF strong undefined = 0.
  Initial nontrivial clear failure сохранён. Затем исправлена отсутствующая
  MatrixMap include в новом тесте; завершающая сборка потребовала только три jobs.
  NRO не создавалась, Switch runtime **HARDWARE PENDING**.
- **Linux ARM64 compile/link и QEMU bootstrap PASS.** Четыре новых CPP и два
  изменённых/новых test CPP скомпилированы; неизменённые objects переиспользованы.
  Это адресная интеграционная проверка, не новая полная ARM64-сборка.
- **Host ASan/UBSan compile PASS, LINK BLOCKED**, runtime NOT RUN.
  Четыре прежних RTTI-блокера закрыты; остались `typeinfo for CIFaceButton` и
  `typeinfo for CMatrixEffectBillboard`. Новых sanitizer exclusions нет.
- **Production map-owner link probe:** 61 → 57 unresolved; закрыты шесть связей,
  открыты две Billboard PrepareDX/UpdateData. Полные списки и команды в JSON.

Тела полных TU сопоставлены с предыдущими effective sources после обращения
конкретных typed adapters и трёх перечисленных initialization replacements.
Кроме добавленного Reset, изменения алгоритмов не обнаружены. Cannon matrix
multiply использует прежний D3DXMATRIX operator с теми же операндами/порядком;
нового математического адаптера нет. Render/UI states, animation, minimap и
исходный порядок операций сохранены; их GPU исполнение этим этапом не доказано.

Следующие реальные владельцы для интеграции — CIFaceButton и полный Billboard;
затем остаются Side/UI, CInfo/Group и полный CVectorObjectGroup.
Полные CGame::Init, RunGameLoop, CFormMatrixGame, карта и бой ещё не запускались.

## Сохранность

13 доступных protected inputs, 107 аппаратных файлов, frozen main NRO и
matching ELF сверены по SHA-256: изменений нет. G:/pack.zip недоступен.
Оба hardware ZIP, сейвы, ресурсы и настройки консоли не менялись. Push не выполнялся.
Источник повторяющегося текста, исторические manual-publish ENOENT и причины
зависаний основной игры остаются OPEN.

[Команды, effective-source audit, runtime и бинарные SHA-256](matrixgame-e2e519-cannon-ui-evidence.json).
