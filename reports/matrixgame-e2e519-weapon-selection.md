# E2E-5.19 — полные Weapon/Selection и настоящий Selection lifecycle

24.10.2026. База `b18a1d27d21f02adafe43dad40b59df70bb4acc9`.

Подключены полные оригинальные `Effects/MatrixEffectWeapon.cpp` и
`Effects/MatrixEffectSelection.cpp`. Сохранены все виды оружия, Fire/Takt,
обработчики попаданий, физические эффекты и callbacks. CLaser/CVolcano,
определённые в том же Weapon CPP, тоже сохранены полностью.
Два translation units целиком сверены после только существующих typed adapters:
22 именованных метода, вспомогательный код и условные ветви не удалялись.
Headers/layouts и 66 portable-core outputs в этом коммите не менялись.

## Новое фактическое исполнение

В общем bootstrap вызвана настоящая `CMatrixEffect::CreateSelection`.
Приватный исходный constructor создаёт массив точек и CBillboard для радиусов
1 и 25; тест не заменяет его открытым прототипом или дублированной формулой.
Вызваны исходные SetPos/SetColor/SetAlpha/SetRadius/BeforeDraw, Release,
SSelPoint destructor и базовый destructor с очисткой настоящих SEffectHandler backlinks.
Проверены оба порядка освобождения, повторное создание/освобождение, отсутствие
изменений texture registry/staging allocations. Карта остаётся nullptr.

ARM64/QEMU: PASS, две общие сессии; в каждой два порядка и четыре реальных
объекта Selection. Texture slots ещё не загружены. Реальный Selection Takt
читает высоты карты, Draw читает camera — они не выполнялись.
Не доказаны визуальная геометрия/материалы, GPU drawing и heap leak freedom
под ASan: новый host sanitizer runtime пока заблокирован линковкой.

devkitA64: полные CPP compile PASS и общий bootstrap link PASS, strong UND = 0.
ELF SHA-256: `fe095e39feca6ef280d68488d63f4f73aa4c461bc70e40f3fbfd1b4bb5734e5a`.
Это bootstrap с dead-code collection, не полный engine/NRO.
MSVC: compile PASS обоих CPP; Windows runtime не выполнялся.
CMake configure/generate PASS; полной повторной сборки движка нет.

## Подтверждённый ARM64-блокер callback payload

В исходном Weapon передаётся `(uintptr_t)this` в фабрики эффектов; `WeaponHit`
затем восстанавливает из callback user указатель на CMatrixEffectWeapon.
Фабрики уже принимают uintptr_t, но следующая часть цепочки всё ещё 32-битная:

| Effect | Constructor user | Storage |
|---|---|---|
| MovingObject | uint32_t | SMOProps::uservalue — uint32_t |
| FirePlasma | uint32_t | m_UserData — uint32_t |
| Flame | uint32_t | m_User — uint32_t |
| BigBoom | uint32_t | m_User — uint32_t |

Реальный devkitA64 compile полного MatrixEffect.cpp с `-Wconversion` подтвердил
ровно четыре сужения uintptr_t → uint32_t в этих factory → constructor вызовах.
Указатели выше 4 GiB теряют старшие биты до обратного вызова. Это статически
подтверждённый дефект MatrixGame ARM64, **в этом срезе ещё не исправлен**.
Он не установлен как причина аппаратных дефектов основной игры.

Следующая правка должна охватить producer/constructor/storage/callback целиком,
сохранив 32-битный Windows ABI и pointer payload на 64-битных платформах.
Нужен реальный high-address callback round trip в оригинальном projectile path;
проверки простого cast в тестовой функции недостаточно.
После изменения headers/layouts потребуется пересборка всех затронутых
потребителей, а не смешивание старых objects с новым SMOProps.

## Точная граница интеграции

В настоящих production objects определены compiler-generated RTTI/vtable
CMatrixEffectWeapon и CMatrixEffectSelection. Прежние два RTTI-блокера устранены.
Host ASan/UBSan: compile PASS, **LINK BLOCKED**, runtime NOT RUN. Остались RTTI:
RobotAI, Building, Flame, Lightening и Repair. Новых sanitizer exclusions нет.

Production ARM64 map owner: закрыты пять прежних Weapon symbols
(constructor, FireEnd, SetOwner, Modify, SoundHit), открылись 23 реальные
транзитивные зависимости. Итого **40 unresolved вместо 22**.
Новые связи — BigBoom, BillboardLine, FirePlasma, Flame, Lightening, MovingObject,
Repair, Shleif, Shorted и четыре исходных MO_*_Takt handlers.
Это раскрытие полного Fire graph, не процент готовности и не запуск боя.

Weapon constructor/lifecycle, Fire/Takt/WeaponHit, попадания и выстрелы
не исполнялись. Полный CGame::Init, мир, RunGameLoop, CFormMatrixGame и бой
не запускались. Switch/deko3d — HARDWARE PENDING. Нового NRO нет.

## Сохранность и данные

Effective engine — 248 файлов, изменены только два указанных CPP.
Основная NRO/ELF, два аппаратных ZIP, 13 доступных protected inputs и 107
аппаратных файлов сверены по исходным SHA. G:/pack.zip недоступен.
Сейвы, ресурсы и настройки консоли не менялись. Push не выполнялся.
CPU raster owner, исторические manual-publish ENOENT и причины freezes
основной игры остаются OPEN.

[Команды, hashes, original source audit, callback-width diagnostics и blockers](matrixgame-e2e519-weapon-selection-evidence.json).
