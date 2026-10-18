# MatrixLib: нативный CFile и чтение настоящих ресурсов

Статус PARTIAL. Это рабочая ресурсная CPU-часть, не полный MatrixGame, не бой,
не engine Run и не аппаратная проверка. Работа ведётся в отдельном worktree;
закреплённые submodules и стабилизационная NRO 3772031 не менялись.

## Реализация

Сохранён публичный API оригинального CFile; Win32 HANDLE заменён private pimpl.
Подготовка проверяет девять оригинальных входных файлов по LF-normalized SHA256.
Оригинальные LoadFromFile/SaveInFile класса CBuf подключены без изменения к
настоящему stdio/64-bit file backend. Package entry IO использует уже существующий
bounded reader основной игры: 16 handles, один FILE на entry, ограниченные packed/
decoded buffers. Metadata/handles/IO общего registry сериализованы mutex.

Сохранены disk-before-package lookup, nested Open/Close, disk-only OpenReadNE,
disk-only writes, списки расширений через ~ и ?parameters в FileExist. Имена,
возвращаемые FileExist, сохраняют исходные разделители; нормализация выполняется
внутри backend. Disk lookup поддерживает ASCII case folding, Unicode physical
paths работают через стандартный wchar_t ABI и явную UTF-8 границу.

Отдельно сохранена особенность оригинала: package seek от EOF использует
положительную дистанцию назад, а disk seek — знаковое смещение Win32/stdio.
Недопустимые позиции и переполнения отвергаются. SizeFull/PointerFull работают
через реальные 64-битные операции; 32-битные getters отвергают переполнение.

Общий Package::Open сохранил прежнюю narrow-path семантику. Для native CFile
добавлен явный OpenUtf8; на Windows он открывает package через wide filesystem
path/_wfopen. Ни одна файловая функция не возвращает фиктивные данные.

## Проверки и их границы

- Оригинальный MSVC x86 CFile: допустимые disk/Unicode/extension/nested-open
  операции и 300 произвольных чтений сжатого fixture на 1100003 bytes PASS.
- Host ASan/UBSan и native MSVC x86: CRC/CBuf/CFile contracts PASS.
- Четыре reader threads выполнили 256 дополнительных чтений; ThreadSanitizer PASS.
- Повреждённый последний compressed block вызывает ошибку; после seek к началу
  чтение восстанавливается. Неудачный OpenPackFiles очищает collection, а 17-й
  entry отвергается; после освобождения slot retry успешен.
- Unicode package pathname содержит кириллицу и emoji; Linux/Windows native PASS.
- Sparse file >4 GiB проверен в Linux host /tmp: настоящее filesystem::file_size,
  read/write/seek и отказ переполняющихся 32-битных getters. Windows этот сценарий
  не запускался, чтобы не создавать физический 4-GiB файл на NTFS.
- Настоящий Matrix/Building/ruins/b0.vo из robots.pkg: 140642 bytes, SHA256
  de47bf4b533f2e179d85dae860740d63923f8e339ad15ca43b4cb4fa27e88ed6,
  FNV32 b98588fc. Совпали original x86, host ASan/UBSan, Linux ARM64/QEMU и
  независимый Python package parser/zlib oracle. Payload в Git не добавляется.
- devkitA64: static CPU library и полный link-only ELF CFile contract собраны,
  AArch64, zero strong undefined symbols. ELF не выполнялся и в NRO не паковался.
- Existing bounded streaming regression PASS: late corruption/retry, interleaved
  readers, raw sequential и large random IO; largest allocation 72112 bytes.

Linux ARM64 runner собирает настоящий zlib 1.3.2 из официального commit
[da607da739fa6047df13e66a2af6b8bec7c2a498](https://github.com/madler/zlib/commit/da607da739fa6047df13e66a2af6b8bec7c2a498).
Источник проверяется на pin/clean state. В систему пакеты не устанавливаются;
SDK Switch продолжает использовать свой portlibs zlib.

## Отличия небезопасных путей оригинала

Исходный x86 PointerFull getter передаёт неинициализированный LONG hi в
SetFilePointer; этот getter не использовался как эталон 64-битных позиций.
Ошибочный short read оригинала завершился 0xc0000005; в исходном CExceptionStr
есть безусловное добавление str2 при default nullptr. Поэтому x86 baseline
сравнивает допустимые операции. Native error paths проверены отдельно.
Эти результаты не устанавливают причину аппаратного freeze после «Готово».

## Остаётся

Windows shareMode является advisory на portable backend; Win32 locking semantics
не объявляются воспроизведёнными. Case folding ограничен ASCII. Кодировка
не-ASCII виртуальных PKG entry names требует отдельной проверки.

Не перенесены CStorage UTF-16 name hashing, его object lifetime при realloc/memcpy,
прочие MatrixLib units, engine entry/Run, реальные materials/scene submission,
swapchain, бой и campaign handoff. Save gating полноценной игры и hardware gates
этим CPU-контрактом не закрываются. Физический прогон 3772031 пользователь отложил.

Локальное доказательство: build/matrixgame/native-core/file-proof.json;
логи file-asan.log, file-tsan.log, file-msvc.log, file-arm64.log, file-devkit.log,
shared-package-regression.log, file-real-{x86,asan,arm64}-result.json.
CI дополнен ARM64 runner, TSan file contract и devkit ELF link probe. Удалённый
CI нового файлового изменения ещё не завершён.
