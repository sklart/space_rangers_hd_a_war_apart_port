"""Clean CMake builds and addressed CTest only; never produces NRO."""
import argparse,json,os,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/e2e522'
def main():
    ap=argparse.ArgumentParser();ap.add_argument('kind',choices=('host','sdk','tests'));kind=ap.parse_args().kind;out=BASE/('clean-'+kind+'-release');out.mkdir(parents=True,exist_ok=True);report={'status':'INCOMPLETE','commands':[]}
    def run(cmd,label):
        t=time.monotonic();p=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace',env=dict(os.environ,DEVKITPRO='C:/devkitPro',DEVKITA64='C:/devkitPro/devkitA64'))
        (out/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8');report['commands'].append({'label':label,'command':cmd,'exit':p.returncode,'seconds':time.monotonic()-t});print(label,p.returncode,(p.stdout+p.stderr)[-4000:],flush=True);assert p.returncode==0
    try:
        if kind=='host':
            target='/mnt/d/repos/space_rangers_hd_a_war_apart_port/build/matrixgame/e2e522/clean-host-release';run(['wsl','--exec','cmake','-S','/mnt/d/repos/space_rangers_hd_a_war_apart_port/port/matrixgame/native-core','-B',target,'-G','Unix Makefiles','-DCMAKE_BUILD_TYPE=Release','-DBUILD_TESTING=OFF','-DCORE_SANITIZERS=OFF'],'configure');run(['wsl','--exec','cmake','--build',target,'--target','matrixgame-native-core','-j','4'],'build')
        elif kind=='tests':
            target='/mnt/d/repos/space_rangers_hd_a_war_apart_port/build/matrixgame/e2e522/clean-tests-release'
            run(['wsl','--exec','cmake','-S','/mnt/d/repos/space_rangers_hd_a_war_apart_port/port/matrixgame/native-engine','-B',target,'-G','Unix Makefiles','-DCMAKE_BUILD_TYPE=Debug','-DCMAKE_CXX_FLAGS_DEBUG=-O1 -g -UNDEBUG','-DBUILD_TESTING=ON','-DCORE_SANITIZERS=ON','-DMATRIXGAME_BC1_UNION_DIR=/mnt/d/repos/space_rangers_hd_a_war_apart_port/build/matrixgame/e2e522/windows-frame'],'configure')
            run(['wsl','--exec','cmake','--build',target,'--target','matrixgame-native-bc1','matrixgame-native-bc1-owner','matrixgame-native-map-queries','matrixgame-native-original-unions','-j','4'],'build')
            run(['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1','ctest','--test-dir',target,'--output-on-failure','-R','^matrixgame-native-(bc1|bc1-owner|map-queries|original-unions)$'],'ctest')
        else:
            cmd=json.loads((ROOT/'build/matrixgame/e2e521/font/cmake-configure.json').read_text());cmd[cmd.index('-B')+1]=str(out);run(cmd,'configure');run([cmd[0],'--build',str(out),'--target','matrixgame-native-entry','-j','4'],'build')
        report['status']='PASS'
    finally:(out/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
