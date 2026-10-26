"""Original full Windows Init/frame regression plus lossless union observations."""
import hashlib,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/matrixgame/e2e522/windows-frame'
def main():
    OUT.mkdir(parents=True,exist_ok=True)
    prior=json.loads((ROOT/'build/matrixgame/e2e521/cursor/windows-frame/result.json').read_text(encoding='utf-8'))
    template=prior['commands'][0]['command'];report={'status':'INCOMPLETE','commands':[]}
    def run(command,label,cwd=ROOT):
        script=OUT/(label+'.cmd');script.write_text(command+'\n')
        p=subprocess.run(['cmd.exe','/d','/c',script],cwd=cwd,capture_output=True,text=True,encoding='utf-8',errors='replace')
        (OUT/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8');report['commands'].append({'label':label,'command':command,'exit':p.returncode});print(label,p.returncode,(p.stdout+p.stderr)[-4000:] if p.returncode else '',flush=True)
        assert p.returncode==0,label
    try:
        original=ROOT/'build/matrixgame/e2e522/clean-sdk-release/source/MatrixGame/src/OriginalMapPrepare.cpp'
        helper=original.with_name('OriginalTextureUnions.cpp');generated=OUT/'OriginalTextureUnionsObserved.cpp';text=helper.read_text(encoding='utf-8')
        needle='        CBottomTextureUnion::Get(i).MakeFromBitmap(bmp);';assert text.count(needle)==1
        generated.write_text('class CBitmap; void ObserveOriginalTextureUnion(const CBitmap&,int);\n'+text.replace(needle,'        ObserveOriginalTextureUnion(bmp,i);\n'+needle),encoding='utf-8')
        sources=[original,generated,ROOT/'tests/matrixgame_union_observer.cpp',ROOT/'port/matrixgame/native-core/texture_mips.cpp'];objects=[]
        for source in sources:
            obj=OUT/(source.stem+'.obj');objects.append(obj)
            cmd=template.replace(str(ROOT/'build/matrixgame/e2e521/source/MatrixGame/src/MatrixCursor.cpp'),str(source)).replace(str(ROOT/'build/matrixgame/e2e521/cursor/windows-frame/MatrixCursor.obj'),str(obj))
            run(cmd,source.stem)
        link=next(c['command'] for c in prior['commands'] if c['label']=='link')
        link=link.replace(str(ROOT/'build/matrixgame/e2e521/cursor/windows-frame/windows-game.exe'),str(OUT/'windows-game.exe')).replace(str(ROOT/'build/matrixgame/e2e521/cursor/windows-frame/windows-game.map'),str(OUT/'windows-game.map'))
        link=link.replace('"'+str(ROOT/'build/matrixgame/e2e521/windows/prepare.obj')+'"','')
        link+=''.join(' "'+str(p)+'"' for p in objects)
        run(link,'link',ROOT/'build/matrixgame/e2e519/original-init-windows')
        p=subprocess.run([OUT/'windows-game.exe',OUT],cwd=prior['cwd'],env=dict(os.environ,MATRIXGAME_UNION_OUTPUT=str(OUT)),capture_output=True,text=True,timeout=240)
        report.update(runtime_exit=p.returncode,stdout=p.stdout,stderr=p.stderr,cwd=prior['cwd']);(OUT/'runtime.log').write_text(p.stdout+p.stderr);print('runtime',p.returncode,(p.stdout+p.stderr)[-4000:],flush=True);assert p.returncode==0
        assert p.stdout.count('original CGame::Init returned: ready=1')==2 and p.stdout.count('actual SDK draws=1008')==2
        report['snapshots']={n:hashlib.sha256((OUT/n).read_bytes()).hexdigest() for n in prior['snapshots']};assert report['snapshots']==prior['snapshots']
        report['union_sha256']={f'union-{i}.dds':hashlib.sha256((OUT/f'union-{i}.dds').read_bytes()).hexdigest() for i in range(6)}
        report['source_sha256']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources+[helper,Path(__file__).resolve()]};report['status']='PASS'
    finally:(OUT/'result.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
