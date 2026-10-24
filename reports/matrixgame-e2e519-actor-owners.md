# E2E-5.19 — полные оригинальные Robot/Flyer owners

24.10.2026. База `582f36393e16f728cc6eda48cd2a9130a46891a4`.

В native-cache подключены целиком `MatrixGame/src/MatrixObjectRobot.cpp`
и `MatrixGame/src/MatrixFlyer.cpp`. Использованы исходные конструкторы,
деструкторы, unit ownership, переноска, движение, анимации, оружейные связи,
выбор объектов, shadows, Draw/Takt и освобождения ресурсов.
Отдельные фиктивные key functions/RTTI definitions не создавались.

Полное содержимое обоих translation units сверено после только перечисленных
ниже адаптеров. Это охватывает 74 именованных метода, вспомогательный код и
все условные preprocessor branches, без удаления ветвей игрового алгоритма.
Headers/layouts и 66 portable-core outputs не менялись.

## Что изменено

- SDK texture/state/draw/transform операции используют существующие typed
  adapters; LodBias передаётся теми же float bits через bit_cast.
- Исходные перекрывающиеся MoveMemory заменены на memmove, stack allocations —
  на compiler intrinsic. UnitInsert/WeaponInsert/UnitDelete алгоритмы сохранены.
- RotationX/Y, Scaling и TransformNormal используют ранее принятый math adapter.
- Недостающие Vec2 dot/projective transform и plane transform добавлены через
  DirectXMath. Матрица plane transform передаётся без скрытого inverse/normalize.
  Все входы считываются до записи output, включая out == in.
- Statics Flyer остаются у исходного общего GameStaticResources owner;
  повторных definitions и подмен cache/device/map нет.

## Реальные новые доказательства

В production object files теперь определены настоящие compiler-generated:

```
typeinfo for CMatrixRobot
vtable for CMatrixRobot
typeinfo for CMatrixFlyer
vtable for CMatrixFlyer
```

Именно два отсутствовавших RTTI предыдущего host link устранены подключением
настоящих owners. Это не означает, что Actor constructor/destructor выполнялись.

Три новых math adapter сопоставлены с настоящими D3DX SDK функциями на Windows:
10 000 наборов, включая projective matrices и in-place aliases, PASS.
Максимальная ошибка компонента 8.58306885e-6; допуск 5e-5 + 1e-5 * abs(expected).
Тестируются конечные входы с abs(projective w) >= 0.5. Нулевой w/NaN/infinity
этим не доказаны. Этот SDK math oracle не является GPU/gameplay proof.

devkitA64: оба полных CPP compile PASS, принятый общий bootstrap link PASS,
strong UND = 0. ELF SHA-256:
`8679582e50319c7e65f6a9064e0186c97a4010b084a15864af0f6c9a1adff67a`.
Это bootstrap ELF с dead-code collection, не полный engine/NRO.
Linux ARM64/QEMU: regression общего bootstrap с новой библиотекой PASS,
две сессии. Actors не создавались.
MSVC: оба полных production TU compile PASS; Windows runtime здесь подтверждён
только для math oracle. CMake configure/generate PASS, полной повторной сборки нет.

## Новая точная граница

Host GCC ASan/UBSan: оба CPP compile PASS, но **LINK BLOCKED**, runtime NOT RUN.
Устранены прежние CMatrixRobot/CMatrixFlyer RTTI; новые metadata references:

- CMatrixRobotAI — `MatrixGame/src/MatrixRobot.cpp`;
- CMatrixBuilding — `MatrixGame/src/MatrixObjectBuilding.cpp`;
- CMatrixEffectWeapon — `MatrixGame/src/Effects/MatrixEffectWeapon.cpp`;
- CMatrixEffectSelection — `MatrixGame/src/Effects/MatrixEffectSelection.cpp`.

Новых sanitizer exclusions нет. Следующий перенос должен включать настоящие
key virtual owners этих классов, не искусственные RTTI записи.

Production map owner force-link, Linux ARM64 без санитайзеров:
закрыт CMatrixFlyer destructor, открылись **9 настоящих транзитивных зависимостей**.
Итого **22 unresolved вместо 14**, поскольку полный lifecycle теперь тянет
group removal, selection, CInfo, interface reposition, building floor и weapon
modify/sound. Рост счётчика не скрывается и не считается регрессией алгоритма
или процентом готовности. Точные новые/оставшиеся symbols находятся в evidence.

Не выполнялись Actor constructors, UnitInsert/WeaponInsert, Takt/Draw,
переноска/столкновения, настоящий destructor cleanup или повторный вход Actor.
Им нужна настоящая карта; фиктивный g_MatrixMap не создавался.
Полный CGame::Init, RunGameLoop, CFormMatrixGame и бой не запускались.
Switch/deko3d — HARDWARE PENDING. Нового NRO нет.

## Сохранность

Effective engine — 248 файлов, изменены только два Actor CPP.
Основная NRO/ELF, оба аппаратных ZIP, 13 доступных protected inputs и 107
аппаратных файлов сверены по исходным SHA. G:/pack.zip недоступен.
Пользовательские сейвы, игровые ресурсы и настройки консоли не менялись.
Push не выполнялся. CPU raster owner, исторические ENOENT и причины freezes
основной игры остаются OPEN.

[Команды, hashes, полный source audit и точные blocker lists](matrixgame-e2e519-actor-owners-evidence.json).
