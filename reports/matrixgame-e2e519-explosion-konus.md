# E2E-5.19 — оригинальные взрыв и конус всплеска

24.10.2026. База `d806776be23c7512ce804c583c13e1e0c9af5a10`.

В общий native-engine target включены полные оригинальные
`Effects/MatrixEffectExplosion.cpp` и `Effects/MatrixEffectKonus.cpp`.
Сохранены десять исходных explosion presets, обработка обломков/искр/огня,
столкновений с землёй/водой, создание всплеска и исходные освобождения ресурсов.
Это перенос production-кода, а не замена боя тестовым сценарием.
Фабрика взрыва и настоящая карта пока не исполнялись.

## Исполненный новый путь

В общем bootstrap без фиктивной карты выполнен оригинальный
`CMatrixEffectKonusSplash` constructor → исходный Takt(0) → Takt(100) → Takt(200).
Вызвана исходная PrepareDX: создано 9 исходных вершин конуса (180 bytes),
повторная PrepareDX сохраняет тот же shared VB.
Проверены два владельца, texture refs +2 → +1 → baseline и оба порядка Release.
После последнего владельца VB уничтожен; texture registry и staging allocations
возвращаются к исходным значениям. Повторное создание/освобождение пройдено
в двух общих сессиях host и ARM64/QEMU.

Текстура — настоящий caller-owned CTextureManaged, ещё без SDK image.
Путь не доказывает загрузку игровых текстур или визуальный результат.
Draw честно достигает отсутствующего device provider и отказывает.
GPU draw, BeforeDraw texture preload и ветка TTL expiration с SubEffect не исполнялись.

Height/radius/Z после 300 ms сверены независимым scalar oracle:
индекс округляется на сетку из 256 значений, затем вычисляется std::sin.
Production TableSin и её float bit-trick не используются в oracle.
Допуск после дискретизации — 2e-5.

## Реальные изменения адаптации

- Полные CPP включены в native-cache и штатный CMake.
- Два исходных statics CMatrixEffectKonus::m_VB/m_VB_ref теперь определены
  в полном оригинальном Konus.cpp. Их прежние копии удалены из GameDefaultsNative;
  типы, начальные значения и порядок static_init не менялись.
- Исходные SDK state/draw/texture вызовы используют существующие typed adapters.
  Triangle fan сохраняет SDK ordinal 6. Графические пути взрыва runtime не проверены.
- Добавлен math adapter RotationYawPitchRoll через DirectXMath с правильным
  порядком pitch/yaw/roll. Против настоящего D3DXMatrixRotationYawPitchRoll:
  10 045 наборов углов PASS, максимальная абсолютная ошибка компонента
  5.36441803e-7 при допуске 5e-6. Это SDK math oracle, не GPU proof.

Все 15 тел исходных методов и десять presets сверены содержательно после
только перечисленных адаптеров. Новых sanitizer exclusions нет.
66 portable-core outputs остались прежними; effective engine по-прежнему 248 файлов.

## Проверки и неудачные тестовые setup

Host ASan/UBSan/leak checks и ARM64/QEMU проверяют новый путь в общем bootstrap.
devkitA64 compile/link PASS, strong UND = 0; итоговый ELF SHA-256:
`522a14787e36fb70e56f25de5e02a9460ded61199fe48bb941ae81e9601124d8`.
Оба полных production TU скомпилированы MSVC. Windows execution здесь
подтверждён только для нового SDK math oracle.
CMake configure/generate проверен; повторной полной сборки всего движка нет.

Два первых setup не принимались как PASS:

1. Попытка получить SDK texture по имени уже существующей portable cache entry
   была правильно отвергнута. Тест переведён на настоящий caller-owned CTextureManaged;
   production cache не менялся.
2. Непрерывный sin с допуском 0.04 был неверным эталоном для исходной таблицы
   из 256 значений. Исправлен независимый oracle с дискретизацией и более строгим
   допуском. Формула игры не менялась.

Логи обоих setup сохранены отдельно. В финальном прогоне повторно компилировался
только изменённый тест; неизменные production objects переиспользованы с проверкой
SHA исходников и записью SHA объектов.

## Оставшаяся граница

Force-link настоящего map owner: **17 unresolved вместо 21**.
Закрыты CMatrixEffectExplosion constructor, ExplosionBuildingBoom,
ExplosionBuildingBoom2 и ExplosionObject. Новых unresolved symbols нет.
Это результат замыкания зависимостей, не запуск мира или процент готовности.
Остались landscape/weapon effects, side/robot/building/cannon/flyer owners,
map Trace, interface hint cleanup и special robot creation; точные symbols в evidence.

Конструктор взрыва требует настоящего набора debris из CMatrixEffect::Init;
фабрика ещё раньше обращается к frustum, высоте и списку эффектов карты.
Не создавались фиктивные debris, карта или результаты боя.
Explosion constructor/Draw/Takt/destructor, terrain collisions и водный всплеск
из самого взрыва runtime **не проверены**.
Полный CGame::Init, карта, RunGameLoop, CFormMatrixGame и бой не запускались.
Switch/deko3d — HARDWARE PENDING. Нового NRO нет.

Команды, SHA, оставшиеся symbols и проверки сохранности:
[evidence](matrixgame-e2e519-explosion-konus-evidence.json).
Основная NRO/ELF, оба аппаратных ZIP, доступные защищённые файлы,
сейвы/ресурсы и 107 аппаратных материалов сохранены. G:/pack.zip недоступен.
Причины CPU raster, исторических ENOENT и freezes основной игры остаются OPEN.
Push не выполнялся.
