from pathlib import Path
import hashlib,json
root=Path(__file__).resolve().parents[1]
out=root/'dist/integrated-e2e516-test-20261124-14'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
text=(root/'dist/integrated-e2e516-test-20261124-12/README.ru.md').read_text(encoding='utf-8')
text=(text.replace('v12','v14').replace('75a83a46fe1de440b05a2b149f3d39724e82c49f','333f1620e3698a715b5cb8d366e05997d7982be5')
 .replace('752abb6b2d46f96e7a88aaaa246b6a7dfd78fa14aea252d5813c693b1e06245a','422d6322f90cbdc6e34b58a502878e078e2151ab75bc44167fea17a86b1b6683'))
text+='\nЭкспериментальная GPU-арена, снижение количества ожиданий и кэш sampler не проверены на Switch. Сборка содержит подробную диагностику.\n'
(out/'README.ru.md').write_text(text,encoding='utf-8')
p=out/'manifest.json';m=json.loads(p.read_text(encoding='utf-8'))
for name in ('port/matrixgame/native-engine/native_gpu_upload_arena.hpp','port/matrixgame/native-engine/native_descriptor_cache.hpp','port/matrixgame/native-engine/native_draw_upload.hpp','port/matrixgame/native-engine/native_command_state_cache.hpp','tests/test_matrixgame_gpu_upload_arena.cpp','tests/test_matrixgame_descriptor_cache.cpp'):
 m['inputs_sha256'][name]=sha(root/name)
p.write_text(json.dumps(m,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
(out/'SHA256SUMS.txt').write_text(''.join(f'{sha(p)}  {p.name}\n' for p in sorted(out.iterdir()) if p.is_file() and p.name!='SHA256SUMS.txt'),encoding='utf-8')
with (root/'reports/windows-reference-rebuild-20261123.ru.md').open('a',encoding='utf-8') as f:
 f.write('''\n\n### v14: GPU-арена и повторное использование sampler\n\nДобавлены кэширование CPU/GPU адресов upload-блока на время владения, кэш CPU sampler-дескрипторов по восьми точным словам состояния и однократное создание неизменных texture handles. Каждый draw сохраняет собственную копию дескрипторов в арене; ошибки фабрики не заменяют старое значение. MATRIX_GPU показывает sampler_descriptor_reuses. Арена и кэш проверены с mock SDK: host ASan/UBSan PASS, ARM64/QEMU без санитайзера PASS; devkitA64 compile/link и структура NRO PASS. Фактическое выполнение GPU, изображение, ожидания и FPS — HARDWARE PENDING.\n\nNRO SHA-256 `422d6322f90cbdc6e34b58a502878e078e2151ab75bc44167fea17a86b1b6683`, Build ID `333f1620e3698a715b5cb8d366e05997d7982be5`, профиль `integrated-e2e516-test-v14`.\n''')
