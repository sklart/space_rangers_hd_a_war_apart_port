"""Run the integrated cache/3G CPU prerequisite on isolated read-only inputs."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--binary',type=Path,required=True)
    p.add_argument('--model',type=Path,required=True)
    p.add_argument('--texture',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--qemu',action='store_true')
    a=p.parse_args();root=Path(__file__).resolve().parents[1];out=a.output.resolve()
    if not out.is_relative_to(root/'build') or out.exists():raise ValueError('Use a fresh build/ output')
    binary=a.binary.resolve(strict=True);out.mkdir(parents=True)
    def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
    inputs=[];copies=[]
    for source,name in [(a.model,'original.vo'),(a.texture,'material.png')]:
        source=source.resolve(strict=True);copy=out/name
        copy.write_bytes(source.read_bytes());copy.chmod(0o444);copies.append(copy)
        inputs.append(dict(source=str(source),sha256=sha(source),bytes=source.stat().st_size))
    short=out/'short.vo';short.write_bytes(copies[0].read_bytes()[:47]);short.chmod(0o444)
    data=copies[1].read_bytes();assert data.startswith(b'\x89PNG\r\n\x1a\n')
    width,height=struct.unpack_from('>II',data,16)
    command=[str(binary),*map(str,copies),str(short),str(width),str(height)]
    if a.qemu:command=['qemu-aarch64','-L','/usr/aarch64-linux-gnu',*command]
    env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'}
    run=subprocess.run(command,cwd=out,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=90)
    (out/'runtime.log').write_text(run.stdout)
    unchanged=all(sha(Path(i['source']))==i['sha256']==sha(c) for i,c in zip(inputs,copies))
    result=dict(status='CPU_INIT_CACHE_PASS' if run.returncode==0 and unchanged else 'FAIL',
        returncode=run.returncode,command=command,executable_sha256=sha(binary),inputs=inputs,
        original_vo_resource=True,texture_scope='controlled PNG fixture; not full CSkinManager/GPU',
        inputs_unchanged=unchanged,hardware_pending=True,cgame_init_called=False,gpu_executed=False,
        allocation_fault_cases=run.stdout.count('PASS actual CHeap calloc failure='))
    (out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    print(run.stdout);print(json.dumps(result,indent=2))
    return int(result['status']=='FAIL')


if __name__=='__main__':raise SystemExit(main())
