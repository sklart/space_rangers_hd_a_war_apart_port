"""New cache/original VO/frame integration only; GPU is never emulated."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--binary',type=Path,required=True)
    p.add_argument('--model',type=Path,required=True)
    p.add_argument('--texture',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--qemu',action='store_true')
    p.add_argument('--skin-resources',action='store_true',help='Run new original skin resource/Takt integration, not the older frame contract')
    a=p.parse_args();root=Path(__file__).resolve().parents[1];out=a.output.resolve()
    if not out.is_relative_to(root/'build') or out.exists():raise ValueError('Use a fresh build/ output')
    binary=a.binary.resolve(strict=True);out.mkdir(parents=True)
    def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
    inputs=[];copies=[]
    for source,name in ((a.model,'original.vo'),(a.texture,'material.png'),(a.model,'second.vo')):
        source=source.resolve(strict=True);copy=out/name;copy.write_bytes(source.read_bytes());copy.chmod(0o444)
        copies.append(copy);inputs.append(dict(source=str(source),sha256=sha(source)))
    command=[str(binary),*map(str,copies)]
    if a.skin_resources:
        (out/'bad-material.png').write_bytes(copies[1].read_bytes()[:24])
        (out/'base.png').write_bytes(copies[1].read_bytes())
        (out/'base_gloss.png').write_bytes(copies[1].read_bytes())
    if a.qemu:command=['qemu-aarch64','-L','/usr/aarch64-linux-gnu',*command]
    run=subprocess.run(command,cwd=out,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1',
        'UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'},text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=90)
    (out/'runtime.log').write_text(run.stdout)
    unchanged=all(sha(Path(i['source']))==i['sha256']==sha(c) for i,c in zip(inputs,copies))
    result=dict(status=('SKIN_RESOURCE_RUNTIME_PASS' if a.skin_resources else 'CACHED_FRAME_RUNTIME_PASS') if run.returncode==0 and unchanged else 'FAIL',
        architecture='ARM64/QEMU' if a.qemu else 'host ASan/UBSan',returncode=run.returncode,
        command=command,executable_sha256=sha(binary),inputs=inputs,inputs_unchanged=unchanged,
        original_vo=True,texture='controlled PNG fixture',gpu_executed=False,cgame_init_called=False,hardware_pending=True,
        derived_vo_surface_names_replaced=a.skin_resources,
        cpp_allocation_fault_cases=run.stdout.count('PASS actual C++ allocation failure='))
    (out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    print(run.stdout);print(json.dumps(result,indent=2));return int(result['status']=='FAIL')

if __name__=='__main__':raise SystemExit(main())
