# MatrixGame — продолжение native UI text

Итог: **TEXT SOFTWARE PASS / FULL NATIVE INIT PARTIAL**.
HOST ASan/UBSan, ARM64/QEMU и SDK compile/link прошли окончательную проверку.
Аудит подтвердил 252 неизменных generated original файла, 15 защищённых
входов и 107 аппаратных входов. Evidence:
reports/matrixgame-text-contract-followup-evidence.json.
SDK ELF SHA256: d74c23e4236c5d64280b47d9b0eb56f74055e1e1c185f5c3d40b5bb81d4229a9.

Text::Render → DeviceIdentity для исходного Font.2Ranger подключены в
E2E-5.24. Здесь устранён дополнительный дефект трёх native-фабрик:
CreateTextFont, GetSurface, CreateRenderSurface читали входное значение `*out`.
Выходной слот теперь не читается, сбрасывается в nullptr перед валидацией,
получает зарегистрированный owner только после успешного создания.
Ненулевое прежнее значение не принимается за существующий объект и не
освобождается. Передавать живой owner без предварительного Release нельзя.

Оригинальные parser/layout/font cache, данные Rangers.ttf, метрики, glyph
coverage и raster algorithm сохранены. Оригинальный Text::Render возвращает
bitmap; native backend действительно рисует BGRA и загружает texture через
native upload. Фиктивного COM device/font и пустой текстуры нет.

Проверки записаны отдельно от предыдущих завершённых этапов:
build/matrixgame/e2e529-text-contract/{host,arm64,sdk}/result.json.
Номер каталога не означает завершения StoredStateBackend.

- HOST: адресный ASan/UBSan, исходный Text::Render «Меню», 17940 Windows
  goldens, output slots с ненулевым прежним значением и обнуление при отказе,
  cleanup/upload failure/retry, cache rebind и managed reupload.
- ARM64/QEMU: те же production font/text TUs и проверки с assertions.
- COMPILE/LINK: отдельный полный devkitA64 ELF, включая оригинальные
  CGame::Init, PrepareMap, Text::Render и настоящий native backend;
  strong undefined symbols отсутствуют. ELF не является доказательством Init.
- WINDOWS HAL: использованы ранее полученные 17940 точных BGRA/layout/return
  сравнений E2E-5.24. Их assets и неизменность raster/exporter проверяются
  повторно по SHA256. Нового Windows runtime в этом продолжении нет.
  Полная Windows-регрессия не повторяется для native-only изменения фабрик.
- SWITCH HARDWARE: NOT RUN / HARDWARE_PENDING. Карта, upload на консоли и
  первый Present не подтверждены.

Первый запуск нового теста завершился assertion: mip1 ошибочно считался
недопустимым, хотя CreateTexture создаёт полную цепочку. Тест исправлен на
UINT_MAX; исходная диагностика сохранена в initial-test-*.
Окончательный статус принимает только tools/matrixgame_text_followup_audit.py
при успешных HOST/ARM64/SDK и проверке hashes защищённых входов.

Следующая открытая dependency: CMatrixMap::ReleasePoolDefaultResources →
CDeviceState(map_device::StateBackend()) →
**map stored device state backend unavailable**. Это source-traced ветка
release/reset, а не наблюдавшаяся первая ошибка полного native Init.
Исходный CDeviceState хранит 103 render ordinals, 13 sampler ordinals ×8,
8 lights/enabled и material. DekoDevice пока не предоставляет этот backend;
из render setter routes есть 24, из sampler 8/13. Нельзя просто привести
original ordinal к D3D numeric ID: например ordinal1 — FILLMODE, numeric8.
Нельзя заменить скалярный StoreState внешним CaptureState lease, удерживающим
default texture/buffer owners и тем самым блокирующим reset.
Инвентаризация: reports/matrixgame-e2e528-stored-state-inventory.json.

Полный native Init остаётся PARTIAL. Для первой карты нужны оставшаяся
stored-state интеграция и реальное native исполнение оригинальной цепочки;
завершение линковки не закрывает эти gates. Ограничения вне исходных font
faces и сложного shaping из E2E-5.24 сохраняются.

NRO, frozen, игровые ресурсы и сейвы не меняются; commit/push не выполняются.
P0, причинная роль GI cache в повторяющемся тексте, manual-publish ENOENT и
исторические Switch hangs остаются OPEN.
