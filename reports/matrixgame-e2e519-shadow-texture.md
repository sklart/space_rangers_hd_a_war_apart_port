# E2E-5.19 — оригинальные shadow textures на настоящем D3D9 HAL

24.10.2026. База: `415fe51c7e39449b7825f6116d0bd02f6a9ee801`.
Основная игра и аппаратные комплекты не пересобирались.

## Новая реально исполненная граница

Оригинальная цепочка `CVectorObjectAnim → CalcShadowProjMatrix →
CVectorObject::CalcShadowTexture → CalcShadowTextureWOMat` теперь выполнена
на настоящем Windows D3D9 HAL с исходной Terron VO (`model-2.vo`).
Это адресная проверка интеграции настоящих методов, без создания фиктивной карты.

В новом backend staging-геометрия загружается в настоящие SDK VB/IB;
вызывается `DrawIndexedPrimitive`. Render texture создаётся настоящим
`D3DXCreateTexture`, включая его согласование размера/формата.
Оригинальный inline DrawBegin перенесён из pinned upstream целиком:
BeforeDraw обоих пулов и настоящий SetFVF. CPU recorder здесь не используется.

Для двух направлений света прочитано **1082 и 1009 ненулевых alpha-пикселей**
из 64×64. Проверены оригинальная managed-ветвь копирования маски,
создание unmanaged render texture и повторное обновление с сохранением её
SDK identity. Результаты обновления имеют те же количества alpha-пикселей.
После каждого из шести результатов проверены исходные target/depth identities,
viewport и SDK stream/index bindings, включая offset и stride.
После уничтожения объектов возвращаются исходный texture registry и нулевой
объём staging allocations. Проверка использует HAL с software vertex processing;
это не доказательство hardware vertex processing или deko3d.

## Устранённые зависимости

Полный оригинальный animator удерживал ссылки на отсутствовавшие VO Pick/PickFull.
Перенесены три исходных метода: PickSimple, Pick, PickFull.
Фильтрация вершин, исходные маски (включая `0x7FFFFFF`), обход треугольников
и выбор ближайшего пересечения сохранены. Чтения float через DWORD заменены
на bit_cast; избыточные присваивания элементам собственного initializer удалены.
TransformNormal использует существующий typed math adapter.

Новый адаптер BoxBoundProbe поддерживает ненормированные направления и
параллельные компоненты. В отдельном SDK translation unit он сопоставлен
с настоящим D3DXBoxBoundProbe: **104 500 конечных входов, ноль расхождений**.
Есть граничные точки, нулевые компоненты и направления, лучи изнутри и снаружи.
NaN/infinity и полный набор вырожденных float-входов этим не доказаны.

В общем bootstrap выполнен оригинальный PickFull на первом/последнем кадрах
Terron, 12 осевых лучей. Проверены конечные неотрицательные расстояния hits,
сохранение output при miss и настоящий animator PickFull miss.
Это проверка выполнения/памяти и контракта, не независимое доказательство
точности всего оригинального алгоритма picking на всех моделях.

Шесть существующих reference aliases g_HInst/g_Wnd/g_Flags/g_ScreenX/g_ScreenY/
g_AvailableTexMem вынесены из init_runtime.cpp в platform_globals.cpp.
Их типы и привязка к тому же platform3g::Current() не изменились.
Это позволяет SDK-потребителю использовать настоящий owner, не вытягивая
весь NativeInitRuntime и не создавая тестовых глобальных подмен.

Force-link настоящего map owner: **23 unresolved вместо 27**; закрыты оба
метода shadow texture и Pick/PickFull, новых missing symbols нет. Оставшийся
список в evidence: реальные владельцы side/robot/building/cannon/flyer,
weapon/effect constructors и cleanup, map Trace, interface hint cleanup,
special robot creation и explosion definitions. Отрицательный map link
не является запуском карты или доказательством полноты CGame::Init.

## Проверки и пределы доказательств

- Host: общий оригинальный bootstrap, две сессии, ASan/UBSan/leak checks PASS.
- Linux ARM64/QEMU: тот же новый срез, две сессии, runtime PASS.
- devkitA64: compile/link PASS, strong UND = 0. ELF SHA-256:
  `5000c4e8ccb74ea797a4d0e12e0365e88bdb79dbcb1f470f3efdd2b20af63b31`.
- Windows MSVC + настоящий D3D9 HAL: link/runtime PASS, SDK pixels/readback,
  повторное обновление и восстановление bindings PASS.
- CMake configure/generate PASS; полной повторной сборки всего движка нет.

Для воспроизведения SDK GPU-среза добавлен отдельный Windows target
`matrixgame-native-shadow-texture-hal` в native-engine CMake.
Аргумент запуска — абсолютный путь к настоящему Terron `model-2.vo`.
Нужны настоящие D3D9 HAL и D3DX9 DLL. Exit 77 означает отсутствие доступного
HAL, 1 — ошибку, 0 — реально выполненные проверки. Окно скрыто.
В данном прогоне проверена адресная MSVC-компиляция/линковка production units;
полная сборка нового CMake target отдельно не выполнялась.

Cold host/QEMU путь честно отказывает без GPU provider на world-transform/
viewport. В циклах new/update caller-owned текстура не меняется и освобождается.
Это отдельная проверка отказа, не доказательство GPU на QEMU/Switch.

Рецепты, SHA исходников и результаты:
[evidence](matrixgame-e2e519-shadow-texture-evidence.json).
Prepared engine содержит 248 файлов; portable-core остался прежним.
HAL использовал view v2 с побайтово теми же выбранными engine sources, что v3;
в v3 и CMake сформирован полный актуальный manifest.

Текущий SDK backend загружает VB/IB заново для каждого draw. Кэширование и
производительность не оценивались. Clipper-positive, текстурированные skins,
все SDK failure/unwind ветви и настоящий Device.Reset не проверены.
Мир, полный CGame::Init, RunGameLoop, CFormMatrixGame и бой не запускались.
Switch/deko3d shadow path — **HARDWARE PENDING / backend ещё не замкнут**.
Нового NRO нет.

Frozen NRO/ELF, оба аппаратных ZIP, доступные protected inputs, сейвы/ресурсы
и аппаратные материалы сверяются отдельно в evidence. G:/pack.zip недоступен.
Push не выполнялся. Причины CPU raster, исторических manual-publish ENOENT
и зависаний основной игры остаются OPEN; этот срез их не исправляет.
