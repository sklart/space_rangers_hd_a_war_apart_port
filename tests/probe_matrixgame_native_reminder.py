"""Independent original SRemindCore timing/order oracle and native lifecycle runner."""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
from probe_matrixgame_native_vector import fixture, storage


def expected():
    state=0x73521;active=[];current=None;gtime=ctime=0;times=[0]*12;trace=bytearray();events=0
    def draw():
        nonlocal state
        state=(state*1664525+1013904223)&0xffffffff
        return state
    def down(index):
        nonlocal current
        if index in active:
            at=active.index(index)
            if current==index:current=active[at+1] if at+1<len(active) else None
            active.remove(index)
    for step in range(5000):
        op,index,value=draw()%8,draw()%12,draw()
        if op<4:
            if index not in active:active.append(index)
            times[index]=gtime+value%(401 if step%7 else 40001)
        elif op==4:down(index)
        elif active:
            ms=value%101
            if current is None:current=active[0]
            gtime+=ms
            while gtime>ctime:
                checks=max(1,ms//10);ctime+=checks*10
                for _ in range(checks):
                    at=active.index(current)+1
                    current=active[at] if at<len(active) else active[0]
                    if gtime>times[current]:
                        trace+=struct.pack('<IIqqq',1,current,gtime,ctime,times[current]);events+=1
                        down(current)
                        if current is None:current=active[0] if active else None
                        if current is None:break
                if not active:break
        trace+=struct.pack('<IIqqII',0,step,gtime,ctime,current if current is not None else 0xffffffff,len(active))
        trace+=b''.join(struct.pack('<I',n) for n in active)+struct.pack('<12q',*times)
    return bytes(trace),events


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--binary',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True);p.add_argument('--reference-binary',type=Path)
    p.add_argument('--runner-binary',type=Path);p.add_argument('--qemu-sysroot',type=Path)
    a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
    shape=out/'geometry.strg';shape.write_bytes(storage(fixture()))
    if os.name=='nt':ctypes.windll.kernel32.SetErrorMode(0x8003)
    command=[str(a.binary.resolve())]
    if a.runner_binary:command+=['-L',str(a.qemu_sysroot.resolve()),str(a.runner_binary.resolve())]
    result=subprocess.run(command+[str(out),str(shape)],capture_output=True,text=True,timeout=90)
    print(result.stdout,end='');print(result.stderr,end='');result.check_returncode()
    wanted,events=expected();actual=(out/'reminder-trace.bin').read_bytes()
    assert actual==wanted,('trace mismatch',len(actual),len(wanted),next((i for i,(x,y) in enumerate(zip(actual,wanted)) if x!=y),None))
    control=json.loads((Path(__file__).parent/'fixtures/matrixgame/reminder-x86-control.json').read_text())
    assert len(wanted)==control['trace_bytes'] and events==control['cleanup_callbacks']
    assert hashlib.sha256(wanted).hexdigest()==control['trace_sha256']
    proof={'scope':'RESOURCE SCHEDULER; NO ENGINE/GPU EXECUTION','operations':5000,'cleanup_callbacks':events,
           'trace_bytes':len(actual),'trace_sha256':hashlib.sha256(actual).hexdigest()}
    if a.reference_binary:
        reference=out/'original';reference.mkdir(exist_ok=True)
        result=subprocess.run([str(a.reference_binary.resolve()),str(reference)],capture_output=True,text=True,timeout=90)
        print(result.stdout,end='');print(result.stderr,end='');result.check_returncode()
        assert (reference/'reminder-trace.bin').read_bytes()==wanted
        control=subprocess.run([str(a.reference_binary.resolve()),'--self-delete-false'],capture_output=True,text=True,timeout=30)
        assert control.returncode!=0,'original negative lifecycle control unexpectedly passed'
        unlink=subprocess.run([str(a.reference_binary.resolve()),'--unlink-self'],capture_output=True,text=True,timeout=30)
        assert unlink.returncode!=0,'original unlink-self control unexpectedly passed'
        proof.update(original_trace='PASS',original_self_delete_false_exit=control.returncode,original_unlink_self_exit=unlink.returncode)
    (out/'oracle.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps(proof))


if __name__=='__main__':main()
