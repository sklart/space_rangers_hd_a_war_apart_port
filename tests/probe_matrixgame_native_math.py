"""Compare original D3DX9 and native Math3D traces; fixture contains no game assets."""
import argparse
import ctypes
import hashlib
import json
import math
import mmap
import os
from pathlib import Path
import struct
import subprocess
import sys
import zlib

FIXTURE=Path(__file__).parent/'fixtures/matrixgame/math3d-x86-control.txt.zlib'


def compare(actual, expected, tolerance=0.00002):
    if len(actual)<len(expected): raise AssertionError('short Math3D trace')
    maximum=absolute=0.0
    for index,(a,b) in enumerate(zip(actual,expected)):
        ak,av=a.split();bk,bv=b.split()
        if ak!=bk: raise AssertionError(f'trace kind mismatch at {index}')
        if ak=='I':
            if int(av)!=int(bv): raise AssertionError(f'integer mismatch at {index}: {av} != {bv}')
        else:
            av,bv=float(av),float(bv)
            if not math.isfinite(av) or not math.isfinite(bv): raise AssertionError('nonfinite trace')
            error=abs(av-bv);scaled=error/max(1,abs(bv))
            maximum=max(maximum,scaled);absolute=max(absolute,error)
            if scaled>tolerance: raise AssertionError(f'float mismatch at {index}: {av} != {bv}; scaled={scaled}')
    return {'compared_values':len(expected),'maximum_scaled_error':maximum,'maximum_absolute_error':absolute}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--package',type=Path)
    parser.add_argument('--entry')
    parser.add_argument('--reference',type=Path,help='Complete original trace, including real-data values when supplied')
    parser.add_argument('--record-control',type=Path,help='Record asset-free original x86 fixture')
    parser.add_argument('--runner-binary',type=Path)
    parser.add_argument('--qemu-sysroot',type=Path)
    args=parser.parse_args()
    if bool(args.package)!=bool(args.entry): parser.error('--package/--entry must be paired')
    output=args.output.resolve()
    command=[str(args.binary.resolve())]
    if args.runner_binary:
        if not args.qemu_sysroot: parser.error('--runner-binary requires --qemu-sysroot')
        command+=['-L',str(args.qemu_sysroot.resolve()),str(args.runner_binary.resolve())]
    command.append(str(output))
    if args.package: command += [str(args.package.resolve()),args.entry]
    if os.name=='nt': ctypes.windll.kernel32.SetErrorMode(0x0001|0x0002|0x8000)
    result=subprocess.run(command,capture_output=True,text=True,timeout=120)
    print(result.stdout,end='');print(result.stderr,end='',file=sys.stderr);result.check_returncode()
    rows=[json.loads(line) for line in result.stdout.splitlines() if line.startswith('{')]
    trace=(output/'math-trace.txt').read_text().splitlines()
    if args.record_control:
        if args.package or rows[-1]['backend']!='D3DX9-x86': parser.error('only the asset-free original x86 run may record a fixture')
        data=('\n'.join(trace)+'\n').encode()
        args.record_control.parent.mkdir(parents=True,exist_ok=True)
        args.record_control.write_bytes(zlib.compress(data,9))
        proof={'producer':'pinned original Math3D.cpp/Math3D.hpp + Microsoft D3DX9 x86',
            'scope':'controlled inputs only; no game assets',
            'binary_sha256':hashlib.sha256(args.binary.read_bytes()).hexdigest(),
            'test_source_sha256':hashlib.sha256(Path(__file__).with_name('test_matrixgame_native_math.cpp').read_bytes().replace(b'\r\n',b'\n')).hexdigest(),
            'test_source_hash_format':'SHA256 LF-normalized UTF-8 source',
            'trace_sha256':hashlib.sha256(data).hexdigest(),**rows[-1]}
        args.record_control.with_suffix('.json').write_text(json.dumps(proof,indent=2)+'\n')
        print(json.dumps({'recorded_original_control':proof}));return
    blob=zlib.decompress(FIXTURE.read_bytes())
    metadata=json.loads(FIXTURE.with_suffix('.json').read_text())
    assert hashlib.sha256(blob).hexdigest()==metadata['trace_sha256']
    expected=blob.decode().splitlines()
    assert sum(line.startswith('F ') for line in expected)==metadata['float_values']
    assert sum(line.startswith('I ') for line in expected)==metadata['integer_values']
    metrics=compare(trace,expected)
    proof={'scope':'Math3D CPU contract only',**rows[-1],'control':metrics}
    if args.reference:
        original=args.reference.read_text().splitlines()
        if len(trace)!=len(original): raise AssertionError('full original trace length mismatch')
        proof['full_reference_control']=compare(trace[:len(expected)],original[:len(expected)])
        if args.package:
            proof['full_reference_geometry']=compare(trace[len(expected):],original[len(expected):],0.00005)
    if args.package:
        real=rows[0]
        count=real['real_triangle_hits']
        assert len(trace)-len(expected)==count*3
        tail=trace[len(expected):]
        analytic_error=distance_error=0.0
        for index,line in enumerate(tail):
            kind,value=line.split();assert kind=='F'
            desired=32 if index%3==0 else 1/3
            error=abs(float(value)-desired)
            assert error<=0.00005*max(1,desired)
            if index%3: analytic_error=max(analytic_error,error)
            else: distance_error=max(distance_error,error)
        from probe_ui_images_release import entries, payload
        from probe_matrixgame_native_storage import parse_storage, fnv
        with args.package.open('rb') as stream,mmap.mmap(stream.fileno(),0,access=mmap.ACCESS_READ) as package:
            selected=next(row for row in entries(package) if row[0]==args.entry)
            source=payload(package,selected)
        graph=parse_storage(source,include_payloads=True)
        def data(name):
            record=next(row for row in graph['records'] if row['name']==name)
            return next(row for row in record['columns'] if row['name']=='data')['arrays'][0]
        vertices,indices=data('verts'),data('idxs')
        assert len(vertices)%32==0 and len(indices)%6==0
        assert real['real_vertices']==len(vertices)//32
        assert real['vertices_fnv32']==fnv(vertices) and real['indices_fnv32']==fnv(indices)
        proof['real']={**real,'source_sha256':hashlib.sha256(source).hexdigest(),
            'entry':args.entry,'analytic_barycentric_error':analytic_error,'ray_distance_error':distance_error}
    else: assert len(trace)==len(expected)
    (output/'oracle.json').write_text(json.dumps(proof,indent=2)+'\n')
    print(json.dumps({'math3d_oracle':'PASS',**proof}))


if __name__=='__main__': main()
