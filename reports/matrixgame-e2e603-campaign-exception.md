# E2E-6.03 — восстановление после C++ исключения в настоящем FRun/Gov

Предыдущий Winter stock attempt3 дошёл до лимита 120000 тактов без исхода.
Исключение fixture завершилось кодом `0xc0000409`; этот запуск остаётся
неуспешным и не считается проверкой возврата, победы или поражения.

В исходном `Robot::FRun` непаскалевское исключение покидало внутренний catch
до GR_DXReset, gamma, громкости и cursor restore. В `TfGov::OnOpen` оно
повторно бросалось до RestoreGameFromMemorySnapshot. Владелец campaign
запрещает уничтожение мира с активным snapshot; этот инвариант сохранён.

`tools/matrixgame_campaign_exception_overrides.py` добавляет два overlays:

- Robot сохраняет исходное exception_ptr, выполняет существующую цепочку
  восстановления вызывающей стороны, затем повторно бросает исключение.
  Существующий finally восстанавливает каталог и LooseFileRoot.
- Gov охватывает LoadPanel/FRun дополнительным catch, восстанавливает мир
  через оригинальный RestoreGameFromMemorySnapshot, очищает незавершённый
  PendingTransition и повторно бросает исходное исключение. Результат боя,
  награда и запись истории не подставляются.

Исходный Pascal error/dialog path и успешный путь сохранены. Если сама
операция восстановления завершится ошибкой, она не подавляется; этот случай
отдельно не инжектировался. Upstream не редактировался. Production generator
теперь маршрутизирует Robot.cpp к overlays; fingerprint source cache включает
новый helper. Старый большой effective-source corpus не перегенерировался.

## Проверки

`build/matrixgame/e2e603-campaign-exception/windows/1/result.json` — PASS,
exit0, partial MSVC ASan:

1. Настоящие host/callbacks/script/UI/campaign, takeoff/landing и Gov OnOpen.
2. Настоящий memory snapshot, NativeRangersInterface и Robot::FRun.
3. Полный CGame::Init и не менее восьми original Draw; затем отдельное
   типизированное исключение из Pump. Это fault injection для error path,
   а не замена обязательной игровой операции или исхода боя.
4. Тот же тип и сообщение дошли до caller. Matrix owners освобождены,
   RobotBattleActive сброшен, snapshot/buffer закрыты, текущий каталог и
   LooseFileRoot восстановлены. Gamma проверена фактическим SDK readback.
5. Оригинальный мир восстановлен: player/planet IDs, turn, money,
   PlanetBattles и PlanetBattleHistory неизменны; PendingTransition=0.
6. Полная очистка campaign/UI/script/host, device/timer leases пусты,
   журнал завершён journal_close. Ни SaveGameToFile, ни UI autosave handler
   не вызывались. Победа или поражение этим тестом не заявляются.

`build/matrixgame/e2e603-campaign-exception/sdk/1/result.json` — PASS:
два **production-generated** файла Robot.cpp/fGov.cpp скомпилированы devkitA64;
в новом отдельном original archive изменены только Robot.o и fGov.o из 378
members. Остальные данные архива сохранены. Retained campaign API ELF
успешно слинкован, AArch64, strong undefined symbols отсутствуют.
ELF: 57188656 bytes, SHA-256
`1991ab40f4078025508b286679143ce899d10e504ee35dddf7be2250908faf21`.
SDK runtime и новая NRO здесь NOT RUN / не создавалась.

[Машинный аудит](matrixgame-e2e603-campaign-exception-evidence.json) PASS:
фактические источники, объекты, binaries/archive/ELF, журнал, protected inputs,
upstream pins и прежние NRO проверены. Полный dependency corpus Windows не
инструментирован ASan; host ASan/UBSan/LSan snapshot evidence E601 относится
к предшествующему коду и не выдаётся за тест этих двух новых exception paths.

## Воспроизведение и открытые границы

```text
python tools/matrixgame_campaign_exception_windows.py
python tools/matrixgame_campaign_exception_sdk.py
python tools/matrixgame_campaign_exception_audit.py --output reports/matrixgame-e2e603-campaign-exception-evidence-new.json
```

Все runners создают отдельные attempt directories; существующие результаты
не перезаписываются. Основной Windows battle runner E602 также компилирует
эти overlays. Обычный Desert2_2e сценарий с economy/campaign0/difficulty3
(`build/matrixgame/e2e602-campaign-frun/windows-difficulty3-economy-campaign0-desert2_2e/1`)
дошёл до лимита 120000 тактов без natural win/loss: map_time3015125,
4 player robots, 15 hostile robots, 14 buildings, одна player base. Это
**INCOMPLETE / не PASS боя**. Исключение лимита теперь дошло до внешнего
`GOV_FRUN_FAILURE CXX=natural mission outcome not reached within 3000000 ms extra simulation`,
после оригинального snapshot load; процесс вернул обычный failure code12,
журнал завершился journal_close. Прежний terminate0xc0000409 не повторился.
Полный естественный outcome/return regression на новых overlays остаётся
неподтверждённым. Runtime log runner теперь записывается во время выполнения,
чтобы следующий долгий прогон можно было наблюдать без перезапуска процесса.

Открыты естественная победа с original completion/reward/history/return,
native/deko интегрированный FRun и новая отдельная диагностическая NRO.
Аппаратный запуск выполняет пользователь. Сохранения, игровые ресурсы,
frozen/upstream и основные/прежние NRO не изменялись; commit/push не было.
