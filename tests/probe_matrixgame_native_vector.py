"""Independent full VO payload/draw-order oracle, controlled failures and optional real PKG."""
import argparse
import copy
import hashlib
import json
import mmap
import os
from pathlib import Path
import struct
import subprocess
from probe_matrixgame_native_storage import parse_storage
from probe_ui_images_release import entries as package_entries, payload as package_payload


def u32(value): return struct.pack('<I', value & 0xffffffff)
def blob(value): return u32(len(value)) + value
def text(value): return value.encode('utf-16-le', 'surrogatepass')


def canonical(records):
    columns = {(r['name'], c['name']): c['arrays'] for r in records for c in r['columns']}
    def data(t, c): return columns.get((t, c), [b''])[0]
    def names(t, c): return columns.get((t, c), [])
    def numbers(t, c): return [v[0] for v in struct.iter_unpack('<I', data(t, c))]
    output = bytearray(b'VOG1')
    for table, column in [('verts','data'), ('idxs','data'), ('frames','data'), ('frames','verts'),
                          ('frames','unions'), ('frames','edges'), ('unions','data'),
                          ('anims','frames'), ('matrices','data')]:
        output += blob(data(table, column))
    surfaces = names('surfs', 'texs')
    output += u32(len(surfaces)) + b''.join(blob(n) for n in surfaces)
    properties = list(zip(names('properties','name'), names('properties','value'), strict=True))
    output += u32(len(properties)) + b''.join(blob(n) + blob(v) for n, v in properties)
    anims = list(zip(names('anims','name'), numbers('anims','id'), numbers('anims','disp'), numbers('anims','cnt'), strict=True))
    output += u32(len(anims)) + b''.join(blob(n) + u32(i) + u32(d) + u32(c) for n,i,d,c in anims)
    matrices = list(zip(names('matrices','name'), numbers('matrices','id'), numbers('matrices','disp'), strict=True))
    output += u32(len(matrices)) + b''.join(blob(n) + u32(i) + u32(d) for n,i,d in matrices)
    simple = data('simple','data')
    output += u32(bool(simple))
    if simple:
        nt, nv = struct.unpack_from('<ii', simple)
        assert len(simple) == 48 + nt*12 + nv*12
        output += blob(simple[:48]) + blob(simple[48:48+nt*12]) + blob(simple[48+nt*12:])
    frames = list(struct.iter_unpack('<10f6i', data('frames','data')))
    unions = list(struct.iter_unpack('<6i', data('unions','data')))
    frame_unions = numbers('frames','unions')
    output += u32(len(frames))
    for frame in frames:
        start, count = frame[10:12]
        # Original CVectorObject::Draw iterates surface first, then frame groups.
        selected = [unions[i] for s in range(len(surfaces)) for i in frame_unions[start:start+count] if unions[i][0] == s]
        output += u32(len(selected))
        for surface, base, minimum, count, triangles, first in selected:
            output += struct.pack('<IiIIII', surface, base, -minimum if minimum < 0 else first*3,
                                  triangles*3, 0 if minimum < 0 else minimum, count)
    return bytes(output)


def storage(records):
    body = u32(len(records))
    for table, columns in records.items():
        body += text(table+'\0') + u32(len(columns))
        for name, (kind, arrays) in columns.items():
            stride = (4,4,1,4,8,2)[kind]
            payload, entries = bytearray(), bytearray()
            for array in arrays:
                assert len(array)%stride == 0
                entries += struct.pack('<III',12+len(payload),len(array)//stride,len(array)//stride)
                payload += array
            value = struct.pack('<III',12+len(payload),len(arrays),stride) + payload + entries
            body += text(name+'\0') + u32(kind) + blob(value)
    return b'STRG' + u32(0) + body


def fixture():
    identity = struct.pack('<16f', *[float(i%5 == 0) for i in range(16)])
    points = [(-1.,-1.,0.),(1.,-1.,0.),(0.,1.,0.)]
    frame = struct.pack('<10f6i',-1,-1,0,1,1,1,0,0,0,2,0,3,0,1,3,0)
    return {
        'verts': {'data': (2, [b''.join(struct.pack('<8f',*p,0,0,1,0,0) for p in [(9.,9.,9.),*points])])},
        'idxs': {'data': (2, [struct.pack('<6H',0,1,2,0,1,2)])},
        'frames': {'data': (2,[frame*2]), 'verts': (2,[struct.pack('<3I',1,2,3)]),
                   'unions': (2,[struct.pack('<3I',0,1,0)]), 'edges': (2,[struct.pack('<2i8B',32,64,1,2,3,4,5,6,7,8)])},
        'unions': {'data': (2,[struct.pack('<12i',1,1,0,3,1,0,0,1,-3,3,1,99)])},
        'surfs': {'texs': (5,[text('texture_a'),text('texture_b')])},
        'anims': {'name': (5,[text('ход 🚀')]), 'id': (1,[u32(7)]), 'disp': (1,[u32(0)]),
                  'cnt': (1,[u32(2)]), 'frames': (2,[struct.pack('<4i',0,50,1,-100)])},
        'matrices': {'name': (5,[text('ствол 🚀')]), 'id': (1,[u32(0)]), 'disp': (1,[u32(0)]), 'data': (2,[identity*2])},
        'properties': {'name': (5,[text('light'),text('dup'),text('dup')]),
                       'value': (5,[text('0,1,2'),text('a'),text('b')])},
        'simple': {'data': (2,[struct.pack('<2i10f3i9f',1,3,-1,-1,0,1,1,1,0,0,0,2,0,1,2,*sum((list(p) for p in points),[]))])},
    }


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--binary', type=Path, required=True);p.add_argument('--output', type=Path, required=True)
    p.add_argument('--package', type=Path);p.add_argument('--runner-binary', type=Path);p.add_argument('--qemu-sysroot', type=Path)
    a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
    inputs=out/'inputs';inputs.mkdir(exist_ok=True);actual=out/'native';actual.mkdir(exist_ok=True)
    cases=[]
    for compressed in (False,True):
        raw=storage(fixture())
        if compressed:
            import zlib
            packed=zlib.compress(raw[8:]);raw=b'STRG'+u32(1)+b'ZL03'+u32(1)+u32(len(packed))+packed
        name=f'valid-{int(compressed)}.strg';(inputs/name).write_bytes(raw);cases.append((name,canonical(parse_storage(raw,include_payloads=True)['records'])))
    f=fixture()
    for optional in ('anims','matrices','properties','simple'): del f[optional]
    f['frames']['edges']=(2,[b''])
    raw_frame=bytearray(f['frames']['data'][1][0])
    for at in (48,112):struct.pack_into('<II',raw_frame,at,0xbaadf00d,0xbaadf00d)
    f['frames']['data']=(2,[bytes(raw_frame)])
    raw=storage(f);name='valid-no-edges.strg';(inputs/name).write_bytes(raw)
    cases.append((name,canonical(parse_storage(raw,include_payloads=True)['records'])))
    variants=[('frames','unions',struct.pack('<3I',0,0xffffffff,0)),
              ('frames','verts',struct.pack('<3I',1,2,4)), ('frames','data',b'x'),
              ('frames','edges',struct.pack('<2i8B',33,64,1,2,3,4,5,6,7,8)),
              ('unions','data',struct.pack('<6i',0,0,-2147483648,3,1,0)),
              ('anims','frames',struct.pack('<4i',0,0,1,50)),
              ('matrices','disp',u32(1)), ('idxs','data',struct.pack('<3H',0,1,65535)),
              ('verts','data',struct.pack('<8f',float('nan'),0,0,0,0,1,0,0)),
              ('properties','value',text('missing paired values')),
              ('surfs','texs',text('embedded\0NUL')),
              ('simple','data',struct.pack('<2i10f',2147483647,3,*([0]*10)))]
    for i,(table,column,value) in enumerate(variants):
        f=copy.deepcopy(fixture())
        kind=f[table][column][0];f[table][column]=(kind,[value])
        name=f'bad-{i}.strg';(inputs/name).write_bytes(storage(f));cases.append((name,None))
    manifest=out/'manifest.txt';manifest.write_text(''.join(n+'\n' for n,_ in cases),encoding='utf-8')
    command=[str(a.binary.resolve())]
    if a.runner_binary: command+=['-L',str(a.qemu_sysroot.resolve()),str(a.runner_binary.resolve())]
    if os.name=='nt':
        import ctypes
        ctypes.windll.kernel32.SetErrorMode(0x8003)
    def run(names, expected, package=None):
        manifest.write_text(''.join(n+'\n' for n in names),encoding='utf-8')
        result=subprocess.run(command+[str(inputs),str(actual),str(manifest)]+([str(package.resolve())] if package else []),capture_output=True,text=True,timeout=240)
        print(result.stdout,end='');print(result.stderr,end='');result.check_returncode()
        rows=[line.split('\t',1)[1] for line in result.stdout.splitlines() if '\t' in line]
        assert len(rows)==len(expected),(len(rows),len(expected))
        hashes=[]
        for i,(row,wanted) in enumerate(zip(rows,expected,strict=True)):
            if wanted is None: assert row.startswith('REJECT'),(names[i],row)
            else:
                assert row=='PASS',(names[i],row)
                received=(actual/f'{i}.native').read_bytes()
                assert received==wanted,(names[i],len(received),len(wanted),next((j for j,(x,y) in enumerate(zip(received,wanted)) if x!=y),None))
                hashes.append(hashlib.sha256(received).hexdigest())
        return hashes
    hashes=run([n for n,_ in cases],[v for _,v in cases])
    real=[]
    if a.package:
        with a.package.open('rb') as stream,mmap.mmap(stream.fileno(),0,access=mmap.ACCESS_READ) as pkg:
            selected=[e for e in package_entries(pkg) if e[0].lower().endswith('.vo')]
            expected=[canonical(parse_storage(package_payload(pkg,e),include_payloads=True)['records']) for e in selected]
            real=run([e[0] for e in selected],expected,a.package)
    report={'scope':'CPU VO DATA/DRAW PLAN; NO GPU EXECUTION/BATTLE','controlled_valid':3,'controlled_rejected':len(variants),
            'controlled_sha256':hashes,'real_vo_count':len(real),'real_combined_sha256':hashlib.sha256(''.join(real).encode()).hexdigest() if real else None}
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))


if __name__=='__main__': main()
