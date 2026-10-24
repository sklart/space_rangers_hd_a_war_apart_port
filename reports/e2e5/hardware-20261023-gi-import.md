# Аппаратный дефект фона: проверка оригинального импортного wrapper

25.10.2026. Причина дефекта остаётся OPEN; исправление в NRO не вносилось.

Предыдущий тест DrawToGraphBuf использовал настоящий ResolveOkgfImport, но
обходил GR_Main::OKGR_Copy_XY_XY_WORD. Новый адресный host-кандидат выполняет
оригинальные Ex_OKGR_Copy_XY_XY_WORD и OKGR_Copy_XY_XY_WORD, включая статический
pas::win::load_import и полный production ResolveImport. Реализация OKGF настоящая.

На Switch статический импорт использует ResolveImport напрямую: ветвь runtime
windows.hpp под __SWITCH__ совпадает здесь с E2E_HOST_RESOLVER_TEST. Это не путь
LoadLibrary/GetProcAddress. Тест подтвердил один статический resolved import,
ноль unmapped, physical DLL loads и dynamic module loads. Предположение о
непроверенном DLL loader в этой конкретной цепочке не подтверждается исходниками.

На настоящем Bm.FormRuins.2RCbg выполнены исходные загрузка, декодирование,
масштабирование, перепаковка в RGB565 и DrawToGraphBuf. Полное копирование,
сдвиг (+8,+3), отрицательное смещение (-7,-5) с внутренним clip проверены по
каждому байту, включая нетронутый padding. Результат 1843200 байт,
FNV64 c7abbe2c80d5e0f4 — как в предыдущем тесте.

Первая команда теста унаследовала __SWITCH__ для host platform services и не
скомпилировалась без libnx. Исправлены только параметры теста; конечный кандидат
использует реальные host services. Повторно используемые GI objects сохраняют
ранее скомпилированные software/Switch ветви. Это не новый sanitizer-прогон,
не ARM64 ABI-проверка и не полная GUI-сессия. Содержимое GI allocation на Switch
этот результат не устанавливает. Виновный владелец и время повреждения растра
по-прежнему неизвестны.

Команда: `python3 tools/e2e523_ruins_import.py` в WSL.
[Данные кандидата](hardware-20261023-gi-import.json) содержат команду сборки,
хэши fixture, исходников, повторно используемых объектов и executable.
Пользовательские ресурсы и сейвы не изменены. Основная NRO и frozen ZIP не
пересобирались; push не выполнялся.
