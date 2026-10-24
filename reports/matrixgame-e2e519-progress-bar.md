# E2E-5.19 — оригинальный владелец progress bars

24.10.2026, продолжение `500b7fc`. В существующий native-cache target включён
полный оригинальный `MatrixProgressBar.cpp`. Это закрывает настоящие зависимости
конструктора и деструктора карты, а не создаёт отдельный CPU executable.

Единственное изменение получения ресурса — вызов уже существующего typed
`legacy_texture::GetManaged(TEXTURE_PATH_PB)`. Он возвращает настоящий cache-owned
`CTextureManaged`; приведение другого portable texture type не используется.
Пять исходных static definitions остаются в GameStaticResources.cpp. Поля и
virtual layout не менялись. Destructor, CreateClone и KillClone совпадают с
предыдущим prepared original побайтно; constructor отличается только factory.
Render calls адаптированы к существующим настоящим typed backends.

## Исполнение и границы

Новый lifecycle test входит в общий bootstrap. В двух сеансах, внутри каждого
по два цикла, он создаёт две реальные progress bars, проверяет refs 0→2→1→0,
Modify, оба clone slots, KillClone, обратный порядок удаления и повторный вход.
После удаления cache сохраняет тот же настоящий unloaded texture object;
последующий общий CacheDeinit освобождает владельца.

В первом private runner lifecycle компилировался через production recipe с
`NDEBUG`: assertions этого файла не выполнялись. Этот результат подтверждал
исполнение и отсутствие sanitizer-ошибок, но не refcount/clone-инварианты.
Итоговый tracked test использует test recipe с `-UNDEBUG` и compile-time
`#error` при `NDEBUG`; именно повторный итоговый прогон принят как доказательство.

Host ASan/UBSan/leak-check и ARM64/QEMU: итоговые результаты и точные команды
приведены в [evidence](matrixgame-e2e519-progress-bar-evidence.json).
Новых sanitizer exclusions нет; прежние ограничения bootstrap сохраняются.
devkitA64: prefix compile/link, strong undefined=0, runtime NOT RUN.
ELF SHA-256: `f3e23a9a138c052612fa4973dbcfa8269f119d300c82edcfa0c1083d9db48f23`.

DrawAll/DrawAllClones не исполнялись, PB texture file не загружался; успешная
GPU отрисовка не заявляется. MSVC для этого среза NOT RUN. Новой NRO нет.
Полные CGame::Init, RunGameLoop, CFormMatrixGame, мир и бой NOT EXECUTED.

## Ближайшие блокеры

Force-link оригинального map owner с тем же полным progress CPP даёт **31**
неразрешённый символ вместо33; новых символов относительно stencil probe нет.
Закрыты только CMatrixProgressBar ctor/dtor. Точный оставшийся список есть в
evidence: ShadowProj/ShadowManager, effect constructors и weapon methods,
side/robot/building/cannon/flyer owners, interface hint cleanup, VO Pick/PickFull
и map Trace. Это отрицательная link-проверка, не запуск карты.

Два подготовленных engine views содержат245 файлов и совпадают. Относительно
stencil view изменён только MatrixProgressBar.cpp. CMake configure/generate
проверен через существующий GNU Make; Ninja отсутствует. Полной повторной
CMake-сборки не было. Состав inputs, команды и private outputs фиксирует evidence.

Основная NRO/ELF, оба старых аппаратных ZIP, пользовательские сейвы и исходные
ресурсы не менялись. Доступные protected inputs проверены отдельно; недоступный
G:/pack.zip не считается проверенным. Причины raster, двух исторических ENOENT
и зависаний основной игры остаются открытыми. Switch HARDWARE PENDING.
