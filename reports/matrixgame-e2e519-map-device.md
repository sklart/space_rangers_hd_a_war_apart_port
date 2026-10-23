# E2E-5.19 — полная MatrixMap.cpp и граница настоящего конструктора

23.10.2026, локальный master, без push. Продолжение `d9f7296`.
Полная исходная MatrixMap.cpp теперь компилируется на host, ARM64 и devkitA64
и входит в общий matrixgame-native-cache. Это **не запуск карты/CGame::Init**.

Сохранены тела игровых методов, обходы групп, stencil/fog branches и порядок
вызовов. Device calls направлены через существующие typed sampler/render/staging
контракты. g_Sampler остаётся исходным CMatrixSampleStateManager с его политикой
анизотропии. LOD/fog float payload передаётся через bit_cast<uint32_t>, а не
числовое преобразование или нарушающий aliasing указатель. Подключён полный уже
подготовленный ShadowProj.hpp; новые fake shadow методы не вводились.

В map_device_backend добавлены view/projection/world transforms, caps и два
DrawPrimitive-вызова. На Windows SDK-вызовы доступны только через настоящий
конкретный D3D9Device и его ReadLegacyHandles; ненастоящие SDK identities не
принимаются. На host/Switch без нужного backend — явный Unsupported, а не
успешное рисование. Существующий whitelist render states сохраняется:
новые stencil/fog состояния ещё не объявлены поддержанными backend-ом.

Windows Manual.exe launch вынесен из исходного HelpRequestHandler в provider,
при сохранении проверки существования файла и выхода из диалога. HANDLEs теперь
закрываются и при исключении Show. Manual.exe ни одним тестом не запускался.

Первый Windows compile выявил конфликт native D3DMATRIX alias с SDK. Исправлена
именно граница: backend header не включает math_types.hpp, принимает row-major
WorldMatrix, а template копирует только trivially-copyable матрицу того же размера.
Все числовые register/value/transform IDs проверены C++ static_assert против
реальных Windows SDK headers. Первые failed logs сохранены.

Windows state lease теперь сохраняет и восстанавливает view/projection наряду
с прежними world/texture states. Изменён существующий реальный NULLREF test:
в двух циклах матрицы специально меняются, после Restore сравниваются побайтово
с исходными. Runtime PASS, device/window заимствованы и остаются живыми.
Это проверка SDK state ownership, **без draw, CGame или Switch**. Windows map
backend отдельно COMPILE PASS, его actual runtime пока NOT RUN.

Общий bootstrap host ASan/UBSan/leaks и ARM64/QEMU завершился0 в двух сессиях.
Новые map boundaries на ненастроенном device явно отказывают и сохраняют
CPU/game-default owners. Полная Map TU скомпилирована и помещена в archive;
неиспользуемые методы отбрасываются линкером. Strong undefined=0 у SDK bootstrap
ELF не означает закрытие зависимостей настоящего владельца карты.

Это проверено отдельно: удержание настоящих CMatrixMap ctor/dtor через linker
--undefined даёт LINK FAIL. Реальные оставшиеся ссылки:

- DevConsole.cpp: CDevConsole ctor/dtor;
- MatrixDebugInfo.cpp: CMatrixDebugInfo ctor/dtor; исходный ctor вызывает D3DXCreateFontW;
- MatrixTransition.cpp: CTransition::Clear в dtor/unwind;
- MatrixSoundManager.cpp: CSound::Init, исходная инициализация sound bookkeeping;
- RoadNetwork: уже перенесён, но не подключён в прежнем ручном bootstrap link recipe.

CMake теперь распространяет существующий matrixgame-native-navigation как PUBLIC
зависимость cache. RoadNetwork suites не повторялись; его отсутствие в старом
ручном forced-link log не объявляется новым дефектом алгоритма. Замыкание
конструктора с navigation и остальными original owners остаётся следующим шагом.
В прежнем комментарии CMinimap был назван преждевременно: текущий log этого не
показывает. Карта не конструировалась на фиктивной памяти или null this.

Fresh production prepare выполнен отдельно: все240 source hashes совпали с
проверенным Map source view. Fresh CMake configure/generate PASS; полной сборки
этого нового каталога не было. Адресно пересобраны четыре TU на архитектуру:
input, game-check, полная MatrixMap и map backend; принятые зависимости сохранены.
Для полного Map object, как ранее для Logic, исключён UBSan vptr: не создаётся
поддельная RTTI неперенесённых классов. Address/остальные UBSan остаются включены;
полные методы карты при этом не исполнялись.

Hashes/команды/логи и текущие границы: matrixgame-e2e519-map-device-evidence.json.
Сборка до коммита, build_dirty=true. Новая NRO не создавалась, frozen main NRO/ELF,
оба аппаратных ZIP, пользовательские saves/resources и107 аппаратных входов
сохранены. Причины intro-растра, двух ENOENT и полного freeze карты остаются OPEN.
