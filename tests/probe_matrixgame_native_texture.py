"""Independent DDS/STRG mip-payload and byte-pitch oracle; no GPU color proof."""
import argparse
import base64
import hashlib
import json
import mmap
from pathlib import Path
import struct
import subprocess
import zlib
from probe_matrixgame_native_bitmap import decode as png_decode, make_png
from probe_matrixgame_native_storage import parse_storage
from probe_ui_images_release import entries, payload
JPEG_ORACLE={}


def jpeg_controls():
    document=json.loads((Path(__file__).parent/'fixtures/matrixgame/jpeg-control.json').read_text())
    rows=[]
    for case in document['cases']:
        b=base64.b64decode(case['jpeg']);rows.append((b,case['valid']))
        if case['valid']:JPEG_ORACLE[hashlib.sha256(b).hexdigest()]=(case['width'],case['height'],base64.b64decode(case['rgba']))
    return rows


def storage(b, compressed=False):
    array=struct.pack('<III',12+len(b),1,1)+b+struct.pack('<III',12,len(b),len(b))
    body=struct.pack('<I',1)+b'0\0\0\0'+struct.pack('<I',1)+b'0\0\0\0'+struct.pack('<II',2,len(array))+array
    if compressed:
        parts=[zlib.compress(body[i:i+65000]) for i in range(0,len(body),65000)]
        body=b'ZL03'+struct.pack('<I',len(parts))+b''.join(struct.pack('<I',len(p))+p for p in parts)
    return b'STRG'+struct.pack('<I',int(compressed))+body


def dds(fmt,w,h,levels=1,padded=False):
    desc={'bc1':(4,0x31545844,0,0,0,0,0),'bc2':(4,0x33545844,0,0,0,0,0),
          'bc3':(4,0x35545844,0,0,0,0,0),'a8':(2,0,8,0,0,0,255),
          'la8':(0x20001,0,16,255,0,0,65280),'l8':(0x20000,0,8,255,0,0,0),
          '565':(0x40,0,16,0xf800,0x7e0,31,0),'5551':(0x41,0,16,0x7c00,0x3e0,31,0x8000),
          '4444':(0x41,0,16,0xf00,0xf0,15,0xf000),'rgb':(0x40,0,24,0xff0000,0xff00,255,0),
          'rgba':(0x41,0,32,255,65280,0xff0000,0xff000000)}[fmt]
    header=[0]*32;header[:8]=[0x20534444,124,0x1007|(8 if padded else 0),h,w,(w*(desc[2]//8)+3)&~3,0,levels]
    header[19:27]=[32,*desc];header[27]=0x1000
    data=bytearray()
    for level in range(levels):
        size=((w+3)//4)*((h+3)//4)*(8 if fmt=='bc1' else 16) if fmt.startswith('bc') else w*h*(desc[2]//8)
        raw=bytes((i*71+level*13+37)%256 for i in range(size))
        if padded and level==0:
            row=w*(desc[2]//8);stride=header[5]
            raw=b''.join(raw[y*row:(y+1)*row]+b'\xcd'*(stride-row) for y in range(h))
        data+=raw;w=max(1,w//2);h=max(1,h//2)
    return struct.pack('<32I',*header)+data


def oracle(b):
    if b[:4]==b'STRG':
        records=parse_storage(b,include_payloads=True)['records']
        b=next(c['arrays'][0] for r in records if r['name']=='0' for c in r['columns'] if c['name']=='0' and c['type']==2)
    if b[:2]==b'\xff\xd8':
        w,h,rgba=JPEG_ORACLE[hashlib.sha256(b).hexdigest()];return 0,[(w,h,w*4,rgba)]
    if b[:8]==b'\x89PNG\r\n\x1a\n':
        w,h,_,_,rgba=png_decode(b);return 0,[(w,h,w*4,rgba)]
    header=struct.unpack_from('<32I',b);_,_,flags,h,w,base_pitch,_,count=header[:8]
    pf,fourcc,bits,*masks=header[20:27];at=128;result=[]
    kind={0:0,0x31545844:1,0x33545844:2,0x35545844:3}[fourcc]
    for level in range(max(count,1)):
        if kind:
            pitch=((w+3)//4)*(8 if kind==1 else 16);size=pitch*((h+3)//4);pixels=b[at:at+size];at+=size
        else:
            source_pitch=base_pitch if level==0 and flags&8 else w*(bits//8)
            pixels=bytearray()
            for y in range(h):
                for x in range(w):
                    pos=at+y*source_pitch+x*(bits//8);value=int.from_bytes(b[pos:pos+bits//8],'little')
                    values=[]
                    for mask in masks:
                        shift=(mask&-mask).bit_length()-1
                        values.append(((value&mask)>>shift)*255/(mask>>shift) if mask else None)
                    r,g,blue,a=values
                    if pf&2:r=g=blue=0
                    if pf&0x20000:g=blue=r
                    if not pf&(1|2) or a is None:a=255
                    pixels+=bytes(int(v+0.5) for v in (r,g,blue,a))
            pitch=w*4;at+=source_pitch*h
        result.append((w,h,pitch,bytes(pixels)));w=max(1,w//2);h=max(1,h//2)
    assert at==len(b)
    return kind,result


def controlled():
    rows=[]
    for fmt in ('bc1','bc2','bc3','a8','la8','l8','565','5551','4444','rgb','rgba'):
        for w,h in ((1,1),(7,5),(16,8)):
            for count in (1,max(w,h).bit_length()):rows.append((dds(fmt,w,h,count),True))
    rows.append((dds('rgb',7,5,3,True),True))
    for b in (dds('bc1',7,5,3),dds('la8',7,5,3),make_png(7,5,6,8,1)):
        rows.extend(((storage(b),True),(storage(b,True),True)))
    good=dds('bc3',7,5,3)
    rows.extend((b,False) for b in (b'',b'bad',good[:127],good[:-1],good+b'junk',storage(b'bad'),storage(storage(good))))
    for at,value in ((4,123),(76,31),(16,0),(12,8193),(28,32),(112,0x200),(24,2),(84,0x30315844)):
        bad=bytearray(good);struct.pack_into('<I',bad,at,value);rows.append((bytes(bad),False))
    for at,value in ((92,0xff00),(100,0x101),(88,15),(104,0)):
        bad=bytearray(dds('rgba',7,5));struct.pack_into('<I',bad,at,value);rows.append((bytes(bad),False))
    rows.extend(jpeg_controls())
    return rows


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--binary',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--package',type=Path);p.add_argument('--runner-binary',type=Path);p.add_argument('--qemu-sysroot',type=Path)
    p.add_argument('--jpeg-oracle-map',type=Path);a=p.parse_args()
    out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);cases=controlled();paths=[]
    if a.jpeg_oracle_map:
        for case in json.loads(a.jpeg_oracle_map.read_text())['cases']:
            rgba=(a.jpeg_oracle_map.parent/case['rgba']).read_bytes()
            assert hashlib.sha256(rgba).hexdigest()==case['rgba_sha256']
            JPEG_ORACLE[case['jpeg_sha256']]=(case['width'],case['height'],rgba)
    for i,(b,_) in enumerate(cases):
        file=out/f'{i}.texture';file.write_bytes(b);paths.append(str(file))
    command=[str(a.binary.resolve())]
    if a.runner_binary:command+=['-L',str(a.qemu_sysroot.resolve()),str(a.runner_binary.resolve())]
    budget_file=out/'jpeg-coefficient-budget.jpg'
    fixture=json.loads((Path(__file__).parent/'fixtures/matrixgame/jpeg-control.json').read_text())
    budget_file.write_bytes(base64.b64decode(fixture['codec_budget_case']))
    budget_result=subprocess.run(command+['--jpeg-codec-limit',str(budget_file)],capture_output=True,timeout=60)
    assert budget_result.returncode==0,('JPEG coefficient budget',budget_result.returncode,budget_result.stderr[-1000:])
    def run(names,source,package=None):
        manifest=out/'manifest.txt';manifest.write_text(''.join(n+'\n' for n in names),encoding='utf-8')
        digest=hashlib.sha256();mips=0;data_bytes=0;formats={}
        with (out/'native-stderr.log').open('wb') as err:
            child=subprocess.Popen(command+[str(manifest)]+([str(package.resolve())] if package else []),stdout=subprocess.PIPE,stderr=err)
            def read(n):
                nonlocal data_bytes
                data=bytearray()
                while len(data)<n:
                    part=child.stdout.read(n-len(data))
                    assert part,('short texture transcript',child.poll(),(out/'native-stderr.log').read_text()[-1000:])
                    data+=part
                digest.update(data);data_bytes+=n;return bytes(data)
            try:
                for i,name in enumerate(names):
                    b,valid=source(i);header=struct.unpack('<3I',read(12))
                    if not valid:assert header==(0,0,0),(name,header);continue
                    kind,levels=oracle(b);assert header==(1,kind,len(levels)),(name,header,kind,len(levels))
                    formats[kind]=formats.get(kind,0)+1;staging=bytearray()
                    for w,h,pitch,data in levels:
                        mips+=1;rows=len(data)//pitch;offset=(len(staging)+255)//256*256;stride=(pitch+31)//32*32
                        info=struct.unpack('<7I',read(28));assert info==(w,h,pitch,len(data),offset,stride,rows),(name,info)
                        assert read(len(data))==data,(name,'mip payload')
                        staging+=bytes(offset-len(staging))
                        staging+=b''.join(data[y*pitch:(y+1)*pitch]+bytes(stride-pitch) for y in range(rows))
                    assert struct.unpack('<I',read(4))[0]==len(staging)
                    assert read(len(staging))==staging,(name,'GPU byte staging')
                assert not child.stdout.read(1)
                assert child.wait(timeout=60)==0,(child.returncode,(out/'native-stderr.log').read_text()[-1000:])
            finally:
                if child.poll() is None:child.terminate();child.wait(timeout=30)
        return dict(cases=len(names),mips=mips,formats=formats,transcript_bytes=data_bytes,sha256=digest.hexdigest())
    controls=run(paths,lambda i:cases[i]);real=None
    if a.package:
        with a.package.open('rb') as f,mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ) as m:
            selected=[e for e in entries(m) if e[0].lower().endswith(('.dds','.strg','.png','.jpg'))]
            real=run([e[0] for e in selected],lambda i:(payload(m,selected[i]),True),a.package)
    result=dict(scope='PNG/JPEG/DDS/STRG mip payload and GPU staging bytes; no GPU execution',controlled=controls,real=real)
    (out/'oracle.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))


if __name__=='__main__':main()
