# MG-2: нативный callback adapter — первый исполняемый блок

Статус PARTIAL. Это реализация контракта, не планетарный бой и не интеграция MG-5.
Стабилизационная NRO 3772031 остаётся отдельным ранее собранным кандидатом.

## Реализация

`port/matrixgame/native/rangers_bridge.{hpp,cpp}` вызывает все 16 настоящих типов
`Robot::TRobotCallbacks`. Публичный заголовок не включает Windows, D3D9 или DLL ABI.
SoundId/ImageId — разные типы с порядковыми 32-битными номерами, а не адресами.
Общий monotonic ID allocator не повторяет ID между экземплярами или после Reset;
при исчерпании пространства отказывает до создания native ресурса. Native pointers
остаются в owning tables. Free/Reset/destructor освобождают принадлежащие bridge
звуки и текстовые буферы через исходные callbacks. Таблицы и вызовы сериализованы.

UTF-16 views копируются в mutable NUL-terminated строки. Embedded NUL отвергается.
Native render получает отдельный clip; временные строки и clip не меняют caller input.
Pixels остаются borrowed до FreeText/Reset/destructor. Отказ после публикации
native text buffer освобождает его. При ошибке регистрации созданного ресурса
вызывается соответствующий release callback. Release callbacks должны не бросать;
это требование ownership boundary, не доказательство всех путей главной игры.
Game services должны пережить bridge, потребители — прекратить использование pixels
до освобождения. Bridge не переносит callbacks на UI thread: будущий engine backend
обязан соблюдать threading contract настоящих game services.

Display settings копируются по полям, включая borrowed legacy graphics pointers без
усечения. Они не объявляются GPU backend Switch. Шесть полей statistics сохраняют
знак native elapsed time. Battle result win/loss не синтезируется.

## Проверки

- Native ASan/UBSan + leak detection: PASS.
- Native ThreadSanitizer, четыре callers / 800 sound create-use-free: PASS.
- Те же contract tests настоящим ARM64 Linux executable под QEMU: PASS.
- Отдельный adapter object настоящим devkitA64 16.1.0 с effective Switch headers: PASS.
- Документированные CMake/CTest sanitizer builds в свежих каталогах: PASS.
- Все 16 callbacks действительно вызваны; stale/foreign IDs отвергнуты, после Reset
  и destructor контролируемые sound/image allocations равны нулю. Unicode и частичный
  RenderText failure проверены. На обоих 64-битных runtime pixel pointer выше 4 GiB.
- Settings и все statistics поля проверены, включая INT32_MIN signed time.

Логи и source/binary hashes: `build/matrixgame/native-bridge/{proof.json,asan.log,
tsan.log,arm64.log,switch-compile.log,cmake-asan.log,cmake-tsan.log}`. Producers в тесте
контролируемые; вызовы настоящих main-game audio/font renderer этим не доказаны.
CI steps добавлены в существующие host-regression/e2e5-arm64-host; remote результат
этой новой ветки ещё не получен. Pinned upstream и закрытые ресурсы не изменены.

## Что остаётся

Native engine entry/Run и lifecycle handoff, аудиосистема/шрифты основной игры в
настоящем бою, ARM64 engine ownership, GPU backend, UI robot constructor, win/loss,
кампания и аппаратные два последовательных боя ещё не реализованы/не проверены.
MG-2 остаётся PARTIAL; MG-3…MG-5 OPEN. Следующее проверяемое действие — подключить
native engine interface к этому контракту и проверить entry/cleanup без Windows DLL,
сохранив отказ неподдерживаемого backend до настоящей реализации рендера.
