# E2E-5.19: оригинальный пакетный renderer интерфейса CInstDraw

24.10.2026. Родитель `d791701`. Локальный master, без push.

В native-cache добавлен **полный MatrixInstantDraw.cpp**, включая обе версии
AddVerts, BeginDraw/DrawFrameBegin, ActualDraw, MarkAllBuffersNoNeed и ClearAll.
Сохранены оригинальные поиск пачки по texture/factor, накопление четырёх вершин,
порядок копирования, расчёт индексов, discard/no-overwrite, порядок bindings и
draw-вызовов. Методы GPU не вырезаны из модуля.

Изменения ограничены существующими typed device adapters, точным SDK значением
TRIANGLELIST=4, ASSERT_DX и явным std::uint8_t вместо byte. Полный effective TU
после обращения адаптаций совпадает с предыдущим оригиналом; headers и ABI
не менялись. Статики GameStaticResources.cpp не продублированы.

## Реальное выполнение в общем bootstrap

В существующий bootstrap добавлен вызов оригинального renderer, без имитации
устройства или результата draw. В каждой из двух общих сессий дважды выполнены:

1. DrawFrameBegin/BeginDraw и пустой ActualDraw без выделения GPU/staging buffers.
2. Два AddVerts с общим texture key и один AddVerts с цветовым множителем.
3. Оригинальный ActualDraw: выделение и заполнение staging VB/IB, затем
   ожидаемый Unsupported на первом binding отсутствующего настоящего device.
4. Проверка 12 вершин и 12 индексов: 312 байт при sizeof(SVert_V4_UV)=24.
5. MarkAllBuffersNoNeed возвращает число staging bytes к исходному;
   ClearAll дважды освобождает накопители; StaticInit обеспечивает повторный вход.

**Host ASan/UBSan с LeakSanitizer и ARM64/QEMU — PASS**, exit 0. Новых sanitizer
exclusions нет. В тесте выполняются настоящие методы движка и выделение памяти,
но **успешных device bindings и GPU draw нет**. No-overwrite после успешно
выведенного кадра и визуальный результат этим тестом не проверены.

devkitA64: отдельный bootstrap ELF **compile/link PASS**, strong undefined 0;
он содержит новый общий тест, но на Switch не запускался. Host/ARM64 адресно
компилируют полный новый CPP и изменённый game-test; неизменённые текущие objects
переиспользованы. Проверка сборки executable не означает линковку полной карты.

## Следующая граница

Map-owner forced link: **7 → 3 unresolved**, закрыты четыре метода CInstDraw,
новых связей не появилось. Остаются:

- CMatrixCursor::SetVisible — system/software cursor lifecycle;
- CTextureManaged::GetPixelColor — настоящий region LockRect для одного пикселя;
- Text::Render — оригинальные Font/layout и рендер текста во временную текстуру.

Число три относится к зависимости конструктора/деструктора карты, а не ко всему
оставшемуся переносу. Полные UI lifecycle/input на карте ещё NOT EXECUTED.
**CGame::Init NOT CALLED; карта NOT CONSTRUCTED; RunGameLoop и бой NOT RUN.**
Switch/deko3d — **HARDWARE PENDING**. Прямоугольный offscreen Clear из предыдущих
этапов также остаётся неподдерживаемым.

Основная NRO/ELF, оба hardware ZIP, пользовательские сейвы и ресурсы сохранены:
сверены хэши 13 доступных protected inputs и 107 аппаратных файлов.
G:/pack.zip недоступен для повторной проверки. Push не выполнялся.

Основные аппаратные причины повторяющегося текста, исторических manual-publish
ENOENT и полных зависаний карты/выхода остаются **OPEN**.

[Команды, source fidelity, runtime и контрольные суммы](matrixgame-e2e519-instant-draw-evidence.json).
