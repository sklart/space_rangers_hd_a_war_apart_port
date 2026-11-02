"""Standard campaign owner target with the separately updated original SDK archive."""
import subprocess,sys
from matrixgame_standard_nro_audit import ROOT,read,check_hashes,sha
if __name__=='__main__':
 dep_path=ROOT/'build/matrixgame/e2e598-campaign/sdk-dependency/result.json';dep=read(dep_path);assert dep['status'].startswith('PASS');check_hashes(dep['dependency_objects_sha256']);check_hashes(dep['source_sha256']);new_archive=ROOT/dep['archive_path'];assert sha(new_archive)==dep['dependency_archive_sha256']
 text=(ROOT/'tools/matrixgame_government_sdk.py').read_text(encoding='utf-8')
 for before,after in (('e2e594-government-session','e2e598-campaign'),('matrixgame-rangers-government','matrixgame-rangers-campaign'),('RangersGovernmentSession','RangersCampaignSession'),('government','campaign'),('GOVERNMENT','CAMPAIGN')):
  assert before in text,before;text=text.replace(before,after)
 anchor="command=prior['commands'][0]['command'][:];command[command.index('-B')+1]=str(BASE)";assert text.count(anchor)==1
 text=text.replace(anchor,anchor+"\n command=[('-DBRIDGE_RANGERS_HOST_ARCHIVE='+"+repr(str(new_archive))+") if s.startswith('-DBRIDGE_RANGERS_HOST_ARCHIVE=') else ('-DMATRIXGAME_RANGERS_DEPENDENCY_MANIFEST='+"+repr(str(dep_path))+") if s.startswith('-DMATRIXGAME_RANGERS_DEPENDENCY_MANIFEST=') else s for s in command]")
 before="['-Wl,--whole-archive',str(archive),'-Wl,--no-whole-archive']";assert text.count(before)==1
 text=text.replace(before,"['-Wl,--whole-archive',str(archive),str(archive.parent/'libmatrixgame-rangers-global-ui.a'),str(archive.parent/'libmatrixgame-rangers-script.a'),'-Wl,--no-whole-archive']")
 anchor="args=next(c['command'] for c in prior['commands'] if c['label']=='link-real-API-closure')[:]";assert text.count(anchor)==1
 text=text.replace(anchor,anchor+"\n  assert sum(s.replace('\\\\','/').endswith('/liboriginal-rangers-host-wave-compat.a') for s in args)==1\n  args=["+repr(str(new_archive))+" if s.replace('\\\\','/').endswith('/liboriginal-rangers-host-wave-compat.a') else s for s in args]")
 text='import sys\nsys.path.insert(0,'+repr(str(ROOT/'tools'))+')\n'+text
 out=ROOT/'build/matrixgame/e2e598-campaign/fixture';out.mkdir(parents=True,exist_ok=True);path=out/'sdk-closure.py'
 if path.exists():assert path.read_text(encoding='utf-8')==text
 else:path.write_text(text,encoding='utf-8')
 subprocess.run([sys.executable,path],check=True)
