from pathlib import Path
import hashlib,json
root=Path(__file__).resolve().parents[1];out=root/'dist/integrated-e2e516-test-20261124-15'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
text=(root/'dist/integrated-e2e516-test-20261124-14/README.ru.md').read_text(encoding='utf-8')
text=(text.replace('v14','v15').replace('333f1620e3698a715b5cb8d366e05997d7982be5','55f9f5b8643211d7d01b1eaf725003c6553d6459')
 .replace('422d6322f90cbdc6e34b58a502878e078e2151ab75bc44167fea17a86b1b6683','3094655502d3d2bc790152c0df2e4c35ec0d233df0edb7b375f4d95794c6498f'))
text+='\nВ v15 внутренние части каждого upload-среза упакованы плотнее. Реальные GPU-адреса, изображение, снижение ожиданий и FPS на Switch не проверены.\n'
(out/'README.ru.md').write_text(text,encoding='utf-8')
p=out/'manifest.json';m=json.loads(p.read_text(encoding='utf-8'))
for name in ('port/matrixgame/native-engine/native_gpu_upload_arena.hpp','port/matrixgame/native-engine/native_descriptor_cache.hpp','port/matrixgame/native-engine/native_draw_upload.hpp','port/matrixgame/native-engine/native_command_state_cache.hpp','tests/test_matrixgame_gpu_upload_arena.cpp','tests/test_matrixgame_descriptor_cache.cpp'):
 m['inputs_sha256'][name]=sha(root/name)
p.write_text(json.dumps(m,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
(out/'SHA256SUMS.txt').write_text(''.join(f'{sha(p)}  {p.name}\n' for p in sorted(out.iterdir()) if p.is_file() and p.name!='SHA256SUMS.txt'),encoding='utf-8')
with (root/'reports/windows-reference-rebuild-20261123.ru.md').open('a',encoding='utf-8') as f:
 f.write('''\n\n### v15: компактное размещение GPU upload\n\nОснование: журнал v14 показывает около 134 draw на drain и преимущественно причину upload_bytes; аппаратный профиль: reports/matrixgame-e2e516-v14-hardware-20261124.ru.md. В общей арене начало каждого draw остаётся выровненным по 4096 байт, внутренние области выравниваются по 256 байт, что кратно SDK alignment дескрипторов и uniform. Предел draw на пакет увеличен с 256 до 512; прежний бюджет 256 МиБ и один GPU upload-блок на пакет сохранены. Пример 192 байта вершин и 12 байт индексов: 20480 -> 8192 байт, до 204 -> 512 срезов в 4 МиБ при таких размерах; это модель, не замер игрового FPS.\n\n80 сочетаний размеров, не перекрывающиеся диапазоны, индексы, дескрипторы, uniform, неверные размеры, ошибки submit/wait и реальная крупная загрузка выше 4 МиБ: host ASan/UBSan и ARM64/QEMU PASS с mock SDK. devkitA64 compile/link и структура NRO PASS. GPU и производительность HARDWARE PENDING; фактическое число загрузок зависит от сцены. Счётчик arena_fills отделяет заполнение блока от общих причин ожидания.\n\nNRO SHA-256 `3094655502d3d2bc790152c0df2e4c35ec0d233df0edb7b375f4d95794c6498f`, Build ID `55f9f5b8643211d7d01b1eaf725003c6553d6459`, профиль `integrated-e2e516-test-v15`.\n''')
