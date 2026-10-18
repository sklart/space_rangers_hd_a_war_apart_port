"""Generate asset-free JPEG controls. Requires Pillow only when regenerating."""
import argparse
import base64
import io
import json
from pathlib import Path
from PIL import Image,features


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);a=p.parse_args();rows=[]
    for mode in ('RGB','L'):
        for w,h in ((1,1),(7,5),(17,9)):
            channels=3 if mode=='RGB' else 1
            source=Image.frombytes(mode,(w,h),bytes((x*37+y*19+c*73)%256 for y in range(h) for x in range(w) for c in range(channels)))
            for progressive in (False,True):
                for subsample in ((0,1,2) if mode=='RGB' else (0,)):
                    encoded=io.BytesIO();source.save(encoded,format='JPEG',quality=87,subsampling=subsample,progressive=progressive)
                    b=encoded.getvalue();pixels=Image.open(io.BytesIO(b)).convert('RGBA').tobytes()
                    rows.append(dict(name=f'{mode}-{w}x{h}-{int(progressive)}-{subsample}',valid=True,width=w,height=h,
                        jpeg=base64.b64encode(b).decode(),rgba=base64.b64encode(pixels).decode()))
    good=base64.b64decode(rows[-1]['jpeg']);bad=[b'',b'bad JPEG',b'\xff\xd8',good[:20],good[:-2],good[:len(good)//2],good+b'tail']
    large=bytearray(good)
    for marker in (b'\xff\xc0',b'\xff\xc2'):
        at=large.find(marker)
        if at>=0:large[at+7:at+9]=(8193).to_bytes(2,'big');break
    bad.append(bytes(large))
    encoded=io.BytesIO();Image.new('CMYK',(7,5),(21,42,63,84)).save(encoded,format='JPEG');bad.append(encoded.getvalue())
    rows.extend(dict(name=f'invalid-{i}',valid=False,jpeg=base64.b64encode(b).decode()) for i,b in enumerate(bad))
    coefficient=bytearray(base64.b64decode(next(c['jpeg'] for c in rows if c['name']=='RGB-17x9-1-0')))
    at=coefficient.index(b'\xff\xc2');coefficient[at+5:at+9]=(4096).to_bytes(2,'big')*2
    result=dict(scope='Controlled generated inputs; no game assets',encoder='Pillow '+Image.__version__,
        oracle='Pillow external API path; same JPEG algorithm family as native',libjpeg=features.version('jpg'),
        libjpeg_turbo=features.version_feature('libjpeg_turbo'),cases=rows,
        codec_budget_case=base64.b64encode(coefficient).decode())
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(result,indent=2)+'\n')


if __name__=='__main__':main()
