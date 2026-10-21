"""Run the actual CVectorObject CPU entry on read-only private model copies."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--binary',type=Path,required=True)
    p.add_argument('--model',type=Path,action='append',required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--qemu',action='store_true',help='Linux ARM64 CPU execution, not Horizon')
    a=p.parse_args();out=a.output.resolve()
    root=Path(__file__).resolve().parents[1]
    if not out.is_relative_to(root/'build') or out.exists():
        raise ValueError('Use a fresh isolated output under build/')
    out.mkdir(parents=True)
    def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()
    inputs=[];copies=[]
    for i,source in enumerate(a.model):
        source=source.resolve(strict=True);copy=out/f'model-{i}.vo'
        copy.write_bytes(source.read_bytes());copy.chmod(0o444);copies.append(copy)
        inputs.append(dict(source=str(source),sha256=digest(source),bytes=source.stat().st_size))
    short=out/'short.vo';short.write_bytes(copies[0].read_bytes()[:47]);short.chmod(0o444)
    command=[str(a.binary.resolve(strict=True)),*map(str,copies),str(short)]
    if a.qemu:command=['qemu-aarch64','-L','/usr/aarch64-linux-gnu',*command]
    env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'}
    run=subprocess.run(command,cwd=out,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=90)
    (out/'runtime.log').write_text(run.stdout)
    unchanged=all(digest(Path(i['source']))==i['sha256'] and digest(c)==i['sha256'] for i,c in zip(inputs,copies))
    result=dict(status='HOST_RUNTIME_PASS' if run.returncode==0 and unchanged else 'FAIL',
        returncode=run.returncode,command=command,inputs=inputs,inputs_unchanged=unchanged,
        executable_sha256=digest(a.binary.resolve()),hardware_pending=True,
        cgame_init_called=False,game_loop_called=False,gpu_executed=False,
        observed_call_log=[line for line in run.stdout.splitlines() if line.startswith('CALL ')],
        reached_methods=(['CVectorObject::StaticInit','CVectorObject::CVectorObject','Load','LoadSpecial(OLF_NO_TEX)',
            'EnumFrameVerts','GetBound','GetAnimById/Name','GetMatrixById/Name','Unload','~CVectorObject'] if run.returncode==0 else []))
    (out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    print(run.stdout);print(json.dumps(result,indent=2))
    return 0 if result['status']=='HOST_RUNTIME_PASS' else 1


if __name__=='__main__':raise SystemExit(main())
