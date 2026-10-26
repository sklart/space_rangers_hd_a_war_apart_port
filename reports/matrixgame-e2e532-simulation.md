# E2E-5.32 — original native Takt

Продолжение после native flat/stencil: исходный CFormMatrixGame::Takt добавлен
в native-entry ELF и в порядок Init → Takt → acquire → Draw → teardown.
Полный игровой цикл и Switch Init/map/frame остаются PARTIAL / HARDWARE_PENDING.

## Что изменено

Генератор извлекает целиком исходный Takt из MatrixFormGame.cpp. Меняется только
экспортируемое имя на TaktOriginalNativeGame; тело сравнивается с upstream
побайтно после обратного переименования. Сохранены CheckLostDevice, вызов
CMatrixMapLogic::Takt, исходные mouse-border, console/arcade и key-action ветки
камеры. 252 прежних generated файла не менялись; добавлен один файл.

В native probe прежний отдельный CheckLostDevice заменён полным Takt.
Acquire выполняется после него, чтобы release/store/reset мог работать без
захваченного display frame. Если video resources ещё не готовы, probe явно
отказывает до acquire; пропущенный Draw не объявляется успешным кадром.
Это по-прежнему один шаг и один кадр, а не L3GRun или полный CForm.

## Что действительно выполнялось

| Профиль | Результат |
| --- | --- |
| COMPILE/LINK | devkitA64: полная native-entry ELF с original Takt, CMatrixMapLogic::Takt, CMatrixMap::Takt, Side::LogicTakt и Init; 0 strong undefined. UAM shaders и остальные зависимости собираются обычной целью. NRO не создаётся. |
| WINDOWS HAL | Две настоящие сессии Init/PrepareMap → paused Takt/Draw → original PressHintButton(HINT_OK) → десять Takt(25)/Draw → Deinit. После снятия стартового диалога игровое время каждой сессии выросло ровно на 250 мс. Реальные вызовы HAL: 11887 draws / 10748 indexed / 1203666 primitives на сессию, включая paused кадр. Cleanup и device restore прошли. |
| HOST ASan/UBSan | NOT RUN для нового simulation delta. CPU-модель карты или фиктивное GPU Init не выдавались за исполнение original Takt. Прежние адресные проверки backend остаются отдельным доказательством. |
| ARM64/QEMU | NOT RUN для нового simulation delta. SDK ARM64 compile/link не является QEMU runtime. |
| SWITCH HARDWARE | NOT RUN / HARDWARE_PENDING: полный native Init, карта, Takt, кадр, управление и бой на Switch не подтверждены. |

В первом HAL-тесте ожидалось, что Init сразу снимает паузу. Реальное исполнение
показало исходный стартовый диалог: Init вызывает EnterDialogMode и Pause(true).
Тест исправлен: он сначала проверяет, что paused Takt не двигает игровое время,
затем использует тот же оригинальный HINT_OK callback, что Keyboard(Enter).
Производственная логика паузы/диалогов не менялась. Прямая запись pause flags
ради PASS не используется.

Сохранены actual HAL readback кадры map-frame-0.png и map-frame-1.png; первый
визуально проверен: карта, объекты, вода, UI и кириллический текст присутствуют,
стартовый диалог закрыт. Эти изображения получены на Windows, не на Switch.
Детерминизм кадров между независимыми сессиями не заявляется: wall clock и
анимация сохраняют исходное поведение. Это 250 мс симуляции на сессию,
не длительная проверка боя, AI или игрового управления.

Команды и входы: build/matrixgame/e2e532-simulation/{sdk,windows}/result.json.
SDK: 180 link inputs; ELF SHA256
a1ed4798a5db6aee25fcc1b8b4f8441a0ff6e08237d907a12554587a0567d91e.
Воспроизведение: tools/matrixgame_simulation_checks.py,
tools/matrixgame_simulation_windows.py, tools/matrixgame_simulation_audit.py.
SDK не исполняется; Windows harness использует прежние проверенные HAL objects
с неизменными declarations и новый original Takt object. Cache/output каждой
проверки размещается в новом приватном каталоге внутри build.

## Следующая реальная зависимость

Проверка состава цели выявила: MatrixFormGame.cpp целиком не входит в native
library. Текущие EngineSink callbacks отправляют события в g_FormCur, но Init
сам по себе не создаёт и не выбирает CFormMatrixGame. Исходный launcher делает
это отдельно через конструктор/FormChange, а CGame::RunGameLoop вызывает L3GRun.
Следовательно, цикл вокруг одних извлечённых Takt/Draw не закрывает ввод.

Следующий этап — настоящие CFormMatrixGame constructor/Enter/Leave,
MouseMove/MouseKey/Keyboard/SystemEvent и исходный L3GRun с native applet/HID
pump. Enter должен использовать исходные default material/light состояния;
SystemEvent — исходные release/recovery paths. Win32 screenshot и fullscreen
cursor clipping остаются отдельными платформенными вызовами внутри form;
их нельзя молча превратить в успешные пустые обработчики.

Первый следующий runtime-отказ Switch не наблюдался. Линковка Takt не доказывает
завершение Init, правильность изображения, управление или бой на консоли.
Полный release/reset/recreate и GPU queue failure recovery остаются открытыми.

NRO, frozen, игровые ресурсы и сейвы не меняются; commit/push нет.
Финальный аудит PASS: 15 защищённых артефактов и 107 аппаратных входов
сохранили fingerprints; upstream pin/чистота и родительский HEAD сохранены.
P0, причинная роль GI cache в повторяющемся тексте, manual-publish ENOENT
и исторические Switch hangs остаются OPEN.
