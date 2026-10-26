"""Clean addressed CMake/CTest with assertions and sanitizers; no full regression sweep."""
import json,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/matrixgame/e2e523-presentation/cmake-host'
def main():
    OUT.mkdir(parents=True,exist_ok=True);result={'status':'INCOMPLETE','commands':[]};target='/mnt/d/repos/space_rangers_hd_a_war_apart_port/build/matrixgame/e2e523-presentation/cmake-host'
    try:
        for label,cmd in [('configure',['wsl','--exec','cmake','-S','/mnt/d/repos/space_rangers_hd_a_war_apart_port/port/matrixgame/native-engine','-B',target,'-G','Unix Makefiles','-DBUILD_TESTING=ON','-DCMAKE_BUILD_TYPE=Debug','-DCMAKE_CXX_FLAGS_DEBUG=-O1 -g -UNDEBUG','-DCORE_SANITIZERS=ON']),('build',['wsl','--exec','cmake','--build',target,'--target','matrixgame-native-presentation-test','-j','4']),('ctest',['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1','ctest','--test-dir',target,'--output-on-failure','-R','^matrixgame-native-presentation-contract$'])]:
            started=time.monotonic();p=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');(OUT/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8');result['commands'].append({'label':label,'command':cmd,'exit':p.returncode,'seconds':time.monotonic()-started});print(label,p.returncode,(p.stdout+p.stderr)[-2500:],flush=True);assert p.returncode==0
        result['status']='PASS'
    finally:(OUT/'result.json').write_text(json.dumps(result,indent=2)+'\n')
if __name__=='__main__':main()
