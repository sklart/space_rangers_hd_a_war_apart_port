from pathlib import Path
import hashlib,json
root=Path(__file__).resolve().parents[1];out=root/'dist/integrated-e2e516-test-20261124-16'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
text=(root/'dist/integrated-e2e516-test-20261124-15/README.ru.md').read_text(encoding='utf-8')
text=(text.replace('v15','v16').replace('55f9f5b8643211d7d01b1eaf725003c6553d6459','20d96026d822a7dd906a35a06e4e44c8fe4c4f73')
 .replace('3094655502d3d2bc790152c0df2e4c35ec0d233df0edb7b375f4d95794c6498f','e134848f18947884180dc9922987b273a9eabb40dc4b7c5844ca938f0a6f6d96'))
text+='\nВ v16 дополнительно кэшируются неизменные привязки shader/texture и состояние raster; uniform-пакет формируется с сохранением всех байтов. Корректность GPU на Switch не проверена.\n'
(out/'README.ru.md').write_text(text,encoding='utf-8')
p=out/'manifest.json';m=json.loads(p.read_text(encoding='utf-8'))
for name in ('port/matrixgame/native-engine/native_gpu_upload_arena.hpp','port/matrixgame/native-engine/native_descriptor_cache.hpp','port/matrixgame/native-engine/native_draw_upload.hpp','port/matrixgame/native-engine/native_command_state_cache.hpp','port/matrixgame/deko/material_uniform_packet.hpp','port/matrixgame/deko/material_program.cpp','port/matrixgame/deko/material_program.hpp','tests/test_matrixgame_command_state_cache.cpp','tests/test_matrixgame_uniform_packet_exact.cpp'):
 m['inputs_sha256'][name]=sha(root/name)
p.write_text(json.dumps(m,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
(out/'SHA256SUMS.txt').write_text(''.join(f'{sha(p)}  {p.name}\n' for p in sorted(out.iterdir()) if p.is_file() and p.name!='SHA256SUMS.txt'),encoding='utf-8')
with (root/'reports/windows-reference-rebuild-20261123.ru.md').open('a',encoding='utf-8') as f:
 f.write('''\n\n### v16: GPU state и uniform packet\n\nДобавлено повторное использование неизменных shader/texture bindings и raster state внутри записи; кэш сбрасывается при новой записи, gamma и clear. Uniform packet заполняется без двойной записи payload, при этом все байты и исходная проверка ошибок сохраняются. Счётчики material_binding_reuses и raster_reuses доступны в MATRIX_GPU. 4096 переходов в модели GPU state и побайтовое сравнение uniform на разных заполнителях, при наличии/отсутствии vertex и неверных аргументах: host ASan/UBSan и ARM64/QEMU PASS. devkitA64 compile/link и NRO PASS. Взаимодействие с реальным GPU, изображение, FPS и стабильность HARDWARE PENDING.\n\nNRO SHA-256 `e134848f18947884180dc9922987b273a9eabb40dc4b7c5844ca938f0a6f6d96`, Build ID `20d96026d822a7dd906a35a06e4e44c8fe4c4f73`, профиль `integrated-e2e516-test-v16`.\n''')
