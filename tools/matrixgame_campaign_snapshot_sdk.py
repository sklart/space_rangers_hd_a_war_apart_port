"""Standard campaign/achievement SDK targets against the preserved E599 dependencies."""
import subprocess, sys
from matrixgame_standard_nro_audit import ROOT, read, sha, check_hashes

def main():
 prior=ROOT/'build/matrixgame/e2e599-campaign-cleanup';dep_path=ROOT/'build/matrixgame/e2e600-campaign-navigation/sdk-dependency/result.json';dep=read(dep_path);assert dep['status'].startswith('PASS');check_hashes(dep['dependency_objects_sha256']);check_hashes(dep['source_sha256']);assert sha(ROOT/dep['archive_path'])==dep['dependency_archive_sha256']
 text=(prior/'fixture/sdk-closure.py').read_text(encoding='utf-8')
 anchor="BASE=ROOT/'build/matrixgame/e2e599-campaign-cleanup/sdk'";assert text.count(anchor)==1;text=text.replace(anchor,"BASE=ROOT/'build/matrixgame/e2e600-campaign-navigation/sdk'")
 anchor="str(archive.parent/'libmatrixgame-rangers-script.a')";assert text.count(anchor)==1;text=text.replace(anchor,anchor+",str(archive.parent/'libmatrixgame-rangers-achievements.a')")
 previous_archive=ROOT/'build/matrixgame/e2e599-campaign-cleanup/sdk-dependency/liboriginal-rangers-host-campaign.a';previous_dep=prior/'sdk-dependency/result.json'
 text=text.replace(repr(str(previous_archive)),repr(str(ROOT/dep['archive_path']))).replace(repr(str(previous_dep)),repr(str(dep_path)))
 out=ROOT/'build/matrixgame/e2e600-campaign-navigation/fixture';out.mkdir(parents=True,exist_ok=True);script=out/'sdk-snapshot-closure.py';assert not script.exists();script.write_text(text,encoding='utf-8')
 subprocess.run([sys.executable,script],check=True)

if __name__=='__main__':main()
