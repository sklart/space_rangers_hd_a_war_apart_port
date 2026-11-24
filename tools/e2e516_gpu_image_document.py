from pathlib import Path
import hashlib,json
root=Path(__file__).resolve().parents[1];out=root/'dist/integrated-e2e516-test-20261124-20'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
text=(root/'dist/integrated-e2e516-test-20261124-19/README.ru.md').read_text(encoding='utf-8')
text=(text.replace('v19','v20').replace('aad951005ff3bbd428d6ebc23ffa1dea85f65906','8191233bf4162cfc497747552a6e319f67db8520')
 .replace('5952b0831a9a877ca57dd75396c815eb2b69ec0fd32bb3ce4b7f412475cfef50','78a6c20dc9fda798a97f177d5e299d4d829c729c315c9b51b5bfd2c6acd4b29c'))
text+='\nВ v20 сокращены повторные операции при подготовке вершин и image-дескрипторов; работа GPU на Switch не проверена.\n'
(out/'README.ru.md').write_text(text,encoding='utf-8')
p=out/'manifest.json';m=json.loads(p.read_text(encoding='utf-8'))
for name in ('port/matrixgame/native-engine/native_gpu_upload_arena.hpp','port/matrixgame/native-engine/native_descriptor_cache.hpp','port/matrixgame/native-engine/native_weak_resource_cache.hpp','port/matrixgame/native-engine/native_draw_upload.hpp','port/matrixgame/native-engine/native_command_state_cache.hpp','port/matrixgame/native-engine/staging_draw.hpp','port/matrixgame/native-core/buffer_resources.cpp','port/matrixgame/deko/fixed_vertex.cpp','port/matrixgame/deko/stencil_state.hpp','port/matrixgame/deko/material_uniform_packet.hpp','port/matrixgame/deko/material_program.cpp','port/matrixgame/deko/material_program.hpp','tests/test_matrixgame_gpu_upload_arena.cpp','tests/test_matrixgame_weak_resource_cache.cpp','tests/test_matrixgame_vertex_decode_one.cpp','tools/e2e516_gpu_v20_checks.sh','tools/e2e516_decode_v20_checks.py'):
 m['inputs_sha256'][name]=sha(root/name)
p.write_text(json.dumps(m,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
(out/'SHA256SUMS.txt').write_text(''.join(f'{sha(p)}  {p.name}\n' for p in sorted(out.iterdir()) if p.is_file() and p.name!='SHA256SUMS.txt'),encoding='utf-8')
with (root/'reports/windows-reference-rebuild-20261123.ru.md').open('a',encoding='utf-8') as f:
 f.write('''\n\n### v20: подготовка вершин и image-дескрипторов\n\nФлаги и число текстур при Decode вычисляются один раз на пакет вершин, чтение diffuse/specular сохранено в исходном порядке; в DecodeTo отдельные цветовые ветки заменяют общий цикл. Неизменные image-дескрипторы кэшируются по weak_ptr, не удерживая GPU-ресурс; при замене или истечении ресурса проверка типа и обновление обязательны. В арене активные image-дескрипторы записываются без предварительного обнуления с последующей перезаписью; неактивные обнуляются. MATRIX_GPU содержит image_descriptor_reuses.\n\nHost ASan/UBSan и ARM64/QEMU PASS: 864 формата вершин/6144 indexed gather, 80 размеров загрузок, все 256 масок слотов, сохранность владельцев при отказах, cache identity/expiry/factory error, uniform и состояния команд. devkitA64 compile/link и структура NRO PASS; эффект на CPU/GPU, стабильность и изображение на Switch HARDWARE PENDING.\n\nNRO SHA-256 `78a6c20dc9fda798a97f177d5e299d4d829c729c315c9b51b5bfd2c6acd4b29c`, Build ID `8191233bf4162cfc497747552a6e319f67db8520`, профиль `integrated-e2e516-test-v20`.\n''')
