"""Actual original config and sampler settings consumption; no GPU fixture."""
import argparse
import json
import subprocess
import xml.etree.ElementTree as ET
from pathlib import Path
from matrixgame_native_settings_checks import ROOT,BASE,ENGINE,sha,linux


def main():
    parser=argparse.ArgumentParser();parser.add_argument('kind',choices=('host','arm64','windows'));kind=parser.parse_args().kind
    out=BASE/('config-'+kind);out.mkdir(parents=True,exist_ok=True);result={'status':'INCOMPLETE','commands':[]}
    source=ROOT/'tests/test_matrixgame_native_settings_config.cpp'
    original=[ENGINE/'MatrixGame/src'/name for name in ('MatrixConfig.cpp','MatrixSampleStateManager.cpp')]
    original+=[ROOT/'port/matrixgame/native-engine/platform_3g.cpp']
    before={str(p.relative_to(ROOT)):sha(p) for p in [source,*original,ROOT/'port/matrixgame/native-engine/native_game_settings.hpp']}
    result['source_sha256']=dict(before)
    def run(command,label):
        command=list(map(str,command));p=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
        (out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode})
        print(kind,label,p.returncode,log[-5500:] if p.returncode or label=='runtime' else '',flush=True);assert p.returncode==0,label
        return log
    try:
        if kind=='windows':
            extra=ROOT/'tests/matrixgame_native_settings_link_globals.cpp';result['source_sha256'][str(extra.relative_to(ROOT))]=sha(extra)
            build=ROOT/'build/matrixgame/e2e559-config-tail/windows/build'
            project=ET.parse(build/'matrixgame-original-config-contract.vcxproj').getroot();ns={'ms':'http://schemas.microsoft.com/developer/msbuild/2003'}
            group=next(node for node in project.findall('ms:ItemDefinitionGroup',ns) if 'Release|Win32' in node.get('Condition',''))
            libraries=group.find('ms:Link/ms:AdditionalDependencies',ns).text.split(';')
            libraries=[str((build/Path(name)).resolve()) if name.startswith('core\\') else name for name in libraries]
            result['libraries_sha256']={name:sha(Path(name)) for name in libraries if Path(name).is_file()}
            previous=(BASE/'windows/build.cmd').read_text();old=str(ROOT/'tests/test_matrixgame_native_settings.cpp')
            command=previous.replace('cl /nologo','cl /nologo /MD').replace(old,str(source)).replace(str(BASE/'windows/settings.exe'),str(out/'settings-config.exe')).replace((BASE/'windows').as_posix()+'/',out.as_posix()+'/')
            command=command.rstrip()+' /Gy /Gw '+' '.join('"'+str(p)+'"' for p in [*original,extra])+' /link /OPT:REF '+' '.join('"'+name+'"' for name in libraries)+'\n'
            script=out/'build.cmd';script.write_text(command);run(['cmd.exe','/d','/c',script],'build')
            runtime=[out/'settings-config.exe',out/'observed.bin'];exe=out/'settings-config.exe'
        else:
            previous=json.loads((BASE/kind/'result.json').read_text());command=previous['commands'][0]['command'][:]
            command[command.index(linux(ROOT/'tests/test_matrixgame_native_settings.cpp'))]=linux(source)
            exe=out/'settings-config';command[command.index('-o')+1]=linux(exe)
            command+=['-ffunction-sections','-fdata-sections','-Wl,--gc-sections',*map(linux,original)]
            run(command,'build');runtime=previous['commands'][-1]['command'][:];runtime[-2:]=[linux(exe),linux(out/'observed.bin')]
        result['stdout']=run(runtime,'runtime');result['binary_sha256']=sha(exe);result['observations_sha256']=sha(out/'observed.bin')
        assert before=={str(p.relative_to(ROOT)):sha(p) for p in [source,*original,ROOT/'port/matrixgame/native-engine/native_game_settings.hpp']}
        result['status']='PASS; FULL ORIGINAL SETTINGS METHODS CPU ONLY'
    finally:(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':main()
