# E2E-5.48 — command backing при constructor unwind

После офлайн-исправления applet poll проверен cleanup частично созданного
`DekoDevice::Impl`. Успешный деструктор уже сбрасывал `commands` перед backing,
но при исключении из конструктора тело `~Impl` не вызывается. Прежний порядок
полей разрушал dynamic pages и persistent base/discard до command buffer.
После initial presentation reset recorder уже мог удерживать backing.

## Изменение

Порядок объявления теперь: persistent memory → dynamic page owner → recorder.
Обратный порядок автоматического разрушения гарантирует уничтожение recorder
раньше всех этих страниц. `CommandDriver`, SDK вызовы, initial presentation,
texture/backend algorithms, `InitAsNative`, original Init/Draw и лимиты памяти
не менялись. Замена только выделенного фрагмента полей прежним восстанавливает
точный SHA-256 platform_switch.cpp из E547.

## Адресное доказательство

CPU-тест извлекает фрагмент полей непосредственно из production .cpp, включая
настоящий NativeCommandMemory и CommandDriver. Для baseline сохранён прежний
фрагмент. Оба варианта используют installed deko3d.hpp UniqueHandle и настоящие
SDK wrapper reset/destructors. Тестовые C API endpoints наблюдают ссылки и
порядок удаления opaque handles; они не являются GPU backend порта.

В каждом профиле выполняются девять сценариев: нормальный teardown, отказы
выделения base/discard, отказ до создания recorder, исключение после attach,
отказы refill и исключения после создания dynamic pages.

| Gate | Результат |
| --- | --- |
| HOST ASan/UBSan | Baseline: 11 освобождений backing при ещё живом recorder. Candidate: 0; все 9 сценариев без утечек и нарушений sanitizers. |
| ARM64/QEMU | Те же baseline=11/candidate=0, освобождение всех handles. |
| COMPILE/LINK | Полный диагностический devkitA64 ELF с настоящим deko3dd; точные команды и результат в SDK profile. |
| Original generated sources | Сравниваются с run2; изменение алгоритмов Init/Draw отсутствует. |
| WINDOWS HAL | Не повторялся: изменение только порядка native deko владельцев. |
| SWITCH HARDWARE | Исправление NOT RUN / HARDWARE_PENDING. GPU UAF не воспроизводился и не заявляется. |

Это доказательство инварианта владения и корректного C++ unwind, а не
воспроизведение фактического обращения GPU/SDK destructor к освобождённой памяти.
Не следует переносить этот результат на queue-loss recovery или весь partial
device rollback. При потере очереди восстановление остаётся отдельным OPEN.

Команды:

```powershell
python tools/matrixgame_command_unwind_checks.py host
python tools/matrixgame_command_unwind_checks.py arm64
python tools/matrixgame_actual_draw_checks.py sdk --output build/matrixgame/e2e548-command-unwind --diagnostic-source build/matrixgame/e2e543-journal/source
```

Доказательства: `matrixgame-e2e548-command-unwind-evidence.json` и
`build/matrixgame/e2e548-command-unwind/{host,arm64,sdk}`.
SDK/sanitizer выводы относятся только к указанным исполняемым путям.

Оба переданных NRO сохраняются с прежними manifest/SHA-256. Третья NRO не
создаётся, новых аппаратных тестов нет. Upstream, основная NRO, frozen,
robots.pkg, ресурсы и сейвы не изменяются; commit/push нет.
Реальная цепочка MatrixGame на Switch пока остановлена до draw/Present/Init
ошибкой empty applet poll из run2; её E547-исправление аппаратно не проверено.
Дальнейшая офлайн-доработка продолжается без объявления первой карты готовой.
