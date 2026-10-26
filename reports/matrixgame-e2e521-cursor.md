# E2E-5.21: native cursor/input provider

25.10.2026. **PARTIAL / HARDWARE_PENDING.** `SwitchWindow::ExchangeCursor`
реализован вместе с native cursor storage, RGBA bitmap path, icon overlay и HID
input adapter. Полная original Init/frame SDK ELF слинкована. Исполнения нового
HID, cursor overlay и Init на Switch нет; первая карта на Switch не загружена.

## Что изменено

- ExchangeCursor принимает null или живой icon своего provider, возвращает
  точную прежнюю identity. Чужой/уничтоженный token отвергается; ID не переиспользуются.
  ReadCursor и DestroyCursorIcon используют тот же registry. Выбранный icon
  сначала нужно заменить; null destroy допустим. Shared snapshot удерживает
  пиксели после удаления handle без use-after-free.
- Сохранены оба исходных пути CMatrixCursor. Software atlas продолжает рисовать
  оригинальный Draw. Для icon path BitmapFrame сохраняет настоящий RGBA frame,
  CreateIcon копирует его с исходным hotspot; исходные Select/Takt продолжают
  переключать кадры. GDI/COM handles на Switch не фабрикуются.
- Native icon перед present загружается в реальную texture arena и рисуется
  через существующие material shaders/deko commands. Сохраняются alpha, порядок
  строк, размер и hotspot; RGBA преобразуется в BGRA для A8R8G8B8 upload.
  Используются point sampling, alpha blending и отключённый depth. Overlay
  передаёт отдельный NativeRenderState в Record, не редактируя захваченное
  состояние оригинальной отрисовки. Выполнение GPU ещё не проверено.
- System display counter и device-cursor visibility хранятся раздельно, как в
  исходных вызовах ShowCursor/IDirect3DDevice9::ShowCursor. Icon overlay следует
  system counter; device-cursor image исходная игра здесь не устанавливает.
  При detach восстанавливаются исходные selection/counters/position provider.
- HID adapter использует настоящие libnx shared-memory mouse, keyboard, touch
  samples и Npad. Собственные HID service shutdown и Npad reconfiguration не
  выполняются. Применяется документированная libnx peripheral activation.
  При входе/resume старые samples отбрасываются; в обычном poll до 17 samples
  каждого источника разбираются от старых к новым с дедупликацией.
- Input router объединяет удержания источников: отпускание mouse, touch или
  Npad не отменяет удержание другого. Координаты ограничены drawable; touch
  1280×720 масштабируется в его extent и удерживает выбранный finger id.
  Focus loss отпускает логические keys/buttons; detach восстанавливает ранее
  сохранённый original Input bitset. Нет Win32 message injection.
- EngineSink вызывает настоящие `Input::onKeyDown/onKeyUp`, `CForm::Keyboard`,
  `MouseMove`, `MouseKey`; исходные key actions остаются в g_Config. Native
  double-click policy — 500 ms / 4 px; typematic — 500 ms / 33 ms, catch-up
  ограничен четырьмя повторами за poll. Это явные native defaults, не чтение
  пользовательских Windows preferences.
- Исправлено владение при частично созданных оригинальных icons: массив слотов
  обнуляется перед заполнением. При отказе следующего CreateIcon cleanup не
  пытается уничтожить неинициализированные handles. Алгоритмы Select/Takt/Draw
  и графические ресурсы сохранены.

Npad mapping: A/B — левая/правая кнопка; правый stick — указатель; D-pad —
настроенные scroll actions; левый stick — unit movement; L/R — rotation;
ZL — Shift action, ZR — Fire; X — unit enter, Y — cancel, Plus — Escape.
Клавиатура остаётся доступной для остальных исходных key actions. Это ещё не
аппаратная приёмка полного controller UX или ввода текста/раскладок.

HID contract сверялся с установленными headers и
[официальным libnx hid.h](https://github.com/switchbrew/libnx/blob/master/nx/include/switch/services/hid.h).
Хэши установленных hid/pad/applet headers записаны в evidence. WheelDeltaY
передаётся как native wheel units; физические wheel direction/scale, touch,
hotplug и Npad требуют проверки на консоли.

## Что действительно исполнилось

| Профиль | Результат и граница |
| --- | --- |
| Host ASan/UBSan/leaks | Native Store/Router/EngineSink, оригинальные CForm/Input и настоящий CMatrixConfig fixture: PASS |
| ARM64/QEMU | Те же CPU методы: PASS |
| Windows x86 MSVC | Те же CPU методы: PASS |
| Windows D3D9 HAL | Два оригинальных Init/frame/Deinit с обновлёнными MatrixCursor/cursor_backend objects: PASS; 1008 draws каждый, snapshots совпали побитово |
| devkitA64 | Новые cursor/input/provider/MatrixCursor objects + полная original Init/frame ELF: compile/link PASS, strong undefined=0 |
| Switch | HID service calls, BitmapFrame/icon GPU upload/render, native Init/PrepareMap/frame: NOT RUN |

CPU-проверки покрыли null/previous identity, foreign/dead handles, selected icon
ownership, retained pixels, RGBA/alpha/BGRA, hotspot/quad, оба счётчика/restore,
три принудительных OOM в Store::Create, merged input, double click, wheel,
touch coordinates, focus release, typematic, inactive/null form, восстановление
original Input и чтение original configured key action. CPU fixture не
имитирует успешные HID IPC, GPU или CGame::Init.

Windows контроль выполнен из-за изменения общего icon unwind. Кадр 800×600,
154038 RGB-цветов; SHA-256 обоих PNG:
`f6f0d25c1e7b2a194f6523f8c96b9a74f7b7385525e5bb5cf7696de7f09a52bb`.
Принудительные OOM в полном CMatrixCursor::Select и физический GPU allocation
этими тремя Store-тестами не заявляются. CMake target добавлена; полный root
CMake rebuild и большие прежние material/residency suites не запускались.

## Точная граница до первой карты

**Исправление предыдущего отчёта:** ExchangeCursor был назван первым блокером
по цепочке создания карты без учёта declaration order членов CMatrixMap.
Поле `m_DI` объявлено раньше `m_Cursor`; его конструктор вызывает backend первым.
Текущая установленная по исходникам граница при успешных предыдущих операциях:

`CGame::Init → создание CMatrixMapLogic → CMatrixMap::m_DI →
CMatrixDebugInfo::CMatrixDebugInfo → debug_font::Create → CreateFor →
Unsupported("debug font SDK creation unavailable on this backend")`.

Create добавляет префикс `original debug font Create:` к этой ошибке. Это
происходит до `RobotPreload`, storage Load и PrepareMap. Успешная линковка
не исполняет конструктор и не закрывает эту границу. На консоли она не наблюдалась.

Следующий шаг — настоящий font provider с исходными glyphs/metrics/draw и
lost/reset lifecycle, затем используемые картой stored-state/offscreen/shadow/
text/video/presentation paths. Win32 cursor clipping остаётся явно unsupported,
а не молчаливым PASS. Полный native Takt/Draw loop, встраивание в хозяина и
аппаратный Init/PrepareMap/первый кадр/recreate/teardown остаются открытыми.
Минимальный bootstrap и полная original Init/frame цель разделены.

## Сохранность

Аудит: прежние хэши 15 доступных защищённых входов, дополнительных frozen
ELF/NRO, основной и root cumulative NRO, 107 аппаратных файлов, robots.pkg,
CFG и frozen cache source. `G:\pack.zip` недоступен, его сохранность здесь не
проверяется. NRO не создана; комплекты, ресурсы и сейвы не изменены. Коммита
и push нет.

P0 основной игры, GI repeating-text causality, `manual-publish ENOENT` и
зависания Switch остаются OPEN. Нового воспроизведения на дефектном экране или
новой аппаратной проверки не было.

Команды: `tools/matrixgame_cursor_checks.py host|arm64|windows|sdk`,
`tools/matrixgame_cursor_windows_frame.py`, `tools/matrixgame_cursor_audit.py`.
[Evidence и хэши](matrixgame-e2e521-cursor-evidence.json);
логи/ELF/контрольные PNG — `build/matrixgame/e2e521/cursor/`.
