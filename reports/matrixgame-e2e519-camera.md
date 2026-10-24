# E2E-5.19: полная оригинальная камера вместо lifecycle subset

24.10.2026. Родитель `33e24f5`. Локальный master, без push.

В native-cache прежняя выборка пяти методов MatrixCameraLifecycle.cpp заменена
полным оригинальным MatrixCamera.cpp: включая SAutoFlyData::Stat/AddWarPair/Takt/
FindAutoFlyTarget и CMatrixCamera::BeforeDraw/Takt/RestoreCameraParams. Камера
больше не представлена только конструктором и освобождением ресурсов.

Никаких решений автоматической камеры, времени жизни боевых пар, интерполяции,
выбора цели и геометрии frustum не менялось. RotationX/RotationY/Translation
направлены на существующие DirectXMath adapters; viewport и view/projection —
на существующие offscreen/map-device adapters с исходной проверкой результата.
Класс, заголовки и ABI не изменялись. Полный TU после обращения этих конкретных
адаптаций побайтово совпал с предыдущим prepared original; проверены также
все engine headers. Это проверка содержимого, не allowlist имён файлов.

## Достигнутая интеграция

Map-owner link probe: **20 → 17 unresolved**, закрыты ровно:

- CMatrixCamera::RestoreCameraParams();
- SAutoFlyData::AddWarPair(CMatrixMapStatic*, CMatrixMapStatic*);
- SAutoFlyData::Stat().

Новых неразрешённых связей нет. Оставшиеся зависимости относятся к
CIFaceList/CInterface/CIFaceMenu и CMatrixCursor::SetVisible. Дублирующих
определений между полным TU и прежней выборкой не оставлено. Host/ARM64 archive
явно удаляет MatrixCameraLifecycle.cpp.o перед добавлением полного TU;
production CMake компилирует только полный MatrixCamera.cpp.

## Адресная проверка владения

В общем bootstrap, рядом с настоящей InitializeMapCamera, добавлен один тест
SAutoFlyData::Release. Он создаёт два настоящих SObjectCore через оригинальный
Create(nullptr), удерживает по три дополнительные ссылки из двух боевых пар и
текущей пары, затем проверяет освобождение всех дополнительных ссылок,
обнуление current pair и безопасный повторный Release. Оставшиеся собственные
ссылки освобождаются оригинальным Release. nullptr здесь — настоящий detached
core; объектов-имитаций и поддельной карты нет.

Host ASan/UBSan с LeakSanitizer и ARM64/QEMU: **PASS**, этот случай выполнен в
двух общих сессиях с cleanup/reentry. Уже существующие проверки реального конструктора
камеры, projection, отказа от неверной конфигурации и повторного входа сохранены.
Новых sanitizer exclusions нет. Это проверка владения, не выполнение AddWarPair
на живых игровых объектах.

devkitA64 compile/link: **PASS**, отдельный bootstrap ELF. Первый full-TU build обнаружил
отсутствующий D3DXMatrixTranslation; использован уже существующий адаптер.
Повторная сборка в том же каталоге переиспользует завершённые зависимости.
После изменения теста перекомпилирован только тест и повторена линковка.
Первый такой relink был запущен без правильного DEVKITPRO и не нашёл switch.ld;
с явными DEVKITPRO/DEVKITA64 линковка успешна. Новая основная NRO не создавалась.

Команды, коды завершения, финальный ELF hash, проверка strong undefined,
source fidelity и protected-input hashes находятся в evidence JSON.

## Граница исполнения и оставшиеся дефекты

Полные игровые BeforeDraw/Takt/FindAutoFlyTarget/AddWarPair/RestoreCameraParams
камеры пока **NOT EXECUTED**: нужна настоящая карта и графический backend.
Проверка линковки не является проверкой выбора боевой цели или корректности
изображения. Исходный NEG_FLOAT и другие неисполненные пути не объявлены
проверенными этим этапом.

**CGame::Init NOT CALLED, карта NOT CONSTRUCTED, RunGameLoop и бой NOT RUN.**
Switch/deko3d — **HARDWARE PENDING**. Прямоугольный Clear, выявленный на предыдущем
этапе, всё ещё не поддерживается offscreen backend.

Основные P0 остаются OPEN: источник повторяющегося текста, точный syscall/пути
исторических manual-publish ENOENT и причины полного зависания карты/выхода.
Основная NRO/ELF, hardware ZIP, пользовательские сейвы и ресурсы не менялись:
проверены хэши 13 доступных protected inputs, frozen NRO/ELF и 107 аппаратных файлов.
G:/pack.zip недоступен для повторной проверки. Push не выполнялся.

[Доказательства и точные оставшиеся зависимости](matrixgame-e2e519-camera-evidence.json).
