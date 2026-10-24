# E2E-5.19: оригинальный GetPixelColor и настоящий region LockRect

24.10.2026. Родитель `1ba2f9c`. Локальный master, без push.

Подключён оригинальный CTextureManaged::GetPixelColor из Texture.cpp. Добавлен
typed LockRegion в legacy_texture_backend: на Windows он преобразует четыре
координаты в настоящий RECT и вызывает IDirect3DTexture9::LockRect; на backend
без SDK явно отказывает через Unsupported. Полный уровень текстуры вместо
запрошенного региона не блокируется и цвет не вычисляется искусственно.

Алгоритм исходного метода сохранён: тот же прямоугольник одного пикселя,
границы, D3DLOCK_READONLY=16, чтение первого пикселя возвращённой области и
UnlockRect. Метод после обращения явных адаптаций совпал с оригиналом;
остальные методы OriginalManagedTexture.cpp не изменились. Классы и ABI
не менялись. Это расширение selected managed methods, не весь Texture.cpp.

## Реальное Windows HAL-выполнение

Новый SDK-free тест `tests/matrixgame_bootstrap_pixel_color.cpp` вызывает
оригинальные CacheInit/CreateManaged/CreateLock/GetPixelColor/UnlockRect и
CCache::Destroy/CacheDeinit. Использован настоящий D3D9 HAL с software vertex
processing, без NULLREF и без поддельных SDK-объектов.

Два создания ARGB8-текстуры 4×4: записаны 16 разных цветов с учётом Pitch,
прочитан каждый пиксель оригинальным методом, проверены отрицательные и верхние
границы. После отказа по границам повторное чтение работает. Последний пиксель
изменён через обычный LockRect и повторно прочитан как 0x12345678. Registry
возвращается к нулю после уничтожения текстур.

**Windows HAL compile/link/runtime PASS, exit 0.** Это доказательство чтения
реальной managed ARGB8-текстуры и lifecycle; не рендер текста, карты или боя.
Использован существующий HAL helper, его test hook перенаправлен на новый тест.
Первый standalone link не включал оригинальный DebugInfo owner, требуемый
существующим helper; добавлен оригинальный object, без заглушек. Журнал сохранён.

Новые test/managed/backend CPP скомпилированы сейчас. Для переиспользованных
Windows core libraries Texture.hpp, Cache.hpp, CHeap.hpp и CMain.hpp побайтово
совпадают с текущими; хэши библиотек и объектов зафиксированы в evidence.

## Host/ARM64 и следующая граница

- **Host ASan/UBSan/LeakSanitizer и ARM64/QEMU:** общий bootstrap PASS,
  exit 0, по две сессии. Проверены отказы GetPixelColor на отрицательных
  координатах без обращения к GPU. Linux-проверки не подтверждают SDK pixels.
- **devkitA64:** отдельный bootstrap ELF compile/link PASS. После изменения
  теста выполнена только адресная перекомпиляция теста/линковка; strong undefined
  финального ELF проверены. На Switch он не запускался, новая NRO не выпускалась.
- **Map-owner link probe:** 3 → 2 unresolved; закрыт GetPixelColor, новых нет.
  Остаются CMatrixCursor::SetVisible и Text::Render.

Host/ARM64 перекомпилируют OriginalManagedTexture, legacy backend и изменённый
bootstrap game-test; неизменённые текущие native objects переиспользованы.
Новых sanitizer exclusions нет. Полная карта и UI input/lifecycle пока не
исполнялись. Количество два относится только к данному forced link probe.

**CGame::Init NOT CALLED; карта NOT CONSTRUCTED; RunGameLoop и бой NOT RUN.**
Switch/deko3d — **HARDWARE PENDING**. Прямоугольный offscreen Clear — отдельный
оставшийся backend blocker; его не следует путать с реализованным LockRegion.

13 доступных protected inputs, основная NRO/ELF, оба hardware ZIP и 107 файлов
аппаратного прогона совпадают по SHA-256. G:/pack.zip недоступен для повторной
проверки. Пользовательские сейвы, ресурсы и настройки консоли не менялись.
Push не выполнялся.

Источники повторяющегося текста основной игры, точный syscall/пути исторических
manual-publish ENOENT и причины зависаний карты/выхода остаются **OPEN**.

[Команды, source fidelity, реальные результаты и хэши](matrixgame-e2e519-pixel-color-evidence.json).
