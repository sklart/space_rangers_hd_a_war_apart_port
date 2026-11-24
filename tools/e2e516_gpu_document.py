from pathlib import Path
import hashlib,json
root=Path(__file__).resolve().parents[1]
out=root/'dist/integrated-e2e516-test-20261124-12'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
old=(root/'dist/integrated-e2e516-test-20261124-10/README.ru.md').read_text(encoding='utf-8')
readme=(old.replace('v10','v12').replace('7ee30bc6bda66a8c45d8a634466c0f31a98cdaa6','75a83a46fe1de440b05a2b149f3d39724e82c49f')
 .replace('52c4feb1095974d63dd5aa8ecc1237e95bea93c3e2dd5d7bcdad426ce95887ad','752abb6b2d46f96e7a88aaaa246b6a7dfd78fa14aea252d5813c693b1e06245a'))
readme+='\nMATRIX_GPU содержит счётчики повторного использования GPU-состояний; MATRIX_GPU_CAUSE показывает причины и суммарное время ожиданий. Физический GPU и FPS на консоли не проверялись.\n'
(out/'README.ru.md').write_text(readme,encoding='utf-8')
p=out/'manifest.json';manifest=json.loads(p.read_text(encoding='utf-8'))
for name in ('port/matrixgame/native-engine/native_command_state_cache.hpp','tests/test_matrixgame_command_state_cache.cpp'):
 manifest['inputs_sha256'][name]=sha(root/name)
p.write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
(out/'SHA256SUMS.txt').write_text(''.join(f'{sha(p)}  {p.name}\n' for p in sorted(out.iterdir()) if p.is_file() and p.name!='SHA256SUMS.txt'),encoding='utf-8')
with (root/'reports/windows-reference-rebuild-20261123.ru.md').open('a',encoding='utf-8') as f:
 f.write('''\n\n### v12: GPU command-state cache и диагностика ожиданий\n\nВ рамках одной записи команд повторно используются неизменные viewport и раскладка вершин. При новой записи и перед gamma renderer кэш сбрасывается; состояние фиксируется только после команды. GPU upload, порядок draw, барьеры и ожидания не изменены. MATRIX_GPU сообщает vertex_layout_reuses и viewport_reuses; MATRIX_GPU_CAUSE — причины и длительность ожиданий за 5 секунд.\n\nМодель replay на 4096 состояниях и переходах: host ASan/UBSan PASS, ARM64/QEMU PASS; GPU не исполнялся. devkitA64 compile/link и NRO PASS. Реальная корректность, ускорение и доля сэкономленных GPU-команд — HARDWARE PENDING. v10 и v11 сохранены.\n\nNRO SHA-256 `752abb6b2d46f96e7a88aaaa246b6a7dfd78fa14aea252d5813c693b1e06245a`, Build ID `75a83a46fe1de440b05a2b149f3d39724e82c49f`, профиль `integrated-e2e516-test-v12`.\n''')
