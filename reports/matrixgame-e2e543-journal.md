# E2E-5.43: ранний durable journal и отдельный diagnostic ELF

Реализован `native_diagnostic_journal`: реальные stdio write + fflush + fsync
(Windows `_commit`), отдельная атомарно создаваемая run directory, JSONL,
build/source identity, возрастающая последовательность, monotonic time,
before/ok/failed scopes, last successful operation, Result и elapsed time.
Неизвестный код при exception unwind записывается как `null`, не как успешный 0.
Длинные сообщения ограничены с явным `detail_truncated`, без обрезки UTF-8 символа.
Stage records не требуют heap allocation; concurrent metric records сериализованы.
Ошибка write/flush sticky: следующий Operation не исполняет тело. Ошибки журнала
не мешают noexcept cleanup. Невлитый scope или exception не становится успехом.

HOST ASan/UBSan и ARM64/QEMU проверяют реальные файлы: durable begin виден до
операции, вложенный exception/Result, Unicode, уникальные повторные запуски,
200 concurrent records, семь отказных сценариев (short write, flush, close,
начальное открытие/commit, неправильный порядок scope, invalid filesystem).
Независимый JSON oracle проверяет содержимое, порядок, числа и UTF-8.
RunControl проверяет release-before-exit, deadline, monotonic clock,
только фактически завершённые Present increments, нулевые frames и overflow.
Это CPU/filesystem evidence; настоящие SD/HID/GPU не исполнялись.

Installed libnx `fs_dev.o::fsdev_fsync` содержит реальный вызов `fsFileFlush`;
сохранён disassembly reference. Это SDK implementation evidence, не аппаратный
power-loss/durability test SD. Windows/host fsync не заменяет Switch filesystem.

## Original Init instrumentation

Сохранены оригинальные вызовы и их результаты. Scoped journal добавлен к:
package open, CFG storage/text branches, CacheInit, platform attachment,
debug-font CreateFor, cursor constructor, map constructor, RobotPreload,
stor.Load, PrepareMap, BuildTexUnions, interface/final resources,
ApplyVideoParams и CMinimap::RenderBackground.

WINDOWS HAL PASS: два настоящих Init/PrepareMap/8-frame/recovery/Deinit сеанса
из foreign cwd. Каждый также выполняет ранний missing-package failure. Два
реальных run.jsonl содержат перед вызовом begin, правильный nested failed
package-open/Init и все 15 обязательных успешных stage names. Это проверка
original Windows пути, не SDK/GPU Switch runtime.

## Native diagnostic target

Добавлен отдельный `matrixgame-native-diagnostic.elf`; прежний
`matrixgame-native-entry` остаётся compile/link launcher. Diagnostic main открывает
журнал до SwitchWindow и первого GPU allocation:
`sdmc:/switch/matrixgame-diag/logs/run-…/run.jsonl`.
Original minimap/output получает подкаталог `run-…/output`; ресурсы — прежний
read-only root `sdmc:/switch/space-rangers-hd-a-war-apart/game` либо argv[1].

Build ID и source SHA-256 создаются из actual own sources, generated original
hashes, parent, installed libnx/deko package records и SDK archive hashes.
Это source fingerprint, отдельно от ELF/NRO SHA. При запуске main потоково
вычисляет SHA-256 argv[0] и пишет реальную image digest либо явный unavailable.
SDK environment включает HOS, package versions, applet type и SVC process memory
results. Applet mode отклоняется до GPU; требуется application mode.

Native backend журналирует device/queue/shader/backing, color/depth allocation,
реальные image sizes/alignment, swapchain attach, batch/Present submit/wait и
destructor body. Counters отражают фактические calls/completions: buffer allocation
count/cumulative bytes (НЕ total VRAM), recorded draws, queue submissions,
completed Present, native TextureSession uploads, texture arena capacity/free.
Другие transfer/texture allocations не выдаются за покрытые этим счётчиком.

При успешном полном Init diagnostic main выполняет оригинальные form/loop/draw
через SwitchBackend и останавливается после как минимум четырёх completed Present.
Дополнительных Clear/Present в original loop нет. B/+ после отпускания запускающей
кнопки и 120-секундный deadline проверяются в cooperative checkpoints. Они не
прерывают блокирующий GPU/system/Init call. Base::CException перехватывается с Info,
std::exception — с what; cleanup ошибок не объявляется PASS.

devkitA64 COMPILE/LINK PASS: 185 reachable own link inputs, 0 strong undefined.
Из 255 generated original файлов изменены шесть: pneumatic fix E2E-5.42,
четыре instrumented original TUs и GameStaticResources с одним новым include.
Остальные 249 совпадают с baseline. Сам native main на Switch не исполнялся.

## Что остаётся до передачи одной NRO

**Диагностическая NRO пока НЕ готова и не создавалась.** Отсутствуют законченные
standalone GPU smoke (включая BC1 upload/draw/present/release), экран статуса и
ошибки, проверенный framebuffer readback/checksum, финальный entry cleanup/exit
audit, elf2nro/package manifest/assets/инструкция. Нельзя объявлять эти пункты
пройденными по факту новой ELF или Windows run.jsonl. Первый реальный Switch
runtime blocker остаётся UNKNOWN; SWITCH HARDWARE NOT RUN / HARDWARE_PENDING.

Следующий шаг — самостоятельный реальный smoke/status path на этом же DekoDevice,
затем bounded original Init/Draw и упаковка ровно одной диагностической NRO.
Не требуется переисполнять большие старые Windows pixel/text suites ради этого.
Основная NRO, upstream, frozen, игровые пакеты и сейвы сохранены; commit/push нет.
