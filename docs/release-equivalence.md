# Проверка формата пакета релиза

Эталон: установленная Windows-версия от 2025-10-13, `Rangers.exe` SHA-256
`83300344af802bc51e64389c58f047e5afdf195c133048098be3881fae29ed98`.
Проверка запускается без копирования игровых файлов:

```text
cd port/switch
make -f Makefile clean
make -f Makefile host-package-test
make -f Makefile host-package-tree-test
make -f Makefile host-package-payload-test
make -f Makefile host-package-payload-cpp-test
make -f Makefile host-corrupt-package-test
make -f Makefile host-package-cycle-test
make -f Makefile host-zl02-synthetic-test
```

На проверенной локальной установке `common.pkg` содержит 51 папку, 1 890
файлов, 1 940 записей, максимальную глубину 4. Дерево строится рекурсивно по
`TargetOffset`; 158-байтная запись диска не содержит нативных указателей в
runtime-представлении. Путь сопоставляется без учёта ASCII-регистра и принимает
`/` и `\`.

Нормализованное дерево имеет FNV-1a-64 `9c74d6b37be3edd2`. Реальный payload
`DATA/Asteroid/00.gai` имеет `kind=2`, размер 246863 и CRC32 `045269e4`.
Его блоки имеют контейнер `ZL02`, decoded size и zlib stream; Switch backend
распаковывает их через zlib без Win32 HANDLE. Проверка на настоящей Switch пока
**PENDING MANUAL VERIFICATION**.
