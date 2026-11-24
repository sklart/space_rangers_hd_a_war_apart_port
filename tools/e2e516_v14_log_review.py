import hashlib,re,subprocess,statistics
from pathlib import Path
p=Path(r'D:\Download\Telegram Desktop\integrated-e2e516-test-v14.7z')
print('archive_sha256',hashlib.sha256(p.read_bytes()).hexdigest())
base='integrated-e2e516-test-v14/'
for name in ('logs/stdout.log','logs/stderr.log','config/########.log','logs/perf515-stage.log','config/Lang.txt'):
 data=subprocess.check_output(['tar','-xOf',str(p),base+name]);s=data.decode('utf-8','replace');lines=s.splitlines()
 print('\n###',name,'bytes',len(data),'lines',len(lines))
 if name.endswith('stdout.log') or name.endswith('Lang.txt'):print('\n'.join(lines[-25:]));continue
 if name.endswith('stderr.log'):
  gpu=[x for x in lines if '[MATRIX_GPU]' in x];causes=[x for x in lines if '[MATRIX_GPU_CAUSE]' in x]
  print('GPU windows',len(gpu),'causes',len(causes));fps=[]
  for x in gpu:
   d={k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',x)}
   if d.get('window_ms'):fps.append(d['frames']*1000/d['window_ms'])
  if fps:print('FPS min median max',round(min(fps),2),round(statistics.median(fps),2),round(max(fps),2))
  print('FIRST GPU',*gpu[:4],sep='\n');print('LAST GPU',*gpu[-6:],sep='\n');print('CAUSES',*causes[-12:],sep='\n')
 matches=[x for x in lines if re.search(r'exception|fatal|crash|abort|conflict|error|fail|planetary battle|game shutdown|programmain',x,re.I) and '[MATRIX_GPU]' not in x]
 print('MATCHES',len(matches));print('\n'.join(matches[:12]+matches[-18:]));print('TAIL');print('\n'.join(lines[-10:]))
print('\n### aggregate comparisons')
for version in ('v7','v14'):
 path=p.parent/f'integrated-e2e516-test-{version}.7z'
 data=subprocess.check_output(['tar','-xOf',str(path),f'integrated-e2e516-test-{version}/logs/stderr.log']).decode('utf-8','replace')
 samples=[{k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',x)} for x in data.splitlines() if '[MATRIX_GPU]' in x]
 draws=sum(x['draws'] for x in samples);drains=sum(x['drains'] for x in samples)
 print(version,'windows',len(samples),'draws',draws,'drains',drains,'draws_per_drain',round(draws/drains,2),'median_fps',round(statistics.median(x['frames']*1000/x['window_ms'] for x in samples),2))
