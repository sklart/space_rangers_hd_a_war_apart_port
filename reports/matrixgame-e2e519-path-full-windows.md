# E2E-5.19: Path и полная Windows-линковка

25.10.2026. Локальный кандидат, без commit/push. E2E-5.19 **PARTIAL**.

Продолжение этого среза: [оригинальные материалы, завершённый PrepareMap и
текущая граница ApplyVideoParams](matrixgame-e2e519-map-loaded.md).
Ниже сохранён результат первоначального Path/линковочного прогона; его
`unowned skin provider` больше не является текущим блокером.
Заключительный аудит повторил SDK Path compile/link с `-UNDEBUG` после
`-DNDEBUG`; assertions теперь включены. Итоговый SDK result и хэши сохранены
в evidence продолжения, старый результат ниже оставлен как исторический.

`CMatrixEffectPath::UpdateData` сохраняет индексы обоих концов и всех next/prev
до `HAllocEx`. Ноль означает nullptr, первый элемент кодируется единицей.
После успешного allocation восстанавливаются указатели; смещения не вычисляются
по освобождённым указателям. `m_DotsMax` публикуется после успешного allocation.
При отказе прежний блок и связи остаются у владельца. `AddPos` в целом не является
транзакцией: добавленная точка остаётся в CBuf, последующая попытка проверена.

| Проверка кандидата | Реальный результат |
| --- | --- |
| Host ASan/UBSan, leak-check, halt_on_error | PASS |
| ARM64/QEMU | PASS |
| devkitA64/libnx | полный Path TU + test driver compile/link PASS, strong undefined = 0; runtime NOT RUN |
| Windows MSVC x86 | адресный Path runtime PASS, assertions включены |

Один и тот же тест выполняет CreatePath, GetPos с независимыми скалярными
координатами, BeforeDraw, Takt, AddPos, Release и повторный вход. Проверены пустой
и непустой списки, nullptr на концах, двусторонняя связность, принудительно
перемещаемый allocation, отказ allocation для обоих списков и повторная попытка.
За два цикла: шесть принудительных перемещений и четыре отказа. Draw/GPU Path
не запускались. Исторический baseline UAF не прогонялся заново.

Оригинальные Path, `L3GRun`, `CMatrixMap::CalcVis`, `CalcMapGroupVisibility`
подключены к Windows-кандидату. Полная линковка original Init и отдельного
Windows game driver успешна; заглушек для этих методов нет. Цикл получен из
исходного 3g.cpp, сохранены message pump, time delta, reminders, Takt и Draw.
Запрос texture memory направлен в реальный Device provider. Полный visibility TU
адаптирован через типизированные device operations, алгоритмы сохранены.
Сами цикл и расчёты видимости в этом прогоне **NOT RUN**. `CalcVis` может писать
в файл карты, поэтому на исходных ресурсах не запускался.

Отдельная CMake-цель `matrixgame-windows-game` не смешивается с
`matrixgame-native-bootstrap`. Связанные Windows EXE получены адресной сборкой
новых объектов с ранее собранными библиотеками; повторной полной CMake-сборки
движка не было. Все объекты используют совместимый /MD; старый /MT managed
object заменён адресно собранным /MD объектом оригинального модуля.
CMake configure/generate PASS; добавлен отдельный адресный Path test target.

На Windows реально успешно выполнен `Direct3DCreate9 → CreateDevice(HAL)`.
Полный `CGame::Init` с настоящими robots.pkg/CFG прошёл statics, heap/cache,
L3GInitAsDLL с borrowed providers, renderer constructor, config ReadParams/
ApplySettings, gamma, sampler, camera, создание map logic/interface и вошёл
в `RobotPreload`. Первоначальный отказ gamma provider исправлен: оригинальный
ApplyGammaRamp безусловен, поэтому перед первой записью сохраняется настоящая
ramp даже без Init-time CANCALIBRATEGAMMA capture. Два входа дошли до одного
блокера; после каждого gamma readback совпадает с исходным, state/timer leases
пусты, map/heap освобождены оригинальным Deinit.

Первый текущий блокер подтверждён runtime-стеком и linker MAP:

`CGame::Init → CMatrixMap::RobotPreload → CVectorObject::LoadSpecial`

Сообщение: `UNSUPPORTED: unowned skin provider`. RobotPreload вызывает
PrepareSpecial(OLF_MULTIMATERIAL_ONLY, CSkinManager::GetSkin, GSP_SIDE).
Текущий native VO loader разрешает только AcquireOwnedSkin с активным
SkinLoadScope и хранит shared owners в m_NativeSkinOwners. Оригинальный
CSkinManager callback с этим контрактом пока не интегрирован. Отключение guard
или подстановка ordinal single-pass материала не является исправлением.

До первой карты требуется согласовать владение оригинальных SSkin и lifetime
CSkinManager с VO loader, затем исполнить оставшийся RobotPreload, stor.Load и
PrepareMap. Новые последующие блокеры пока неизвестны. Успешный возврат Init,
загрузка карты, RunGameLoop, полноценный GPU draw и бой **NOT RUN**.

P0 основной игры остаётся открыт. В [frozen-аудите](e2e5/hardware-20261023-cache-frozen-audit.md)
подтверждено наличие дефекта общего GI-кэша в точных аппаратных исходниках;
его причинная связь с повторяющимся текстом не установлена. Исторические
manual-publish ENOENT и Switch hangs не закрыты. Новых аппаратных запусков нет.
Повторно сверены 15 доступных защищённых файлов, все 107 файлов аппаратных
входов, исходный robots.pkg и CFG: изменений нет. Большие прежние наборы
runtime-тестов не повторялись.

[Машинные результаты, команды и хэши](matrixgame-e2e519-path-full-windows-evidence.json).
