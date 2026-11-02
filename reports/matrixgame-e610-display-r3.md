# MatrixGame E610 r3: VI owner и ориентация presentation

**Исправления исходников; аппаратное подтверждение r3 ещё не получено.**
Основание — пользовательские журналы и снимок из C:/pack.zip, разобранные
в [отчёте r2](matrixgame-e610-r2-hardware-20261102.md).

## Изменения

1. Standalone entry создаёт SwitchDisplay, который явно владеет VI service,
   display, layer и NWindow. Он живёт дольше SwitchWindow, DekoDevice и Init runtime.
   ReadAdapterDisplayMode использует тот же живой display, не открывая и не
   закрывая его повторно. Реальные resolution и VBlank timing сохранены.
   Заимствованному окну без явной display binding запрос режима отказывает;
   ID дисплея и частота не подставляются константами.
2. Удалены ссылки standalone/backend на nwindowGetDefault. libnx открывает
   собственный default display до main, когда default_window.o включён в ELF;
   такой скрытый второй владелец несовместим с нашим прежним повторным OpenDisplay.
   Новый standalone использует стандартные публичные SDK calls для своего окна,
   без замены функций SDK и без изменения SDK/upstream.
   При первом link обнаружено, что SDL_switchvideo.o также подтягивает
   nwindowGetDefault. Для двух standalone целей добавлен стандартный linker
   --wrap=nwindowGetDefault: SDL получает настоящее окно SwitchDisplay.
   При отсутствии живого владельца accessor аварийно отказывает, не возвращая
   фиктивное окно. Приёмочный link обязан не содержать default_window.o startup
   (__nx_win_init / __nx_win_exit) и обычный nwindowGetDefault.
3. Gamma vertex shader согласован с DkDeviceFlags_OriginUpperLeft:
   texel row 0 отображается в target row 0. Геометрия, матрицы игры, текст,
   UV исходных ресурсов и горизонтальная ось не менялись.
4. В GPU smoke добавлен identity presentation readback всего асимметричного
   кадра с сравнением против первичного framebuffer. Отклонения RGB более ±1
   или любое отличие alpha приводят к FAIL. Отражённый кадр не может пройти
   только за счёт правильного центрального пикселя. Постоянная gamma-LUT
   проверяется отдельным последующим кадром.
5. NACP titles: MatrixGame r3 Quick Smoke и MatrixGame r3 Interactive.
   Пути ресурсов r2 сохранены; журнала/вывода — отдельные run directories.

Оригинальный интерфейс конструктора SwitchWindow(NWindow&, bool) сохранён
для существующих зависимостей; новый standalone передаёт явную display binding.
Полная Campaign/Gov entry по-прежнему не создаётся и не проверяется.

## Адресные проверки

- Windows/MSVC: ownership failure injection для всех шести стадий acquisition/
  configuration и проверки pixel checker, включая отражения X/Y и alpha: PASS.
- Host ASan/UBSan/LSan и ARM64/QEMU UBSan: те же короткие проверки — PASS.
  Это тесты реального generic ownership/checker кода; VI backend на host не исполнялся.
- Exact production GLSL на Mesa, GL_UPPER_LEFT / GL_ZERO_TO_ONE:
  21 gamma ramp, 69888 pixels, alpha и исходная текстура сохранены — PASS.
- Negative control: shader из r2 в том же режиме провалил сравнение пикселей
  (exit 6/assertion); assertion не ослаблялся. Это воспроизведение программной
  ошибки соглашения о начале координат, не аппаратная приёмка Switch.
- Большие Windows боевые сценарии и полные регрессии не повторялись.

Локальные результаты: build/matrixgame/e610-display-r3-checks/
(cpu-result.json, sanitizer-arm64-result.json, gl-result.json и logs).

## Источники API и граница

Сверены установленные libnx 4.12.0 headers и официальная реализация
[default_window.c v4.12.0](https://github.com/switchbrew/libnx/blob/v4.12.0/nx/source/display/default_window.c),
[native_window.c v4.12.0](https://github.com/switchbrew/libnx/blob/v4.12.0/nx/source/display/native_window.c),
[vi.c v4.12.0](https://github.com/switchbrew/libnx/blob/v4.12.0/nx/source/services/vi.c).
Контракт origin/scanout проверен по
[deko3d swapchain](https://github.com/devkitPro/deko3d/blob/master/source/dk_swapchain.cpp)
и установленному deko3d.h; master-ссылка — поясняющий источник, не pin SDK.
Используются прежние локальные libnx/deko3d, их версии/хеши фиксируются сборкой.

Следующий аппаратный запуск должен подтвердить одиночный VI owner, настоящий
mode query, A.orientation.presentation_check, полный Init и четыре видимых
кадра карты. До него **HARDWARE PENDING**; физическое изображение/ввод,
боевые действия, Campaign/Gov и победа не объявляются пройденными.

[Инструкция r3](../docs/matrixgame-e610-display-r3-readme.ru.md).

## Сохранённая первая сборка

`build/matrixgame/e610-display-r3/1` (source 998a4cd53d6ec59d5308533ed6dbeb640820737e):
compile/link завершились, но package gate остановился на прежнем ожидаемом
NACP title. Эта попытка не публикуется как PASS. Кроме того, nm/map выявили
SDK default_window.o из SDL_switchvideo.o. Последующая попытка использует
явную SDL binding и проверяет ожидаемые новые titles, сохраняя проверку NACP,
его размер и точное соответствие встроенного ресурса.
