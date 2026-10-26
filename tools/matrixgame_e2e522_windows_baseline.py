"""Independent unchanged original compositor oracle; no shared helper in baseline."""
import hashlib,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/matrixgame/e2e522/windows-baseline';CAND=ROOT/'build/matrixgame/e2e522/windows-frame'
def main():
    OUT.mkdir(exist_ok=True);candidate=json.loads((CAND/'result.json').read_text());report={'status':'INCOMPLETE','commands':[]}
    def run(cmd,label,cwd=ROOT):
        script=OUT/(label+'.cmd');script.write_text(cmd+'\n');p=subprocess.run(['cmd.exe','/d','/c',script],cwd=cwd,capture_output=True,text=True,encoding='utf-8',errors='replace');(OUT/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8');report['commands'].append({'label':label,'command':cmd,'exit':p.returncode});print(label,p.returncode,(p.stdout+p.stderr)[-2500:] if p.returncode else '',flush=True);assert p.returncode==0
    try:
        original=ROOT/'build/matrixgame/e2e521/source/MatrixGame/src/OriginalMapPrepare.cpp';generated=OUT/'OriginalPrepareObserved.cpp';text=original.read_text(encoding='utf-8');needle='        CBottomTextureUnion::Get(i).MakeFromBitmap(bmp);';assert text.count(needle)==1 and 'BuildOriginalTextureUnions' not in text
        generated.write_text('class CBitmap;void ObserveOriginalTextureUnion(const CBitmap&,int);\n'+text.replace(needle,'        ObserveOriginalTextureUnion(bmp,i);\n'+needle),encoding='utf-8')
        command=candidate['commands'][0]['command'];fresh=ROOT/'build/matrixgame/e2e522/clean-sdk-release/source/MatrixGame/src/OriginalMapPrepare.cpp';obj=OUT/'prepare.obj';command=command.replace(str(fresh),str(generated)).replace(str(CAND/'OriginalMapPrepare.obj'),str(obj));run(command,'prepare')
        command=next(c['command'] for c in candidate['commands'] if c['label']=='link');command=command.replace('"'+str(CAND/'OriginalMapPrepare.obj')+'"','').replace('"'+str(CAND/'OriginalTextureUnionsObserved.obj')+'"','');command=command.replace(str(CAND/'windows-game.exe'),str(OUT/'windows-game.exe')).replace(str(CAND/'windows-game.map'),str(OUT/'windows-game.map'));command+=' "'+str(obj)+'"';run(command,'link',ROOT/'build/matrixgame/e2e519/original-init-windows')
        p=subprocess.run([OUT/'windows-game.exe',OUT],cwd=candidate['cwd'],env=dict(os.environ,MATRIXGAME_UNION_OUTPUT=str(OUT)),capture_output=True,text=True,timeout=240);(OUT/'runtime.log').write_text(p.stdout+p.stderr);report['stdout']=p.stdout;report['runtime_exit']=p.returncode;assert p.returncode==0
        report['union_sha256']={f'union-{i}.dds':hashlib.sha256((OUT/f'union-{i}.dds').read_bytes()).hexdigest() for i in range(6)};assert report['union_sha256']==candidate['union_sha256']
        report['snapshots']={n:hashlib.sha256((OUT/n).read_bytes()).hexdigest() for n in candidate['snapshots']};assert report['snapshots']==candidate['snapshots'];report['source_sha256']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (original,generated,Path(__file__).resolve())};report['status']='PASS';print('independent original BuildTexUnions baseline: six DDS hashes + full original frames EXACT PASS',flush=True)
    finally:(OUT/'result.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
