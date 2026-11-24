from pathlib import Path
import hashlib,json
root=Path(__file__).resolve().parents[1];out=root/'dist/integrated-e2e516-test-20261124-23'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
text='''# Space Rangers HD — интегрированный тест v23

Экспериментальная сборка; Switch GPU, производительность и новые жесты не проверены.

Build ID: b6f6789878c52be85285db19247759581206b034
SHA-256 NRO: 672018893e8c255f5a0075d638d7dd6dd1f2b63885b0afd310a18e264d17f6c8

Скопируйте srhd-integrated-test.nro в отдельную папку sdmc:/switch/ и запустите через hbmenu в application mode, удерживая R при запуске игры. Игровые ресурсы: sdmc:/switch/space-rangers-hd-a-war-apart/game. Старые NRO не заменяйте. Конфигурация, сохранения и журналы этого теста находятся отдельно в sdmc:/switch/space-rangers-hd-a-war-apart/integrated-e2e516-test-v23/.

В планетарном бою левый стик двигает курсор, правый перемещает камеру. Свайп одним пальцем перемещает карту, короткое касание работает как нажатие. Нажатие правого стика приближает камеру, левого — отдаляет; удержание повторяет шаги. Свайп использует штатные команды прокрутки камеры и не претендует на точное попиксельное следование пальцу. Сенсорная раскладка в остальных экранах не изменена.

Для сравнения используйте учебный бой и похожие движения камеры. Если возникнет ошибка, сохраните каталог профиля v23 вместе с logs и crash report до следующего запуска. Не используйте важные сохранения в эксперименте.
'''
(out/'README.ru.md').write_text(text,encoding='utf-8')
p=out/'manifest.json';m=json.loads(p.read_text(encoding='utf-8'))
for name in ('port/matrixgame/native-engine/native_gpu_upload_arena.hpp','port/matrixgame/native-engine/native_arena_preference.hpp','port/matrixgame/native-engine/native_vertex_scratch.hpp','port/matrixgame/native-engine/native_vertex_marks.hpp','port/matrixgame/native-engine/native_sampler_set_cache.hpp','port/matrixgame/native-engine/native_weak_resource_cache.hpp','port/matrixgame/native-engine/native_touch_gesture.hpp','port/matrixgame/native-engine/native_input_switch.hpp','port/matrixgame/native-engine/native_input_switch.cpp','port/matrixgame/deko/material_uniform_packet.hpp','port/matrixgame/deko/fixed_vertex.cpp','tests/test_matrixgame_touch_gesture.cpp','tests/test_matrixgame_vertex_scratch.cpp','tests/test_matrixgame_arena_preference.cpp','tools/e2e516_gpu_v23_checks.sh','tools/e2e516_decode_v23_checks.py'):
 m['inputs_sha256'][name]=sha(root/name)
p.write_text(json.dumps(m,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
(out/'SHA256SUMS.txt').write_text(''.join(f'{sha(p)}  {p.name}\n' for p in sorted(out.iterdir()) if p.is_file() and p.name!='SHA256SUMS.txt'),encoding='utf-8')
with (root/'reports/windows-reference-rebuild-20261123.ru.md').open('a',encoding='utf-8') as f:
 f.write('''\n\n### v23: сенсорный свайп, зум и оптимизации CPU/GPU\n\nВ бою короткое касание преобразуется в click при отпускании; после порога движения жест вместо click даёт штатные KA_SCROLL в направлении, противоположном движению пальца. Отслеживается finger_id, при потере finger другие контакты блокируются до полного отпускания; при потере фокуса жест и удержания сбрасываются. В остальных UI используется прежняя обработка касаний. Клик правого стика отправляет шаг zoom in через существующий B_WHEEL/ZoomInStep, левого — zoom out, удержание повторяет с интервалом 220 мс; нормальное движение стиков сохранено. Это не попиксельная прокрутка; аппаратное удобство и работа жестов НЕ ПРОВЕРЕНЫ.\n\nIndexed draw использует device-local временный буфер с двумя независимыми слотами и fallback для вложенности; заполнение сбрасывается до значений по умолчанию, память крупнее 4096 вершин не удерживается. Таблица marks использует stack до 512 вершин. При fallback GPU-арены на 4 МиБ попытки 8 МиБ откладываются на 64 успешных выделения, затем автоматически повторяются.\n\nHost ASan/UBSan и ARM64/QEMU PASS: touch tap/swipe/zoom repeat и объединение источников; scratch reuse/reset/nesting/oversized/failure; предпочтение и fallback арены; 864 формата и 6144 indexed draw bit-exact с 12276 исключёнными выделениями против эталона; регрессии буферов, marks и кэшей. Это CPU/mock, не реальное HID/GPU. SDK compile/link и NRO PASS; Switch GPU, новые жесты, производительность и длительная игра HARDWARE PENDING.\n\nNRO SHA-256 `672018893e8c255f5a0075d638d7dd6dd1f2b63885b0afd310a18e264d17f6c8`, Build ID `b6f6789878c52be85285db19247759581206b034`, профиль `integrated-e2e516-test-v23`.\n''')
