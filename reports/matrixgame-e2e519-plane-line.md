# E2E-5.19 — D3DXPlaneIntersectLine и следующая граница карты

23.10.2026, master, без push. Продолжение `c3141f7`: снята математическая
зависимость полной MatrixMap.cpp в исходных UnitPickGrid/UnitPickWorld.
Тела этих методов не менялись; сами методы и полная карта ещё NOT EXECUTED.

Добавлен D3DXPlaneIntersectLine в существующий native-core/math_types.hpp.
Это пересечение бесконечной прямой с плоскостью; результат вне отрезка допустим.
Нулевой dot(normal,end-start) возвращает nullptr, не меняя выход. Почти
параллельная прямая с ненулевым dot принимается. Локальные значения позволяют
записать результат поверх любого входного конца.

[Контракт Microsoft](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dxplaneintersectline)
описывает nullptr при параллельности. Детали почти параллельного случая,
неизменности выхода и aliasing измерены непосредственно на установленной
`C:/Windows/SysWOW64/d3dx9_43.dll`, SHA256 записан в evidence.
XMPlaneIntersectLine из уже поставленной DirectXMath здесь не обёрнут:
его near-equal epsilon отвергает ненулевые наклоны, которые принимает D3DX9.
Например, исходная DLL принимает z-направление1e-10 и возвращает x=1e10.

В `tests/probe_matrixgame_plane_line_win32.cpp` тот же native adapter сравнивается
с GetProcAddress настоящей x86 DLL. Проверены основные и128 детерминированных
наклонных случаев, в каждом — отдельный выход и оба aliases. Указатели/null
совпадают; максимальная scaled численная ошибка2.14225884e-6 при допуске2e-5.
Это численная совместимость выбранных конечных данных, **не побитовая
тождественность всех float**, не проверка NaN/Inf/denormal.

В общем bootstrap после настоящих game defaults выполняются адресные проверки
этого контракта. Отдельный фиктивный game/device prototype не создаётся.

| Gate | Фактический результат |
|---|---|
| MSVC x86 + оригинальная D3DX9 DLL | Runtime comparison PASS |
| Host ASan/UBSan/leak | Общий bootstrap, два цикла: PASS |
| ARM64/QEMU | Тот же общий bootstrap, два цикла: PASS |
| devkitA64 | Отдельный ELF COMPILE/LINK PASS, strong undefined=0; runtime NOT RUN |
| Полная MatrixMap.cpp | COMPILE FAIL; обе ошибки PlaneIntersectLine сняты |
| Карта, CGame::Init, CFormMatrixGame, GPU/бой | NOT EXECUTED / HARDWARE_PENDING |

Следующая первая ошибка полной TU теперь `MatrixMap.cpp:1500`,
`CMatrixMap::DrawLandscapeSurfaces`: g_D3DD->SetSamplerState на incomplete
IDirect3DDevice9, вместе с D3DSAMP_MIPMAPLODBIAS и LPDWORD. Далее отсутствуют
sampler/filter и fixed-function render state API. Их нельзя закрывать
безусловным success или вырезанием исходного draw. Требуется интеграция
настоящего device/backend контракта.

Host/ARM/SDK пересобраны только input.cpp и изменённая game-check TU, остальные
принятые Logic archives/driver/dependencies переиспользованы. input.cpp повторно
скомпилирован для отдельного архива, его исходник не менялся. Новая функция
добавлена в math header, прежние функции/ABI не менялись. В ранее принятом
Logic object остаётся оговорённое исключение UBSan vptr. Full engine rebuild
и повторные standalone CList/BigVB/IB/RoadNetwork suites не выполнялись.

Сводка source/binary/log hashes: `matrixgame-e2e519-plane-line-evidence.json`.
Source view240 файлов прежний; новый math_types.hpp — внешний portable input,
его hash фиксируется отдельно. Сборка до коммита, build_dirty=true, embedded
Git SHA не заявляется. NRO не создавалась, frozen main/ELF/ZIP и saves/assets
не менялись. Причины аппаратного текста, двух ENOENT и freeze карты всё ещё OPEN.
