# E2E-5.49 — explicit surface readback до оригинального Init

Продолжение офлайн-доработки после двух ручных аппаратных запусков.
Новых Switch запусков и новой NRO нет.

## Обнаруженная граница раннего smoke

RunSmoke исполняется до InitializeNativeGame. Его BC1 framebuffer check
вызывал `native_surfaces::Read`, а тот — `Active → ReadyDevice`, требующий
уже согласованную 3G-платформу. До original Init эта готовность отсутствует.
Поэтому исправленный applet poll сам по себе не позволил бы завершить smoke.
Это установлено по исходникам и воспроизведено адресным CPU-тестом; фактический
аппаратный run2 остановился раньше, в appletGetMessage, а не в readback.

## Изменение

Добавлен `native_surfaces::ReadFor(session, surface)`. Он проверяет live session,
принадлежность surface, texture locks, depth/lock status, заменённую presentation
surface, доступность readback callback и точный размер возвращённых байтов.
Лишь после этих проверок исполняется тот же настоящий callback backend.
В smoke передаётся Session от уже созданного DekoDevice. Никакие флаги ready
не выставляются; original platform/window/device Init не пропускается.

Обычный `Read(surface)` сохраняет проверку глобального Active/ReadyDevice
и делегирует проверенному ReadFor. Все прежние корректные входы используют
тот же callback и те же ownership/lock/type/byte-count проверки. Для null
surface вместо разыменования добавлен явный invalid_argument.

Windows offscreen branch использует исходный D3D9 GetRenderTargetData и не
вызывает native ReadFor. Общие контракты original Init/Draw и Windows SDK
ресурсов не менялись; полная Windows HAL регрессия здесь не повторяется.

## Проверки

| Gate | Покрытие |
| --- | --- |
| HOST ASan/UBSan | Прежний `3G device negotiation not complete` воспроизведён через production Read. ReadFor выполняет byte-exact чтение при ready=false/device=nullptr. Проверены null/foreign/depth/locked/read failure/short data/missing callback/replaced presentation/closed session. |
| ARM64/QEMU | Тот же production surface API с явным CPU transfer driver. Не выполняет GPU или полный Init. |
| Existing surface regression | Get/create/bind/viewport/lock/refcount/retained owner, OOM/read-write failure/retry, original offscreen scope rollback, lease/recreate/close. Assertions сохранены. |
| COMPILE/LINK | Production smoke с ReadFor и полный diagnostic devkitA64 ELF с original Init/Draw; strong symbols проверяются SDK profile. |
| WINDOWS HAL | NOT REPEATED: неизменная отдельная D3D9 ветка. |
| SWITCH HARDWARE | ReadFor, исправленный poll, smoke draw/readback/Present и полный Init NOT RUN / HARDWARE_PENDING. |

В тесте не создаётся фиктивный успешный native Init: глобальная ready остаётся
false на всём раннем блоке. Transfer callback — явная тестовая CPU-модель,
отделённая от production DekoTextureStore::ReadImage в SDK ELF.

Команды из текущего подготовленного workspace:

```powershell
python tools/matrixgame_surface_checks.py host --output build/matrixgame/e2e549-preinit-readback
python tools/matrixgame_surface_checks.py arm64 --output build/matrixgame/e2e549-preinit-readback
python tools/matrixgame_actual_draw_checks.py sdk --output build/matrixgame/e2e549-preinit-readback --diagnostic-source build/matrixgame/e2e543-journal/source
```

Итоговый аудит: `matrixgame-e2e549-preinit-readback-evidence.json`.
Прежние E525 результаты не перезаписаны. Оба переданных диагностических пакета,
основная NRO, upstream, frozen, ресурсы, аппаратные входы и сейвы сохранены.
Commit/push и третьей NRO нет.

Последняя наблюдённая аппаратная граница остаётся run2 appletGetMessage=59905.
После E547/E548/E549 исходники продвинулись, но следующую реальную GPU ошибку
без нового запуска назвать нельзя. Первая загрузка карты и управление боем
по-прежнему открыты; офлайн-работа продолжается.
