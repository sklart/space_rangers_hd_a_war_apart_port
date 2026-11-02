# E608: production UI-контекст в полном Windows-бою и возврате

Проверка PASS: новый `RangersUiContext` E607 действительно использован вместе
с оригинальной кампанией и полным MatrixGame-боем. Это совместная runtime
проверка на Windows D3D9 HAL, с частичной MSVC ASan-инструментализацией.
Это ещё не native/deko runtime и не новая native точка входа.

Проверенная цепочка:

1. Deferred Host → NativeRangersInterface (Windows reference) → production
   UiContext (оригинальные CFG/TCC/canvas/palette/cursors/depth).
2. Script → GlobalUi → настоящая генерация кампании и warmup.
3. Оригинальные приказы takeoff/landing: ход 301 → 311, без записи координат
   игрока тестом и без загрузки сохранения.
4. Gov layout/OnOpen/OnClose/message-loop stack → memory snapshot → Robot::FRun
   → реальная передача окна через CallerPresentation → полный CGame::Init.
5. Оригинальные Takt/Draw и обычные команды управления/строительства.
6. Естественное поражение: outcome=2, map_time=808000, ticks=32155.
7. Возврат окна, настоящий D3D reset/gamma readback → original memory snapshot
   restore → оригинальные Gov history/statistics/reward/dialog/requested return.
8. Campaign → GlobalUi → Script → UiContext → Host cleanup без оставшихся
   device/timer leases и с восстановлением исходного рабочего каталога.

Проверены карта Winter1_1e (реальный map_id=12), battle difficulty=3,
campaign difficulty=0, economy controller, законная heavy-конструкция и squad.
Результат и характеристики юнитов не задавались тестом. Gov начислил +90,
история увеличилась на одну запись с фактической статистикой, игрок вернулся
на прежнюю планету, PendingTransition обработан до нуля. Естественная победа
по-прежнему NOT PROVEN.

## Артефакты и воспроизведение

```text
python tools/matrixgame_campaign_frun_windows.py --ui-context --caller-lease --difficulty 3 --strategy economy --campaign-difficulty 0 --map Winter1_1e.cmap --design heavy --formation squad
```

Новый флаг `--ui-context` удаляет прежний ручной Caller/SoftwareUi из
сгенерированного сценария и использует настоящий production owner до запуска
script workers. Runner компилирует текущие UiContext и renderer и исключает
прежний renderer object из link inputs. Старые режимы и результаты сохранены.

Результат: `build/matrixgame/e2e608-ui-campaign/windows-difficulty3-economy-campaign0-winter1_1e-heavy-squad-caller-lease/1/result.json`.
В том же каталоге сохранены восемь новых objects, expanded compiler inputs,
link response, binary, полный runtime.log, original Init journal и снимки
19 исходников с `source-snapshot.json`. Исходный result.json не переписывался.

[Машинный аудит E608](matrixgame-e2e608-ui-campaign-evidence.json) проверяет
бинарник/objects/reused inputs, журнал Init и journal_close, исходники и снимки,
natural outcome и original return markers. Приватный `rangers-user` содержит
только пустые каталоги config/logs/runtime/save: файлов нет. Защищённые входы,
frozen-комплекты, чистые upstream pins, основная/root/E590 NRO и E604 ZIP с
его NRO и 198 link inputs проверены без изменений. Commit/push не выполнялись.

E607 host ASan/UBSan/LSan, ARM64/QEMU и SDK compile/link повторно не запускались:
проверены существующие хеши и доказательства. Свежий повторный аудит сохранён
в `build/matrixgame/e2e607-ui-context/evidence-recheck-20261028.json`.

## Открытые границы

Точка входа — Gov::OnOpen. UI StartPlanetBattle autosave handler исключён из
сценария согласно запрету менять сохранения. Основной Galaxy UI Run loop и
физический вывод Gov-интерфейса этим Windows fixture не доказаны.

Нужно подключить native HID к оригинальному Rangers UI message loop, согласовать
координаты с aspect-fit RGB565 presenter, собрать общую native UI/script/
campaign/Gov entry на одном Window/DekoDevice и отдельную стандартную NRO.
Сейчас EngineSink отправляет события форме MatrixGame; при её отсутствии
Rangers UI не получает эти события через этот sink. Старый GR_WinMessage и
Win32 compatibility input используют отдельный pump/state — требуется их
явная интеграция с native источником, включая смену владельца при FRun.
GPU/HID/window поведение на Switch и аппаратная приёмка остаются NOT RUN.
