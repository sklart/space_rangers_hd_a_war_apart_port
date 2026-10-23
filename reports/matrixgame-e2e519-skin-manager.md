# E2E-5.19 — полный skin manager и повторная очистка

24.10.2026. Продолжение `82ad129`, локальный master, без push.

Полный оригинальный `MatrixSkinManager.cpp` подключён к общему native-cache.
Сохранены GetSkin, Takt, Prepare и все исходные 2/3/4/6-stage callbacks,
ветвления side/ordinal/no-alpha, gloss/mask/back и математика UV.
Пять запросов текстур направлены в уже существующий типизированный
`legacy_texture::GetManaged`, вместо приведения portable CacheTexture к
несовместимому original CTextureManaged. Transform, texture, render/sampler
operations используют существующие адаптеры. Неподдерживаемое устройство
отклоняется; успешные draw-вызовы не имитируются. Статики по-прежнему
определены единожды в GameStaticResources.cpp, layout заголовка не менялся.

## Доказанный дефект и исправление

В общем bootstrap настоящий GetSkin с пустым набором текстур выделяет
skin record и массив на настоящем game heap. Затем оригинальный Prepare
отказывает при запросе capabilities отсутствующего устройства. Это
отрицательная проверка unwind/cleanup, не готовый материал и не fake GPU.

Исходный Clear освобождает записи и массив, но оставляет прежние указатели
и счётчики. Второй Clear в baseline реально завершился ASan
**heap-use-after-free** на чтении массива: stack содержит original GetSkin
allocation, первый Clear/free и второй Clear/read. Лог:
`build/matrixgame/e2e519/skin-baseline/runtime.log`.

Исправление: после исходного освобождения каждой группы обнулить её указатель
и счётчик. В кандидате проверены две группы ordinal/no-alpha, два цикла
GetSkin→refusal→Clear→Clear→Takt и повторный вход без StaticInit между циклами,
в каждом из двух сеансов общего владельца. Нет ручного доступа к private
массивам, подставных текстур и фабрикации device caps.

**Ограничение:** исходная публикация записи до Prepare сохранена. При отказе
Prepare запись ещё не является готовым материалом; перед повторным GetSkin
нужен Clear. Exception-transactional publication и allocation-failure injection
здесь не реализованы и не подтверждены. Успешный GetSkin с реальной текстурой,
Preload/callback rendering и изменение UV живого материала NOT RUN.

## Результаты и граница

- Host ASan/UBSan/leak-check: exit0, оба сеанса; прежний baseline воспроизвёл
  UAF. Skin TU требует `-fno-sanitize=vptr`: callback table удерживает ссылки
  на RTTI настоящих ещё не перенесённых world-object классов. ASan и прочие
  UBSan проверки сохранены. Также остаются прежние Console global-redzone
  и Logic/Map vptr исключения. Это не полный набор sanitizer checks.
- ARM64/QEMU: exit0, тот же общий bootstrap и два сеанса.
- devkitA64: COMPILE/LINK PASS отдельного ELF, strong undefined=0;
  CSkinManager/GetSkin присутствуют. SHA-256
  `aec0333b6d5dfc3f334df9ecd3da2ae77111f521908c652ab902831d698ecec7`.
  Runtime/Switch HARDWARE PENDING; новая NRO не упаковывалась.
- Fresh CMake configure/generate PASS; обе skin compile recipes содержат
  function/data sections и документированное vptr исключение. Полная повторная
  сборка CMake не выполнялась. Windows skin compile/runtime в этом шаге NOT RUN.

Настоящий CMatrixMapLogic ctor/dtor по-прежнему **LINK FAIL**, но теперь
осталось **15** unique missing symbols вместо16: CSkinManager::Clear снят.
Точный список в [evidence](matrixgame-e2e519-skin-manager-evidence.json).
Мир, CGame::Init, RunGameLoop, CFormMatrixGame и бой **NOT EXECUTED**.

Сверены242 engine files в двух независимо подготовленных views; отличается
от water stage только MatrixSkinManager.cpp. Отдельно сверены66 core outputs
в трёх views. Build evidence записан до коммита, `build_dirty=true`.
Совпали13 доступных protected inputs,107 аппаратных файлов, frozen main
NRO/ELF и оба аппаратных ZIP. G:/pack.zip недоступен. Пользовательские сейвы,
ресурсы и старые комплекты не менялись.

Новых доказательств причин повторяющегося intro raster, двух исторических
manual-publish ENOENT и зависаний карты/консоли не получено. Эти вопросы OPEN.
