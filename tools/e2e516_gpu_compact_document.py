from pathlib import Path
import hashlib,json
root=Path(__file__).resolve().parents[1];out=root/'dist/integrated-e2e516-test-20261124-17'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
text=(root/'dist/integrated-e2e516-test-20261124-16/README.ru.md').read_text(encoding='utf-8')
text=(text.replace('v16','v17').replace('20d96026d822a7dd906a35a06e4e44c8fe4c4f73','e2a9559b01ddf218ab517f6e7d743e05dea33fb2')
 .replace('e134848f18947884180dc9922987b273a9eabb40dc4b7c5844ca938f0a6f6d96','c7e1c50abe3df3d56bc79623e38200263a38410962b69d3dcf9a7d1ada6c817e'))
text+='\nДополнительно уплотнены uniform-данные арены и кэшируются состояния цвета. GPU-поведение и выигрыш на консоли не проверены.\n'
(out/'README.ru.md').write_text(text,encoding='utf-8')
p=out/'manifest.json';m=json.loads(p.read_text(encoding='utf-8'))
for name in ('port/matrixgame/native-engine/native_gpu_upload_arena.hpp','port/matrixgame/native-engine/native_descriptor_cache.hpp','port/matrixgame/native-engine/native_draw_upload.hpp','port/matrixgame/native-engine/native_command_state_cache.hpp','port/matrixgame/deko/material_uniform_packet.hpp','port/matrixgame/deko/material_program.cpp','port/matrixgame/deko/material_program.hpp','tests/test_matrixgame_gpu_upload_arena.cpp','tests/test_matrixgame_command_state_cache.cpp','tests/test_matrixgame_uniform_packet_exact.cpp','tools/e2e516_gpu_v17_checks.sh'):
 m['inputs_sha256'][name]=sha(root/name)
p.write_text(json.dumps(m,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
(out/'SHA256SUMS.txt').write_text(''.join(f'{sha(p)}  {p.name}\n' for p in sorted(out.iterdir()) if p.is_file() and p.name!='SHA256SUMS.txt'),encoding='utf-8')
with (root/'reports/windows-reference-rebuild-20261123.ru.md').open('a',encoding='utf-8') as f:
 f.write('''\n\n### v17: компактный uniform и кэш цвета\n\nUniform-срез арены сокращён с 4096 до 2560 байт, что покрывает все реальные привязки шейдеров и соответствует выравниванию 256 байт; владеющий старый путь сохраняет 4096 байт. При неизменном состоянии повторные color/blend/alpha команды пропускаются, depth/stencil по-прежнему устанавливается; сброс при новом recorder, clear и gamma. Предел пакета увеличен с 512 до 1024 draw при неизменных одном upload-блоке и общем бюджете. Пример 192 байт вершин и 12 индексов: в v15/v16 8192 байта, теперь 4096; модель в 4 МиБ: 512 -> 1024 среза при таких размерах, не измерение реальной нагрузки.\n\nHost ASan/UBSan и ARM64/QEMU PASS: 80 вариантов размещения, данные и дескрипторы, ошибки выделения/submit/wait, крупная загрузка; byte-exact сравнение старого и компактного uniform и границы; состояние и сброс cache. SDK compile/link и структура NRO PASS; реальное использование GPU и FPS HARDWARE PENDING.\n\nNRO SHA-256 `c7e1c50abe3df3d56bc79623e38200263a38410962b69d3dcf9a7d1ada6c817e`, Build ID `e2a9559b01ddf218ab517f6e7d743e05dea33fb2`, профиль `integrated-e2e516-test-v17`.\n''')
