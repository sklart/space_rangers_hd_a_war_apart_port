from pathlib import Path
import hashlib
root=Path(__file__).resolve().parents[1]
out=root/'dist/integrated-e2e516-test-20261124-9'
readme='''# Space Rangers HD — интегрированный тест v9

Экспериментальная сборка; работа на Switch и ускорение ещё не проверены.
Build ID: c7fb9f53f9e9e67df48bce6efe971d2733510766
SHA-256 NRO: ab88514cf5407799dabb083ef54f06fd9119440d29d29992c410114c396d44dc

Скопируйте srhd-integrated-test.nro в отдельную папку внутри sdmc:/switch/ и запустите через hbmenu, удерживая R при запуске игры. Игровые ресурсы должны находиться в sdmc:/switch/space-rangers-hd-a-war-apart/game. Старую NRO не заменяйте.

Профиль v9 отдельный: sdmc:/switch/space-rangers-hd-a-war-apart/integrated-e2e516-test-v9/. Для сравнения запустите тот же учебный планетарный бой и похожие действия камеры, что и в v7. Проверьте изображение, управление, плавность, штатный выход из боя и игры. Передайте каталог профиля v9 с logs; при ошибке также сохраните crash report до следующего запуска. Не используйте важные сохранения для теста.

В журнале MATRIX_GPU поле explicit_flushes отражает число явных сбросов приложения, а не реальные отправки GPU. Ускорение и корректность нужно подтвердить на консоли.
'''
(out/'README.ru.md').write_text(readme,encoding='utf-8')
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
(out/'SHA256SUMS.txt').write_text(''.join(f'{sha(p)}  {p.name}\n' for p in sorted(out.iterdir()) if p.is_file() and p.name!='SHA256SUMS.txt'),encoding='utf-8')
with (root/'reports/windows-reference-rebuild-20261123.ru.md').open('a',encoding='utf-8') as f:
 f.write('''\n\n### v9: две экспериментальные оптимизации планетарного боя\n\nПользователь подтвердил работу стиков v7. В v9 на время видимого кадра включён существующий r43 путь SDK flush для drain по лимиту draw; другие причины drain сохраняют прежнее поведение. Новое поле explicit_flushes не является счётчиком реальных GPU kickoff. В NativeDraw проверка последовательных вершин заменена на проверку непрерывного диапазона при полной инициализации; при отказе быстрой проверки выполняется прежняя проверка каждой вершины. Логи профиля сохранены.\n\nHost ASan/UBSan PASS, ARM64/QEMU без sanitizer PASS, ARM64/QEMU с UBSan завершился SIGSEGV и остаётся INCONCLUSIVE; SDK compile/link и структура NRO PASS. Производительность, изображение и стабильность на Switch — HARDWARE PENDING. v7 и промежуточная v8 сохранены.\n\nNRO SHA-256 `ab88514cf5407799dabb083ef54f06fd9119440d29d29992c410114c396d44dc`; Build ID `c7fb9f53f9e9e67df48bce6efe971d2733510766`; профиль `integrated-e2e516-test-v9`.\n''')
