# E2E-5.19: оригинальный переход экрана

Подключён полный исходный `MatrixTransition.cpp`: BuildTexture, Render,
RenderToPrimaryScreen, Takt и Clear. Раньше native target содержал только Clear.
Оригинальные чтение конфигурации, геометрия, скорости сдвига, batching и флаг
карты сохранены. SDK calls проходят через существующие render/sampler/offscreen
адаптеры; захват окна — через новый transition_backend с настоящим Windows GDI.
CPU-подмена снимка окна, текстуры или успешного боя не используется.

## Доказанный результат

Forced full CGame::Init link на ARM64 сократился с шести отсутствующих связей до
четырёх. Обе связи CTransition закрыты оригинальными методами. Остались:

- `CMatrixMap::CreatePoolDefaultResources(bool)`;
- `CMatrixMap::InitObjectsLights()`;
- `CMatrixMap::PrepareMap(Base::CStorage&, std::wstring const&)`;
- `CMatrixMap::Restart()`.

Это граница **link**, не выполненный CGame::Init. Карта и бой не запущены.

Host ASan/UBSan/LSan: compile/link/runtime PASS. Оригинальный BuildTexture
действительно вызывается и отклоняется на отсутствии native window до выделения
геометрии. Пустые Render/Takt, Clear/Clear и повторное создание исполняются в
двух сессиях. ARM64/QEMU повторяет новый адресный сценарий — PASS.

devkitA64: оба новых TU и обновлённый test driver COMPILE PASS; кандидатный ELF
LINK PASS, strong undefined отсутствуют. Native объекты предыдущего Text-кандидата
переиспользованы; старый lifecycle member удалён из новой копии архива.
Основная NRO/ELF и frozen комплекты не затрагиваются. Switch HARDWARE PENDING.

Windows MSVC: полный оригинальный TU и backend COMPILE PASS. Отдельный настоящий
HAL/GDI test LINK/RUNTIME PASS: три входа, создание совместимых bitmap/DC,
BitBlt, исходный алгоритм WinBitmap::Save с GetDIBits/FlipY/SwapByte. Все RGB
пиксели двух разноцветных половин16×16 и ориентация совпали. Освобождение DC
идемпотентно; Bitmap cleanup/reentry проверены. Полный игровой BuildTexture,
загрузка DXT1 именно этого снимка и RenderToPrimaryScreen на Windows **ещё не
исполнялись**. Этот backend test не подменяет полный игровой gate.

## Владение и оставшиеся ограничения

GetDC теперь принадлежит CaptureDC с ReleaseDC в деструкторе. Сохраняются оба
исходных явных Release, а ранние возвраты при отсутствии Transitions/разрешения
также освобождают DC. Bitmap сохраняет исходную пару DeleteDC/DeleteObject,
CPU-данные освобождает CBitmap. Это изменение управления SDK-ресурсом, а не
алгоритма перехода. Новых sanitizer exclusions нет.

Исходный Clear оставлен целиком: он освобождает m_Geom только внутри `if(m_Tex)`.
BuildTexture выделяет геометрию раньше создания текстуры. Риск утечки при
раннем отказе после этого выделения **остаётся**, runtime воспроизведение
такого пути ещё не сделано. Cold refusal не доказывает cleanup после GDI/DXT1
failure. Повторный BuildTexture на уже заполненном объекте также не проверен.

Windows Bitmap::Save перенесён с исходной последовательностью запросов,
поддержкой16/24/32bit, преобразованием16→32 и порядком каналов. Остальные
платформы явно отклоняют GDI capture и Present без настоящего provider.
Обычный deko3d/provider для этого перехода пока отсутствует.

Артефакты: `build/matrixgame/e2e519/transition-{source,host,arm64,sdk,msvc,gdi}`.
Host/ARM ABI соответствует предыдущим native объектам. Обновлённый общий
test driver включён в финальные host, ARM64 и SDK-кандидаты; промежуточный
ARM64 baseline исполнял прежний driver, поэтому для нового сценария учитывается
только `updated-result.json`. SDK runner исправлял окружение DEVKITPRO и
экранирование header macros; это не изменения движка.

[Команды, хэши и реальные результаты](matrixgame-e2e519-transition-evidence.json).
Проблемы основной игры — повторные фоны, исторический syscall ENOENT,
аппаратные freeze карты/выхода — остаются OPEN. Push и новая NRO не выполнялись.
