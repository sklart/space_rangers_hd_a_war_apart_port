# E2E-5.19 — сохранение указателя в callbacks эффектов ARM64

## Доказанная причина и исправление

Оригинальный `CMatrixEffectWeapon::Fire` передаёт `this` как `uintptr_t`.
Общие factories и callback typedef также используют `uintptr_t`, но constructors
и хранилища MovingObject, FirePlasma, Flame и BigBoom принимали `uint32_t`.
На ARM64 старшие 32 бита адреса терялись до вызова обработчика.
Это дефект MatrixGame; связи с зависаниями основной игры не установлено.

В подготовленных исходниках исправлены только типы opaque payload: 12 замен
в восьми CPP/HPP. Colors, light, hitmask и user_param не изменены. Зафиксированный
upstream не редактировался. Overlay проверяет точное количество замен.
Для интеграции добавлены четыре полных оригинальных CPP эффектов с существующими
типизированными адаптерами графики и математики. Алгоритмы не заменены заглушками.

## Воспроизведение до и после

Тест вызывает оригинальную `MOEnum` через прямую forwarding-функцию в том же TU.
Передаётся настоящий адрес stack payload, находящийся выше 4 GiB. Callback
проверяет равенство адреса, затем читает marker по восстановленному указателю.
Оригинальная MOEnum дополнительно вычисляет квадрат расстояния 50; проверена
её ветвь с отсутствующим handler. Карта остаётся nullptr, фиктивный мир не создан.

- До исправления ARM64/QEMU: expected `7b7a0c84e408`, observed `c84e408`, storage 4;
  контролируемый выход 1 `moving-object callback payload truncated`.
  Завершено 0 из 2 сессий, наблюдался один callback. Baseline использует прежние
  вспомогательные объекты; это воспроизведение ошибки, не свежая регрессионная сборка.
- После исправления: полная свежая ARM64-сборка и QEMU runtime PASS, две сессии,
  четыре точных round trips, storage 8. Старые native objects не подмешивались.
- Полная свежая devkitA64 compile/link PASS, strong undefined symbols: 0.
  NRO не выпускалась, Switch runtime **HARDWARE PENDING**.
- Реальные MSVC x86 executables до/после: идентичные размеры четырёх классов,
  SMOProps и смещения полей. `uint32_t` и `uintptr_t` — одинаковый canonical type
  этого Win32 compiler; исходный 32-битный ABI сохранён.
- devkitA64 `-Wconversion` полного MatrixEffect.cpp: четыре прежних предупреждения
  о сужении pointer payload устранены, после исправления таких предупреждений 0.
- Свежая host ASan/UBSan: compile PASS, **LINK BLOCKED**, runtime NOT RUN.
  Не хватает оригинальных RTTI Building, Lightening, Repair, Shleif и RobotAI.
  Новые sanitizer exclusions и фиктивные RTTI не добавлены.

Baseline → fixed source: ровно восемь файлов / 12 замен типов. Все 313 CPP/HPP/H
подготовленного reference дерева побайтово совпадают с тремя свежими сборками.
Это сравнение effective sources, отдельно от source manifest JSON, который
закономерно содержит другие hashes. Не следует трактовать 313 как количество запущенных компонентов.

## Граница исполнения и следующий блокер

Реально выполнен оригинальный callback передачи opaque payload, а не полноценный
выстрел: constructors всех четырёх эффектов, world collisions, WeaponHit и Fire/Takt
этим тестом не исполнены. Для трёх остальных эффектов width подтверждён исходниками
и компилятором, отдельного runtime их callbacks нет. Полные CGame::Init,
RunGameLoop, CFormMatrixGame и планетарный бой ещё не запущены.

Следующий этап — подключение оставшихся оригинальных владельцев эффектов и
игровых объектов для закрытия настоящих зависимостей map owner/RobotAI/Building.
Нельзя считать успешную линковку ограниченного bootstrap запуском всего движка.

## Сохранность

13 доступных protected inputs, 107 файлов аппаратного прогона, frozen main NRO
и matching ELF сверены по SHA-256: изменений нет. G:/pack.zip недоступен, его
текущую сохранность проверить нельзя. Основная NRO и оба hardware ZIP не
пересобирались; сейвы, ресурсы и настройки консоли не менялись. Push не выполнялся.
Источник CPU-композиции повторяющегося текста, исторические manual-publish ENOENT
и причины зависаний основной игры остаются OPEN.

[Команды, результаты, source hashes и бинарные SHA-256](matrixgame-e2e519-callback-payload-width-evidence.json).
