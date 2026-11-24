from pathlib import Path
import hashlib,json
root=Path(__file__).resolve().parents[1];out=root/'dist/integrated-e2e516-test-20261124-18'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
text=(root/'dist/integrated-e2e516-test-20261124-17/README.ru.md').read_text(encoding='utf-8')
text=(text.replace('v17','v18').replace('e2a9559b01ddf218ab517f6e7d743e05dea33fb2','b389fc1e3284d57edc2a92c78adbe9e4f8a95923')
 .replace('c7e1c50abe3df3d56bc79623e38200263a38410962b69d3dcf9a7d1ada6c817e','75231cf37cb70c27410978ff30386fe37988a8ad781caa83121ee256893f52cb'))
text+='\nДополнительно кэшируются повторные проверки вершин в пределах draw и постоянное состояние линий. Работа на физическом GPU не проверена.\n'
(out/'README.ru.md').write_text(text,encoding='utf-8')
p=out/'manifest.json';m=json.loads(p.read_text(encoding='utf-8'))
for name in ('port/matrixgame/native-engine/native_gpu_upload_arena.hpp','port/matrixgame/native-engine/native_descriptor_cache.hpp','port/matrixgame/native-engine/native_draw_upload.hpp','port/matrixgame/native-engine/native_command_state_cache.hpp','port/matrixgame/native-engine/staging_draw.hpp','port/matrixgame/native-core/buffer_resources.cpp','port/matrixgame/deko/material_uniform_packet.hpp','port/matrixgame/deko/material_program.cpp','port/matrixgame/deko/material_program.hpp','tests/test_matrixgame_index_validation_equivalence.cpp','tests/test_matrixgame_command_state_cache.cpp','tools/e2e516_gpu_v18_checks.sh'):
 m['inputs_sha256'][name]=sha(root/name)
p.write_text(json.dumps(m,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
(out/'SHA256SUMS.txt').write_text(''.join(f'{sha(p)}  {p.name}\n' for p in sorted(out.iterdir()) if p.is_file() and p.name!='SHA256SUMS.txt'),encoding='utf-8')
with (root/'reports/windows-reference-rebuild-20261123.ru.md').open('a',encoding='utf-8') as f:
 f.write('''\n\n### v18: повторные индексы и состояние линий\n\nВ пределах одного callback-free ValidateIndexed 32-слотовый кэш пропускает повторное подтверждение уже проверенных вершин; проверка каждого индекса относительно объявленного диапазона сохраняется. Цикл чтения разделён по ширине индекса. Неизменная ширина/штриховка линий повторно используется внутри записи команд, сбрасывается при новой записи, clear и gamma; MATRIX_GPU отражает line_reuses.\n\nСравнение с независимым последовательным эталоном на 1000 наборах (643 PASS, 357 отказов), в том числе U16/U32, частично неинициализированные буферы, отрицательный base; регрессии границ и состояние cache: host ASan/UBSan и ARM64/QEMU PASS. SDK compile/link и NRO PASS; влияние на скорость и GPU HARDWARE PENDING.\n\nNRO SHA-256 `75231cf37cb70c27410978ff30386fe37988a8ad781caa83121ee256893f52cb`, Build ID `b389fc1e3284d57edc2a92c78adbe9e4f8a95923`, профиль `integrated-e2e516-test-v18`.\n''')
