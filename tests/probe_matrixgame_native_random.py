"""Independent MSVC stream oracle for the pinned MatrixGame random wrappers."""
import argparse
import ctypes
import hashlib
import json
import math
import os
from pathlib import Path
import struct
import subprocess
import sys


def expected(threads):
    result=bytearray();state=1;values=0
    def draw():
        nonlocal state
        state=(state*214013+2531011)&0xffffffff
        return (state>>16)&32767
    def f32(value):return struct.unpack('<f',struct.pack('<f',value))[0]
    def integer(value):
        nonlocal values
        result.extend(struct.pack('<I',value&0xffffffff));values+=1
    def f64(value):
        nonlocal values
        result.extend(struct.pack('<d',value));values+=1
    def real32(value):
        nonlocal values
        result.extend(struct.pack('<f',value));values+=1
    def rnd(start,end):return ((draw()*(1.0/32767))*abs(end-start))+start
    def rnd_float():return f32(f32(draw())/f32(2147483645))
    for _ in range(32):integer(draw())
    seeds=list(range(256))+[0x7fffffff,0x80000000,0xffffffff,0x12345678]
    for seed in seeds:
        state=seed
        for _ in range(1024):integer(draw())
        state=seed^0xa5a5a5a5
        for i in range(128):
            integer(-1000+i+draw()%(1778-2*i));integer(77+draw()%1);integer(-321+draw()%1222)
            f64(rnd_float());f64(-5.25+rnd_float()*(7.75-(-5.25)));f64(rnd(-1234,5678))
            real32(f32(rnd(0,1000-i)))
            real32(f32(f32(rnd(0,2*(i-64)))-f32(i-64)))
            integer(math.floor(rnd(0,int(float(i*19+1)-0.55))+0.5));integer(math.floor(rnd(0,int(-37-0.55))+0.5))
    main_state=state
    for thread in range(threads):
        state=1;integer(draw());state=71+thread
        for _ in range(1024):integer(draw())
    state=main_state;integer(draw())
    return bytes(result),values


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--runner-binary',type=Path);parser.add_argument('--qemu-sysroot',type=Path)
    args=parser.parse_args();out=args.output.resolve();command=[str(args.binary.resolve())]
    if args.runner_binary:
        if not args.qemu_sysroot:parser.error('runner requires sysroot')
        command+=['-L',str(args.qemu_sysroot.resolve()),str(args.runner_binary.resolve())]
    command.append(str(out))
    if os.name=='nt':ctypes.windll.kernel32.SetErrorMode(0x0001|0x0002|0x8000)
    run=subprocess.run(command,capture_output=True,text=True,timeout=120)
    print(run.stdout,end='');print(run.stderr,end='',file=sys.stderr);run.check_returncode()
    row=json.loads(run.stdout.strip());actual=(out/'random-trace.bin').read_bytes();reference,count=expected(row['threads'])
    assert row['seeds']==260 and row['values']==count
    if actual!=reference:
        position=next((i for i,(a,b) in enumerate(zip(actual,reference)) if a!=b),min(len(actual),len(reference)))
        raise AssertionError(f'RNG transcript differs at byte {position}; actual bytes={len(actual)}, expected={len(reference)}')
    digest=hashlib.sha256(actual).hexdigest()
    if row['threads']==4:
        assert digest=='2005e75f84d8a3ed429f34ceb38da48b4be9ae47250b009bd4690cf6bedef1e5' # Recorded original MSVC x86 trace.
    proof={'scope':'RNG CPU stream only','sha256':digest,'bytes':len(actual),**row}
    (out/'oracle.json').write_text(json.dumps(proof,indent=2)+'\n')
    print(json.dumps({'independent_random_oracle':'PASS',**proof}))


if __name__=='__main__':main()
