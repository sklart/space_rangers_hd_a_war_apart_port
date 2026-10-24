# E2E-5.19: замкнута link-цепочка CGame::Init

Подключён полный оригинальный MatrixGamePathUtils.cpp. PathToOutputFiles
сохраняет исходное построение `personal/SpaceRangersHD/dest` и относительный
fallback при реальном отказе Windows Shell location query. Ни временная
папка, ни фиктивный пользовательский путь не подставляются.

## Достигнутая граница

ARM64/Linux и devkitA64: принудительное включение настоящего символа
`CGame::Init` теперь **LINK PASS, отсутствующих связей нет**. Новый devkitA64
ELF содержит MapPrepare, CTransition и output-path helper вместе с текущим
test driver. Strong undefined отсутствуют. Это первый полный link данного
пути; **CGame::Init не выполнялся, карта и бой не запущены**.

Host ASan/UBSan/LSan: compile/link и обычный bootstrap runtime PASS. Вызван
именно оригинальный PathToOutputFiles, который на Linux/ARM64 отклоняет
отсутствующее native mapping. Повторный вход не получает вымышленный каталог.
Отдельный полный host Init link также PASS по финальному result.json.

Windows MSVC: оригинальный TU и backend compile/link PASS. Реальные
SHGetSpecialFolderLocation/SHGetPathFromIDListW, владение PIDL и повторный вход
прошли четыре раза. Создание нового и отказ для уже существующего каталога
проверены только в `build/matrixgame/e2e519/output-paths-msvc/created-directory-0`.
Полная функция с созданием Documents/SpaceRangersHD **не вызывалась**:
пользовательская папка и данные игры не изменялись.

## Семантика и владение

ITEMIDLIST теперь принадлежит FolderItem; CoTaskMemFree вызывается при выходе
из области, включая исключения. У исходного метода освобождения PIDL не было.
Повторный запрос в уже занятый owner отклоняется до перезаписи указателя.
Настоящий CreateDirectoryW сохранён отдельной SDK-операцией; его результат
оригинальный алгоритм, как и прежде, не проверяет.

При ошибке SHGetPathFromIDListW adapter явно отказывает вместо использования
неинициализированного wchar buffer. Размер260 сверяется с MAX_PATH Windows
SDK. На Linux/ARM64/Switch personal-folder mapping пока **не реализовано**,
и эта ветвь бросает Unsupported. Успешной работы файлового вывода MatrixGame
на Switch этот этап не доказывает.

## Следующий настоящий исполнительный блокер

В начале оригинального MatrixGame.cpp CGame::Init повторяет static_init и
присваивает `g_MatrixHeap = HNew(NULL) CHeap`. NativeInitRuntime::Begin уже
выполнил статическую инициализацию и владеет своей кучей через matrix_heap_.
Прямой вызов поверх живой integrated сессии перезапишет опубликованный
указатель без согласования владельцев. Оригинальный CGame::Deinit удаляет
свою кучу через HDelete. Перед вызовом Init требуется согласовать создание,
передачу владения, failure unwind и повторный вход. Guard-only отказ не
считается запуском игры; оригинальную логику и её Deinit нужно сохранить.

Адресные проверки переиспользовали native objects предыдущих этапов;
изменённые TU и test driver пересобраны. ELF построен из текущих исходников
до локального коммита, поэтому его provenance — parent f28f7fc плюс хэши
изменённых файлов и effective manifest, а не ложный build_git будущего коммита.
Новой NRO нет. Команды, symbol root, ELF SHA256 и границы:
[evidence](matrixgame-e2e519-output-paths-evidence.json).

Основная NRO, оба frozen ZIP, saves/assets и107 файлов аппаратного прогона
сохранены. Причины основных P0 остаются OPEN; push не выполнялся.
