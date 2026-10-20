"""Prepare private VO/independent mesh inputs, or compare executed packing transcripts."""
import argparse
import hashlib
import json
import mmap
from pathlib import Path
import struct
from probe_ui_images_release import entries,payload
from probe_matrixgame_native_storage import parse_storage

ROOT=Path(__file__).resolve().parents[1]
def mesh_oracle(raw):
    records=parse_storage(raw,include_payloads=True)['records']
    columns={(r['name'],c['name']):c['arrays'] for r in records for c in r['columns']}
    def data(table,column): return columns[(table,column)][0]
    vertices=data('verts','data');indices=data('idxs','data')
    assert vertices and indices and len(vertices)%32==0 and len(indices)%2==0
    frames=list(struct.iter_unpack('<10f6i',data('frames','data')))
    groups=list(struct.iter_unpack('<6i',data('unions','data')))
    ids=[v[0] for v in struct.iter_unpack('<I',data('frames','unions'))]
    surfaces=len(columns[('surfs','texs')])
    output=bytearray(b'BGM1'+struct.pack('<I',len(vertices))+vertices+struct.pack('<I',len(indices))+indices+struct.pack('<I',len(frames)))
    draws=0
    for frame in frames:
        start,count=frame[10:12]
        selected=[groups[i] for surface in range(surfaces) for i in ids[start:start+count] if groups[i][0]==surface]
        output+=struct.pack('<I',len(selected));draws+=len(selected)
        for surface,base,minimum,vertices_count,triangles,first in selected:
            output+=struct.pack('<iiIIII',surface,base,-minimum if minimum<0 else first*3,triangles*3,0 if minimum<0 else minimum,vertices_count)
    return bytes(output),dict(vertices=len(vertices)//32,indices=len(indices)//2,frames=len(frames),draws=draws)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--compare',type=Path,action='append')
    args=parser.parse_args();out=args.output.resolve()
    if args.compare:
        assert len(args.compare)>=2
        values=[p.read_bytes() for p in args.compare]
        assert all(v==values[0] for v in values),'Packing transcript mismatch'
        assert values[0][:4]==b'BVB1'
        report=dict(status='PASS',scope='Executed live-buffer bytes/ranges only; no GPU draw/battle',
                    executions=len(values),bytes=len(values[0]),sha256=hashlib.sha256(values[0]).hexdigest())
        out.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report));return
    assert args.package and out.is_relative_to(ROOT/'build')
    if out.exists() and any(out.iterdir()): raise ValueError('Use fresh private fixture output')
    out.mkdir(parents=True,exist_ok=True)
    with args.package.open('rb') as stream,mmap.mmap(stream.fileno(),0,access=mmap.ACCESS_READ) as archive:
        candidates=sorted((e for e in entries(archive) if e[0].lower().endswith('.vo')),key=lambda e:(e[2],e[0]))
        # Three actual assets across resource sizes; keep original per-source
        # BigVB/BigIB limits and require nonempty renderable geometry.
        wanted=[len(candidates)//4,len(candidates)//2,len(candidates)-1]
        result=[];used=set()
        for start in wanted:
            for index in range(start,-1,-1):
                entry=candidates[index]
                if entry[0] in used: continue
                raw=payload(archive,entry)
                try: oracle,counts=mesh_oracle(raw)
                except (KeyError,AssertionError,struct.error): continue
                if counts['vertices']*32>8*1024*1024 or counts['indices']*2>1024*1024 or not counts['draws']:continue
                name=f'model-{len(result)}';(out/(name+'.vo')).write_bytes(raw);(out/(name+'.mesh')).write_bytes(oracle)
                used.add(entry[0]);result.append(dict(name=name,source=entry[0],source_sha256=hashlib.sha256(raw).hexdigest(),oracle_sha256=hashlib.sha256(oracle).hexdigest(),**counts));break
        assert len(result)==3
    (out/'inputs.json').write_text(json.dumps({'scope':'Private original resources; do not commit payloads','models':result},indent=2)+'\n')
    print(json.dumps(result))
if __name__=='__main__': main()
