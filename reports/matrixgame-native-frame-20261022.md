# MatrixGame: кэш → оригинальный VO → deko draw, 22.10.2026

Продолжение после принятого E2E-5.18, на локальном `master`, без физической Switch.
Основная NRO и оба замороженных аппаратных комплекта не изменялись.

## Новая граница исполнения

Добавлена связка настоящего кэша и CPU-среза оригинального `CVectorObject` с
существующим GPU renderer. Это **не полный `CGame::Init` и не планетарный бой**.

На host и ARM64/QEMU исполняется:

`NativeInitRuntime::Begin → Cache::Get(VO) → CVectorObject::Prepare/LoadSpecial(OLF_NO_TEX)
→ PrepareCachedFrame → PrepareNativeFrame → DX_Prepare → BigVB/BigIB Select
→ проверка фактически привязанных буферов → освобождение → повторный вход`.

`PrepareNativeFrame` — новый явно названный bridge в генерируемом CPU-срезе,
а не переименование или подмена исходного `Draw`. Из pinned `VectorObject.cpp`
берутся исходные вызовы `Select` и surface-first обход групп текущего кадра.
`VectorObjectRange` сохраняет обе исходные ветви `DrawIndexedPrimitive`, включая
отрицательный `m_IBase` и смещения обоих общих буферов. Перед выбором объекта
сбрасывается кэш выбора BigVB/BigIB, как в начале исходной отрисовки.
Вызовы `DX_Prepare` и `m_RemindCore.Use` соответствуют исходному `BeforeDraw`.
Сам `BeforeDraw` с preload callbacks и полный `Draw` здесь не определяются.

На devkitA64 **скомпилирован и слинкован, но не исполнен** дальнейший путь:

`NativeDekoSession::Begin → настоящий DekoMeshRenderer/device/queue
→ LoadObject/LoadTexture через кэш → PrepareCachedFrame → SetTextureAsset
→ DekoBufferRenderer::Draw → DrawFrame/indexed GPU draw/readback → Deinit`.

Сессия владеет offscreen deko-устройством. Она не притворяется заимствованным
`IDirect3DDevice9`, не создаёт вымышленных caps/VRAM и не устанавливает WndProc.
После завершения GPU queue уничтожаются renderer и GPU-ресурсы, затем VO,
текстуры кэша и CPU staging. Исключение при создании renderer откатывает начатый
CPU lifecycle. Повторный `Begin` активной сессии отвергается до изменений.
Эти GPU-ветви rollback/reentry пока подтверждены только компиляцией и просмотром
порядка владения; их аппаратный runtime остаётся открытым.

Материал каждого surface задаётся явно по существующему single-pass native
shader contract. Отсутствующий материал, нулевой/неподдерживаемый texture slot,
конфликт разных текстур в одном слоте и истёкший владелец не заменяются белой
текстурой. CPU payload удерживается shared lease до окончания загрузки и draw.
Поддержка всех эффектов исходного `CSkinManager` этим **не заявляется**.

## Адресные проверки

- **Host ASan/UBSan runtime PASS:** настоящий `wood05.vo`, две отдельные копии
  ресурса в одном кэше, явная контролируемая PNG, два lifecycle-цикла.
  Первый объект: 103 вершины, 480 индексов, одна группа рисования.
  Проверены неправильный номер кадра, отсутствие материалов, недопустимый slot,
  переключение объектов с различными base offsets, возвращение к первому объекту,
  освобождение/пересоздание буферов и сохранность payload lease после Deinit.
- **ARM64/QEMU runtime PASS:** тот же новый интеграционный сценарий.
- **devkitA64 compile/link PASS:** ELF содержит фактические deko device creation,
  cache load, native frame preparation, texture upload и indexed draw. Strong
  undefined symbols: 0. Его `main` запускает CPU-контракт; GPU entry сохранён
  через ссылку `MatrixGameNativeSessionDraw` и **не вызывается offline**.
- **GPU/Horizon runtime HARDWARE_PENDING.** Новый ELF не упакован в NRO и не
  предлагается как готовый аппаратный тест. Проверка изображения, видимости
  модели, GPU allocation failure и повторного создания устройства не выполнена.

Исходная VO: SHA-256
`04497dcc94ede26f183d847a4dd094d3ba05bc825d09fcfa73fa36bfae80981e`.
PNG — контролируемая fixture, **не текстура из игры**, SHA-256
`d81cd93a0fccbc517719f9cb6563f6837327dc32a412f59238a95699831ba210`.
Это один новый сценарий интеграции, а не повтор предыдущих standalone suites.
Полный compile inventory E2E-5.18 (14/92) повторно не прогонялся.

Команды и хэши окончательных бинарников/effective sources находятся в
[машинном свидетельстве](matrixgame-native-frame-20261022-evidence.json).
Логи: `build/matrixgame/native-frame/`; старые результаты E2E-5.18 сохранены.
После просмотра diff повторно собраны только затронутые targets: последняя
правка заголовка добавляла комментарий о сроке жизни borrowed VO и bindings.

Коммит кода: `9d5883e32d030f28a51f8ea8f2176dd2ce02da92`. Сборки выполнялись
до коммита (`build_dirty=true`); нормализованные исходники сверены с его Git blobs.
Все генерируемые effective sources трёх сборок совпали по SHA-256. Deko-сессия
компилируется только SDK target; host/QEMU подтверждают подготовку кадра,
но не заменяют исполнение этого GPU-кода. Последующий коммит меняет документацию.

Окончательный SDK ELF: `build/matrixgame/native-frame/sdk/cmake/matrixgame-native-frame-probe.elf`,
SHA-256 `820fcc38400b3b68253b557509a5b72a25d235c1848e420b8073604110d5da8d`,
Build ID `91c2e8a4a16cf54780d9f7f029c484f137491c94`.
Воспроизводимый SDK build с новым output:

```powershell
python tools/matrixgame_init_sdk.py --output build/matrixgame/frame-sdk-new --cached-frame
```

Для host использовать CMake source `port/matrixgame/native-engine`,
`RelWithDebInfo`, `CMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer`;
собирать только `matrixgame-native-frame-probe`. Запускать
`tests/probe_matrixgame_cached_frame.py --binary <binary> --model <original.vo>
--texture <plain.png> --output <fresh-build-path>`. Для ARM64 добавить `--qemu`;
точные компилятор, библиотеки и include paths сохранены в evidence JSON.

## Что пока блокирует CGame

1. `upstream/matrixgame/MatrixLib/3G/VectorObject.hpp` и полный
   `VectorObject.cpp`: зависимость от D3D9, skin callbacks, тени и полная анимация.
   CPU header остаётся отдельным; оригинальный GPU header им не маскируется.
2. `upstream/matrixgame/MatrixGame/src/MatrixSkinManager.cpp`: старые
   `CTextureManaged*`/`Tex()`, reflection texture, burn/object state, многопроходные
   combiners. Новый `MaterialTexture` нельзя передавать этим cast-ам. Автоматическая
   загрузка skin по имени из VO ещё не подключена.
3. `upstream/matrixgame/MatrixLib/3G/3g.cpp::L3GInitAsDLL` и
   `MatrixGame/src/MatrixGame.cpp::CGame::Init`: исходный borrowed-device/window
   contract, caps/gamma и последующее применение video/render state. Собственная
   offscreen deko-сессия ещё не интегрирована вместо этого platform entry.
   `CFormMatrixGame` и `RunGameLoop` не запускались; результат боя не создаётся.

## Следующий аппаратный доступ

Использовать неизменённый [вечерний сценарий](../docs/e2e517-evening-switch-check.md):
основную E2E-5.16 — для переходов экранов/задержек карты и сохранения после перелёта
на копии данных; отдельную замороженную GPU NRO — для её существующего smoke test.
Новые CPU/GPU связи этого отчёта в тех NRO отсутствуют. Для нового GPU entry ещё
нужны отдельный запускаемый harness, подходящие преобразования модели и проверка
readback; один SDK ELF не закрывает эти аппаратные действия.

Сверены до/после 13 защищённых файлов: все пользовательские saves/config/logs,
посторонний provenance JSON, основной и GPU ZIP, `G:/pack.zip`, VO и PNG.
Хэши совпали. Исходные игровые ресурсы и gitlink MatrixGame не изменены. Push нет.
