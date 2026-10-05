# Semantic delta

Сравнение: release `730bdf6` → FPC-source `7342a10` → C++ `57fa689`.

| Category | FPC | C++ | правило для Switch |
|---|---|---|---|
| GAMEPLAY | 2026 prerelease lineage | 2.1.2500 заявлен, commit не доказан | не переносить без differential test |
| BUGFIX | есть post-release fixes | unknown provenance | документировать и оставить release default |
| PORTABILITY | SDL, 64-bit, ARM64, Unicode, threads | нет | допустим как reference проблемы |
| ABI | адаптированы Pascal/native boundaries | Win32 DLL ABI | изолировать за interfaces |
| SERIALIZATION | явно сохраняет legacy 32-bit formats | generated layouts | хранить поля формата в fixed-width types |
| RENDERER | portable OKGF | `okgf.dll` ABI | перейти на OKGF только с image tests |
| AUDIO/VIDEO | native/SDL path, Vorbis; video adapters | DirectSound/AVI/Xvid | отдельные backends; intro может быть отключено |
| FILESYSTEM/PLATFORM | paths/SDL runtime | Windows adapters | GameRoot + Switch user data |
| UNKNOWN | gameplay delta к release не исчерпывающе классифицирован | source decomp commit не установлен | blocker до переноса game units |

Семантический diff на данном milestone намеренно ограничен доказуемыми provenance-гранями. Массовой line diff generated Delphi/Pascal/C++ не выполнялся: он не заменяет классификацию поведения и создал бы ложную точность.
