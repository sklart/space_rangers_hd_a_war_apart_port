from pathlib import Path
import hashlib,json
root=Path(__file__).resolve().parents[1];out=root/'dist/integrated-e2e516-test-20261124-19'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
text=(root/'dist/integrated-e2e516-test-20261124-18/README.ru.md').read_text(encoding='utf-8')
text=(text.replace('v18','v19').replace('b389fc1e3284d57edc2a92c78adbe9e4f8a95923','aad951005ff3bbd428d6ebc23ffa1dea85f65906')
 .replace('75231cf37cb70c27410978ff30386fe37988a8ad781caa83121ee256893f52cb','5952b0831a9a877ca57dd75396c815eb2b69ec0fd32bb3ce4b7f412475cfef50'))
text+='\nВ v19 переиспользуется неизменное depth/stencil-состояние и сокращены повторные операции декодирования вершин. GPU и FPS на Switch не проверены.\n'
(out/'README.ru.md').write_text(text,encoding='utf-8')
p=out/'manifest.json';m=json.loads(p.read_text(encoding='utf-8'))
for name in ('port/matrixgame/native-engine/native_gpu_upload_arena.hpp','port/matrixgame/native-engine/native_descriptor_cache.hpp','port/matrixgame/native-engine/native_draw_upload.hpp','port/matrixgame/native-engine/native_command_state_cache.hpp','port/matrixgame/native-engine/staging_draw.hpp','port/matrixgame/native-core/buffer_resources.cpp','port/matrixgame/deko/fixed_vertex.cpp','port/matrixgame/deko/stencil_state.hpp','port/matrixgame/deko/material_uniform_packet.hpp','port/matrixgame/deko/material_program.cpp','port/matrixgame/deko/material_program.hpp','tests/test_matrixgame_command_state_cache.cpp','tests/test_matrixgame_vertex_decode_one.cpp','tools/e2e516_decode_v19_checks.py'):
 m['inputs_sha256'][name]=sha(root/name)
p.write_text(json.dumps(m,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
(out/'SHA256SUMS.txt').write_text(''.join(f'{sha(p)}  {p.name}\n' for p in sorted(out.iterdir()) if p.is_file() and p.name!='SHA256SUMS.txt'),encoding='utf-8')
with (root/'reports/windows-reference-rebuild-20261123.ru.md').open('a',encoding='utf-8') as f:
 f.write('''\n\n### v19: depth/stencil и декодирование вершин\n\nПовторно используется неизменный depth/stencil state внутри записи; ключ включает все поля обеих граней, reference, маски, depth test/write/compare. ReadStencilState и проверка attachment по-прежнему выполняются для каждого draw; cache сбрасывается при recorder/clear/gamma. В декодировании вершин предварительно вычисляются флаги формата и текстур, отдельные ветки цветов сохраняют порядок чтения. MATRIX_GPU содержит depth_reuses.\n\nХост ASan/UBSan и ARM64/QEMU PASS: 864 формата, 3456 вершин, 2592 некорректных случая, 6144 indexed gather; 1000 сравнений index validation с эталоном, проверка изменений полей depth/stencil и 4096 переходов state. devkitA64 compile/link и NRO PASS. Физический GPU, графика и FPS HARDWARE PENDING.\n\nNRO SHA-256 `5952b0831a9a877ca57dd75396c815eb2b69ec0fd32bb3ce4b7f412475cfef50`, Build ID `aad951005ff3bbd428d6ebc23ffa1dea85f65906`, профиль `integrated-e2e516-test-v19`.\n''')
