"""Actual HAL oracle: enable/mask states have no effect on single-sample pixels."""
import json
import subprocess
from pathlib import Path
from matrixgame_single_sample_checks import ROOT,BASE,sha


def main():
    out=BASE/'windows-reference';out.mkdir(parents=True,exist_ok=True)
    vs=r'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat'
    exe=out/'single-sample.exe';script=out/'build.cmd'
    script.write_text(f'call "{vs}" -no_logo -arch=x86 -host_arch=x64 && cl /nologo /EHsc /std:c++20 /O2 /UNDEBUG "{ROOT/"tests/probe_matrixgame_single_sample.cpp"}" /Fo"{out.as_posix()}/" /Fe"{exe}" /link d3d9.lib user32.lib\n')
    report={'status':'INCOMPLETE','commands':[]}
    try:
        for label,cmd in [('build',['cmd.exe','/d','/c',str(script)]),('runtime',[str(exe),str(out/'pixels.bgra')])]:
            p=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
            (out/(label+'.log')).write_text(log,encoding='utf-8');report['commands'].append({'label':label,'command':cmd,'exit':p.returncode})
            print(label,p.returncode,log,flush=True);assert p.returncode==0
            if label=='runtime':report['stdout']=log
        report['pixels_sha256']=sha(out/'pixels.bgra');report['binary_sha256']=sha(exe)
        report['source_sha256']={name:sha(ROOT/name) for name in ('tests/probe_matrixgame_single_sample.cpp','tools/matrixgame_single_sample_reference.py')}
        report['status']='PASS; ACTUAL WINDOWS HAL SINGLE-SAMPLE DRAW READBACK'
    finally:(out/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':main()
