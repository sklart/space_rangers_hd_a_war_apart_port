"""Local JPEG reference export; private PKG pixels remain in an ignored output folder."""
import argparse
import base64
import hashlib
import io
import json
import mmap
from pathlib import Path
import struct
import subprocess
import sys
from PIL import Image,features
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tests'))
from probe_ui_images_release import entries,payload


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--package',type=Path);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--d3dx-binary',type=Path);a=p.parse_args();out=a.output.resolve()
    if out.exists() and any(out.iterdir()) and not (out/'jpeg-oracle.json').is_file():raise ValueError('Unmanaged reference output')
    (out/'input').mkdir(parents=True,exist_ok=True);(out/'pixels').mkdir(exist_ok=True)
    rows=[(c['name'],base64.b64decode(c['jpeg'])) for c in json.loads((ROOT/'tests/fixtures/matrixgame/jpeg-control.json').read_text())['cases'] if c['valid']]
    if a.package:
        with a.package.open('rb') as f,mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ) as m:
            rows += [(e[0],payload(m,e)) for e in entries(m) if e[0].lower().endswith('.jpg')]
    cases=[];names=[]
    for i,(name,b) in enumerate(rows):
        file=out/'input'/f'{i}.jpg';file.write_bytes(b);names.append(str(file))
        image=Image.open(io.BytesIO(b)).convert('RGBA');rgba=image.tobytes();digest=hashlib.sha256(b).hexdigest()
        relative=f'pixels/{digest}.rgba';(out/relative).write_bytes(rgba)
        cases.append(dict(name=name,jpeg_sha256=digest,width=image.width,height=image.height,rgba=relative,rgba_sha256=hashlib.sha256(rgba).hexdigest()))
    d3dx=None
    if a.d3dx_binary:
        manifest=out/'manifest.txt';manifest.write_text(''.join(n+'\n' for n in names),encoding='utf-8')
        result=subprocess.run([str(a.d3dx_binary.resolve()),str(manifest)],capture_output=True,timeout=180)
        (out/'d3dx-stderr.log').write_bytes(result.stderr)
        if result.returncode:raise RuntimeError(('D3DX reference',result.returncode,result.stderr[-2000:]))
        at=0;maximum=0;different=0;total=0;absolute=0
        for case in cases:
            w,h=struct.unpack_from('<2I',result.stdout,at);at+=8;size=w*h*4
            assert (w,h)==(case['width'],case['height'])
            original=result.stdout[at:at+size];at+=size;canonical=(out/case['rgba']).read_bytes();assert len(original)==len(canonical)
            errors=[abs(x-y) for x,y in zip(original,canonical)]
            assert all(v==255 for v in original[3::4]),'JPEG D3DX alpha'
            maximum=max(maximum,max(errors,default=0));different+=sum(bool(v) for v in errors);total+=len(errors);absolute+=sum(errors)
            case['d3dx_max_abs_error']=max(errors,default=0)
        assert at==len(result.stdout)
        d3dx=dict(scope='actual D3DXLoadSurfaceFromFileInMemory SYSTEMMEM; no Draw/Present',max_abs_error=maximum,
            differing_bytes=different,compared_bytes=total,mean_abs_error=absolute/max(1,total))
    result=dict(scope='local controlled+private JPEG canonical references',oracle='Pillow external API; same libjpeg algorithm family as native',
        pillow=Image.__version__,libjpeg_turbo=features.version_feature('libjpeg_turbo'),cases=cases,d3dx=d3dx)
    (out/'jpeg-oracle.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(dict(cases=len(cases),d3dx=d3dx)))


if __name__=='__main__':main()
