# E2E-5.19 — оригинальный projected-shadow owner

24.10.2026, продолжение `ef32123`. В существующий native-cache target включён
полный `MatrixLib/3G/ShadowProj.cpp`, включая вынесенные ранее оригинальные
SDK-dependent header bodies. Draw и texture-binding calls направлены в
существующие настоящие typed backends. Отдельного executable не добавлено.

## Доказанный дефект и исправление

Исходный `CVOShadowProj::Prepare` повторно вызывал AddSource для одного и того же
`m_preVB/m_preIB`, не снимая предыдущую регистрацию. После удаления владельцев
оставались orphan pools. Реальное выполнение original lifecycle в общем
ASan/UBSan bootstrap завершилось LeakSanitizer FAIL: **816 bytes / 13 allocations**.
Использованы два настоящих shadow owner, повторный Prepare, удаление одного,
DX_Prepare оставшегося и повторный вход bootstrap. UAF в этом прогоне не наблюдался.

Исправлен именно owner lifecycle:

- старые source-регистрации снимаются перед заменой и перед пустой проекцией;
- объединённый vertices/indices block освобождается один раз;
- пустая проекция отпускает texture ref через оригинальный DestroyTexture;
- пустые DX_Prepare/DX_Free/RenderMin безопасно завершаются;
- общий VB/IB pool сохраняется до последнего живого shadow owner и затем
  уничтожается, включая случай владельца без геометрии.

Раннее создание пулов в constructor сохранено: настоящий, ещё не включённый
`CMatrixShadowProj::ReadGeometry` напрямую использует их. Отложенное создание
из первого private candidate отклонено при анализе этого потребителя.
Constructor совпадает с original после удаления единственного increment
нового static owner counter; DestroyTexture совпадает точно. Полей экземпляра
и virtual entries не добавлено. Существующие m_VB/m_IB definitions остаются
в GameStaticResources.cpp; новый static counter определён только в ShadowProj.cpp.
Алгоритмы terrain projection и исходные игровые ресурсы не менялись.

## Проверки и их пределы

Итоговый общий bootstrap: host ASan/UBSan/leak-check и ARM64/QEMU, по два сеанса.
В каждом проверены оба порядка удаления, repeated Prepare, empty→nonempty,
совпадение всех vertex/index staging bytes, initialized ranges, повторные
DX_Free, MarkAllBuffersNoNeed/recreate, texture refs и возврат allocations к baseline.
Новых sanitizer exclusions нет; прежние ограничения общего bootstrap сохраняются.

Input — четыре настоящие позиции Terron.vo и явная четырёхиндексная strip
fixture. Это проверка ownership/upload контракта Prepare, **не** исполнение
terrain projection. Original RenderMin доходит до настоящей границы устройства
и получает Unsupported; успешного GPU draw нет. GPU texture file не загружался.

Ошибки двух промежуточных test setups сохранены отдельно: ожидание отсутствия
предыдущих VO bindings и запрещённый вложенный Context. Итоговый тест использует
существующий настоящий Context и явно снимает предыдущие bindings перед
измерением. Производственный v2 код при этом не менялся; повторно собран только
test setup, production/driver objects сверены побайтно.

devkitA64: prefix compile/link PASS, strong undefined=0; runtime NOT RUN.
ELF SHA-256: `a1befdbc80a1a194fff65be67fdba887dc11cbe4ad31e11fa0c8061e1afb8239`.
MSVC, OOM injection и Switch NOT RUN. Новой NRO нет.
CMake configure/generate проверен; полной повторной CMake-сборки не было.
Два engine views245 совпадают; изменились только ShadowProj.cpp/.hpp,
MatrixShadowManager.cpp остался прежним. Подробные команды, hashes, failed
baseline и итоговые результаты: [evidence](matrixgame-e2e519-shadow-proj-evidence.json).

## Граница исполнения

Force-link map owner: **29 unresolved вместо31**. Закрыты CVOShadowProj ctor
и DestroyTexture; новых unresolved относительно предыдущего probe нет.
Остаются CMatrixShadowProj/ShadowProjBuild*, effect/weapon owners, side/robot/
building/cannon/flyer owners, interface hint cleanup, VO Pick/PickFull и map Trace.
Полный CGame::Init, карта, RunGameLoop, CFormMatrixGame и бой NOT EXECUTED.
ReadGeometry и настоящая генерация shadow texture ещё не исполнялись.

Эта утечка относится к MatrixGame, который не входит в основную игровую NRO.
Она не объявляется причиной зависаний основной игры. Raster, исторические
manual-publish ENOENT и freezes остаются OPEN. Frozen main NRO/ELF, оба аппаратных
ZIP, доступные protected inputs и107 аппаратных файлов сверены; G:/pack.zip
недоступен. Сейвы/ресурсы и настройки Switch не менялись. Push не выполнялся.
