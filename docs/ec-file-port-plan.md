# EC_File: Milestone 4 portable backend plan

Анализ выполнен по `SpaceRangersHD_decomp` `build-2025-10-13` @ `730bdf6` и
соответствующему C++ upstream. `EC_File.cpp` остаётся исходным translated unit;
Win32-зависимый backend расположен в `EC_HsFile.cpp`. Для Switch требуется
узкий adapter `TPackCollectionEC` поверх `srhd_awa::package::Package`, а не
эмуляция HANDLE.

| EC_File операция | EC_HsFile/package operation | Win32 dependency | Portable equivalent | status |
| --- | --- | --- | --- | --- |
| constructor/destructor/reset | `CloseEntryHandle` and logical lifetime | none in `EC_File.cpp` | unchanged source and adapter close | RELEASE_EQUIVALENT |
| open/read acquire | `OpenEntryByPathAcrossPackages` | `CreateFileA` in original backend | package lookup, 16 slots/package | PORTABILITY_DELTA |
| close/release | `CloseEntryHandle` | package slot/loose HANDLE | logical slot close | PORTABILITY_DELTA |
| read | `ReadEntryHandle` | loose `ReadFile` | `Package::ReadEntry` | PORTABILITY_DELTA |
| write | `WriteEntryHandle` | `WriteFile` | unsupported for package entries | PORTABILITY_DELTA, deferred |
| seek/position/size | `Seek/GetEntryHandlePosition/GetEntryHandleSize` | loose `SetFilePointer` | logical 32-bit-safe slot | PORTABILITY_DELTA |
| missing path | `TryAcquireReadHandle` | collection lookup | false / translated exception flow | RELEASE_EQUIVALENT |
| loose file | `UseLooseFiles`, `LooseFileRoot` | `CreateFileA`, `FileExists` | deferred until startup requires it | UNKNOWN |
| collection precedence | first matching package | none | first package wins | RELEASE_EQUIVALENT |
| locking | `PackageFileLock` | original critical section | existing non-Windows `std::recursive_mutex` | PORTABILITY_DELTA |

`EC_File` использует package collection для чтения, а loose files — отдельно.
Milestone 4 должен добавить collection routing и adapter origin `BEGIN/CURRENT/END`
без Win32 HANDLE emulation. Сохраняются nested `OpenDepth` и exception flow
upstream `EC_File`; `ReadWideString` остаётся скомпилированным вместе с unit.
Read-only package backend не должен становиться универсальным write backend.

`platform/msys-runtime-text.patch` применяется только на MSYS к host test
compile и сразу откатывается. Он заменяет недоступный MSYS `ftruncate/fileno`
на `std::filesystem::resize_file` в неиспользуемой текстовой append-ветке
runtime; Switch и Linux CI компилируют неизменённый upstream.
