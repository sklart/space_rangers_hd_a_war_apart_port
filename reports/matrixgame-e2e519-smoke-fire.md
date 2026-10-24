# E2E-5.19 — полный оригинальный SmokeAndFire в общей интеграции

24.10.2026. База `dc71b2fb1cf306509548fb81b0ba0acd1eb06903`.

В штатный native-engine target добавлен весь исходный
`MatrixGame/src/Effects/MatrixEffectSmokeAndFire.cpp`: Smoke, Fire,
FireStream и FireAnim. Вместо новых реализаций использованы оригинальные
конструкторы, Takt, SpawnPuff, Draw и Release. Все 21 out-of-line тела
сверены с предыдущим effective source после только существующих typed adapters.
Формулы движения, генератор случайных чисел, интервалы появления, TTL,
цвета, alpha, масштабы и обращения к настоящей карте не менялись.
Новых sanitizer exclusions нет.

## Что действительно выполнено

В общем оригинальном bootstrap при `g_MatrixMap == nullptr` выполнен
настоящий map-independent FireStream: HNew с оригинальным effect heap,
конструктор пяти CBillboardLine, SetPos, BeforeDraw, Takt и Release.
Проверены настоящий тип эффекта, связь с SEffectHandler и очистка обратной
ссылки оригинальным базовым деструктором, повторное создание/освобождение.
Texture registry и staging allocation возвращаются к исходным значениям.
Этот путь использует ещё не загруженные billboard texture slots; он не доказывает
загрузку игровых текстур, GPU drawing или визуальный результат эффекта.

Host с ASan/UBSan/leak checks и ARM64/QEMU: runtime PASS, по две общие сессии;
в каждой два создания/освобождения FireStream.
devkitA64: compile/link PASS, strong UND = 0, ELF SHA-256:
`ae6013d39a5b10f9af642e62439095d410235c873195a8b7795f7fb2bfa572e7`.
MSVC: compile PASS полного production TU; Windows runtime здесь не выполнялся.
CMake configure/generate и состав effective sources проверены; повторной полной
сборки всего движка нет. Использована уже проверенная база предыдущего среза.

## Точная граница

Force-link оригинального map owner: **21 unresolved вместо 23**.
Закрыты CMatrixEffectSmoke constructor и CMatrixEffectFireAnim constructor;
новых unresolved symbols нет. Точный список оставшихся зависимостей в evidence.
Он включает landscape/explosion/weapon effects, настоящих владельцев
side/robot/building/cannon/flyer, map Trace, hint cleanup и special robot creation.
Это измерение замыкания зависимостей, а не процент готовности или запуск карты.

Smoke/Fire factories обращаются к frustum настоящей карты ещё до создания
объекта; FireAnim constructor читает настоящее время карты.
Эти пути сохранены полностью, но **не выполнялись**. Smoke/Fire Takt,
SpawnPuff, FireAnim simulation и все Draw в этом срезе не проверены runtime.
Конструкторы/виртуальные методы находятся в полном скомпилированном модуле,
что само по себе не доказывает игровое поведение.

Причины дефектов основной игры — CPU raster owner, исторические
manual-publish ENOENT и freezes — остаются OPEN. Этот срез не затрагивает
сохранения, ввод, таймер или основной renderer.
Полный CGame::Init, создание карты, RunGameLoop, CFormMatrixGame и бой ещё
не выполнены. Switch runtime — HARDWARE PENDING. Нового NRO нет.

Все точные SHA, команды, оставшиеся map-owner symbols и проверки сохранности:
[evidence](matrixgame-e2e519-smoke-fire-evidence.json).
Frozen NRO/ELF, два аппаратных ZIP, доступные protected inputs и 107 аппаратных
файлов сверяются по исходным SHA. G:/pack.zip недоступен и не считается проверенным.
Сейвы, ресурсы и настройки консоли не менялись. Push не выполнялся.
