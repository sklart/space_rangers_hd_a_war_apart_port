# EC_File: граница следующего этапа

Анализ выполнен по `SpaceRangersHD_decomp` `build-2025-10-13` @ `730bdf6` и
соответствующему C++ upstream. Этот документ не подключает `EC_File` к NRO.

| EC_File операция | Package operation | Статус 3.1 |
| --- | --- | --- |
| Open read | поиск во collection и `OpenEntryByPath` | частично: один package поддержан |
| Close | `CloseEntry` | поддержано |
| Read | `ReadEntry` | поддержано |
| Seek | seek по logical handle | требуется compatibility origin adapter |
| GetPosition | position handle | поддержано |
| GetSize | entry data size | поддержано |
| Write | package write path | намеренно отсутствует: read-only milestone |
| Loose file | Win32/portable filesystem file | отсутствует, отдельная задача |
| Several packages | `OpenEntryByPathAcrossPackages` | отсутствует, отдельная задача |

`EC_File` использует package collection для чтения, а loose files — отдельно.
Milestone 4 должен добавить collection routing и adapter origin `BEGIN/CURRENT/END`
без Win32 HANDLE emulation. Read-only package backend не должен становиться
универсальным write backend.
