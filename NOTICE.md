# Notice

Этот проект использует SpaceRangersHD_CPP и восстановленный SpaceRangersHD_decomp как исходные reference. Права на игровую логику, имена и ресурсы сохраняются за оригинальными правообладателями; наличие MIT-лицензии у upstream C++ не распространяет MIT автоматически на translated game code.

Portable renderer OKGF имеет собственную MIT-лицензию в его submodule. Игровые ресурсы, оригинальные executables и DLL не распространяются этим репозиторием.

Исходники движка планетарных боёв MatrixGame подключены отдельным подмодулем `upstream/matrixgame` из https://github.com/vladislavrv/MatrixGame. Согласно upstream README, каталоги `MatrixGame` и `MatrixLib` распространяются под GNU GPL версии 2 или более поздней; текст лицензии сохранён в `upstream/matrixgame/LICENSE`. Сторонние зависимости libpng и zlib имеют собственные лицензии. MatrixGame пока не включён в Switch NRO; добавление подмодуля не меняет лицензии остальных компонентов проекта.
