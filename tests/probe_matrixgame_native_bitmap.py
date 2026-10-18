"""Independent PNG fixtures/decoder and streaming CBitmap pixel-byte oracle."""
import argparse
import ctypes
import hashlib
import json
import mmap
import os
from pathlib import Path
import struct
import subprocess
import zlib
from probe_ui_images_release import entries, payload

PASSES=((0,0,8,8),(4,0,8,8),(0,4,4,8),(2,0,4,4),(0,2,2,4),(1,0,2,2),(0,1,1,2))
CHANNELS={0:1,2:3,3:1,4:2,6:4}
def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
def sample(x,y,c,depth):return (x*(31 if depth<16 else 1237)+y*17+c*(71 if depth<16 else 8191))%2**depth


def make_png(w,h,kind,depth,interlace):
    n=CHANNELS[kind];palette=[bytes(((i*29+7)%256,(i*71+19)%256,(i*11+91)%256)) for i in range(min(17,2**depth))] if kind==3 else []
    raw=bytearray()
    for x0,y0,dx,dy in (PASSES if interlace else ((0,0,1,1),)):
        if x0>=w or y0>=h:continue
        for y in range(y0,h,dy):
            values=[((x+3*y)%len(palette) if kind==3 else sample(x,y,c,depth)) for x in range(x0,w,dx) for c in range(n)]
            if depth==16:row=struct.pack('>'+'H'*len(values),*values)
            elif depth==8:row=bytes(values)
            else:
                row=bytearray((len(values)*depth+7)//8)
                for i,v in enumerate(values):row[i*depth//8]|=v<<(8-depth-i*depth%8)
            raw+=b'\0'+row
    out=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',w,h,depth,kind,0,0,interlace))
    if palette:
        out+=chunk(b'PLTE',b''.join(palette))+chunk(b'tRNS',bytes(0 if i==0 else 255 if i==1 else (i*47)%256 for i in range(len(palette))))
    elif kind==0:out+=chunk(b'tRNS',struct.pack('>H',sample(0,0,0,depth)))
    elif kind==2:out+=chunk(b'tRNS',struct.pack('>3H',*(sample(0,0,c,depth) for c in range(3))))
    return out+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b'')


def decode(data):
    if data[:8]!=b'\x89PNG\r\n\x1a\n':raise ValueError('signature')
    at=8;packed=bytearray();palette=[];trans=b'';header=None;ended=False
    while at<len(data):
        if at+12>len(data):raise ValueError('chunk extent')
        count=struct.unpack_from('>I',data,at)[0];kind=data[at+4:at+8];value=data[at+8:at+8+count]
        if len(value)!=count or at+12+count>len(data) or struct.unpack_from('>I',data,at+8+count)[0]!=zlib.crc32(kind+value)&0xffffffff:raise ValueError('CRC')
        at+=12+count
        if kind==b'IHDR':header=struct.unpack('>2I5B',value)
        elif kind==b'IDAT':packed+=value
        elif kind==b'PLTE':palette=[tuple(value[i:i+3]) for i in range(0,len(value),3)]
        elif kind==b'tRNS':trans=value
        elif kind==b'IEND':ended=True;break
    if not ended or not header:raise ValueError('PNG framing')
    w,h,depth,kind,compression,filtering,interlace=header;n=CHANNELS[kind]
    if compression or filtering or depth not in (1,2,4,8,16) or not w or not h or w*h>16777216:raise ValueError('PNG dimensions')
    raw=zlib.decompress(packed);pos=0
    bpp=1 if kind in (0,3) else n;legacy=bytearray(w*h*bpp) if kind!=4 else None;rgba=bytearray(w*h*4)
    for x0,y0,dx,dy in (PASSES if interlace else ((0,0,1,1),)):
        if x0>=w or y0>=h:continue
        pw=(w-x0+dx-1)//dx;stride=(pw*n*depth+7)//8;previous=bytearray(stride);filter_bytes=max(1,(n*depth+7)//8)
        for y in range(y0,h,dy):
            f=raw[pos];pos+=1;row=bytearray(raw[pos:pos+stride]);pos+=stride
            if len(row)!=stride:raise ValueError('PNG row')
            for i in range(stride):
                a=row[i-filter_bytes] if i>=filter_bytes else 0;b=previous[i];c=previous[i-filter_bytes] if i>=filter_bytes else 0
                if f==1:v=a
                elif f==2:v=b
                elif f==3:v=(a+b)//2
                elif f==4:
                    p=a+b-c;d=(abs(p-a),abs(p-b),abs(p-c));v=a if d[0]<=d[1] and d[0]<=d[2] else b if d[1]<=d[2] else c
                elif f==0:v=0
                else:raise ValueError('PNG filter')
                row[i]=(row[i]+v)&255
            previous=row
            for px,x in enumerate(range(x0,w,dx)):
                if depth==16:s=struct.unpack_from('>'+'H'*n,row,px*n*2)
                elif depth==8:s=tuple(row[px*n:px*n+n])
                else:s=((row[px*depth//8]>>(8-depth-px*depth%8))&((1<<depth)-1),)
                v=tuple(c>>8 if depth==16 else c for c in s)
                if kind==3:
                    r,g,b=palette[s[0]];a=trans[s[0]] if s[0]<len(trans) else 255
                elif kind in (0,4):
                    r=g=b=(s[0]*255//((1<<depth)-1)) if depth<8 else v[0]
                    a=v[1] if kind==4 else 0 if trans and s[0]==struct.unpack('>H',trans)[0] else 255
                else:r,g,b=v[:3];a=v[3] if kind==6 else 0 if trans and s==struct.unpack('>3H',trans) else 255
                off=(y*w+x)*4;rgba[off:off+4]=bytes((r,g,b,a))
                if legacy is not None:legacy[(y*w+x)*bpp:(y*w+x+1)*bpp]=bytes(v)
    if pos!=len(raw):raise ValueError('PNG decoded tail')
    return w,h,kind,bytes(legacy) if legacy is not None else None,bytes(rgba)


def transcript(w,h,bpp,fmt,pixels):
    masks=(255,0,0,0) if bpp==1 else (255,65280,16711680,4278190080 if bpp==4 else 0)
    return struct.pack('<10I',1,w,h,bpp,bpp*8,fmt,*masks)+pixels


def check_exports(out):
    crop=bytes((2+x*2)*11+(2+y*2)*3+c*17+7 for y in range(2) for x in range(2) for c in range(4))
    assert decode((out/'rgba.png').read_bytes())[4]==crop
    b=(out/'rgba.dds').read_bytes();head=struct.unpack_from('<32I',b)
    assert len(b)==144 and head[0:6]==(0x20534444,124,0x81007,2,2,16)
    assert head[19:28]==(32,65,0,32,0xff0000,0xff00,0xff,0xff000000,0x1000)
    assert b[128:]==b''.join(bytes((crop[i+2],crop[i+1],crop[i],crop[i+3])) for i in range(0,len(crop),4))
    for name,w,h,bpp,pixels in [('rgba.bmp',2,2,4,crop),('rgb.bmp',5,3,3,bytes((16,32,57))*15)]:
        b=(out/name).read_bytes();offset=struct.unpack_from('<I',b,10)[0];assert b[:2]==b'BM'
        assert struct.unpack_from('<iiHH',b,18)==(w,h,1,bpp*8)
        pitch=(w*bpp+3)&~3
        for y in range(h):
            row=b[offset+(h-1-y)*pitch:offset+(h-1-y)*pitch+w*bpp]
            expected=b''.join(bytes((pixels[i+2],pixels[i+1],pixels[i]))+pixels[i+3:i+bpp] for i in range(y*w*bpp,(y+1)*w*bpp,bpp))
            assert row==expected


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--binary',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--package',type=Path);p.add_argument('--runner-binary',type=Path);p.add_argument('--qemu-sysroot',type=Path)
    p.add_argument('--reference-binary',type=Path);a=p.parse_args()
    out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);inputs=out/'inputs';inputs.mkdir(exist_ok=True)
    cases=[]
    for kind,depths in [(0,(1,2,4,8,16)),(2,(8,16)),(3,(1,2,4,8)),(4,(8,16)),(6,(8,16))]:
        for depth in depths:
            for interlace in (0,1):
                for w,h in ((1,1),(7,5),(16,9)):
                    b=make_png(w,h,kind,depth,interlace);file=inputs/f'{len(cases)}.png';file.write_bytes(b);cases.append((str(file),b,True))
    good=make_png(7,5,6,8,0);bad_crc=bytearray(good);bad_crc[-5]^=1
    invalid=[b'',b'bad PNG',good[:8],good[:-1],good[:len(good)//2],bytes(bad_crc)]
    for w,h in ((8193,1),(8192,8192)):
        invalid.append(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',w,h,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(b''))+chunk(b'IEND',b''))
    for b in invalid:
        file=inputs/f'{len(cases)}.png';file.write_bytes(b);cases.append((str(file),b,False))
    command=[str(a.binary.resolve())]
    if a.runner_binary:command+=['-L',str(a.qemu_sysroot.resolve()),str(a.runner_binary.resolve())]
    if os.name=='nt':ctypes.windll.kernel32.SetErrorMode(0x8003)
    def run(rows,source,package=None):
        manifest=out/'manifest.txt';manifest.write_text(''.join(n+'\n' for n in rows),encoding='utf-8')
        digest=hashlib.sha256();pixels=0;accepted=0
        with (out/'native-stderr.log').open('wb') as err:
            child=subprocess.Popen(command+[str(manifest),str(out)]+([str(package.resolve())] if package else []),stdout=subprocess.PIPE,stderr=err)
            try:
                for index,name in enumerate(rows):
                    b,valid=source(index)
                    wanted=decode(b) if valid else None
                    for force in (False,True):
                        header=child.stdout.read(40)
                        if len(header)!=40:raise AssertionError(('short native header',name,child.poll()))
                        info=struct.unpack('<10I',header);size=info[1]*info[2]*info[3] if info[0] else 0
                        assert size<=64*1024*1024
                        data=bytearray()
                        while len(data)<size:
                            block=child.stdout.read(size-len(data))
                            if not block:raise AssertionError(('short native pixels',name))
                            data+=block
                        expected=bytes(40)
                        if wanted:
                            w,h,kind,legacy,rgba=wanted
                            if force:expected=transcript(w,h,4,1,rgba)
                            elif legacy is not None:expected=transcript(w,h,1 if kind in (0,3) else CHANNELS[kind],2 if kind==3 else 1,legacy)
                        assert header+data==expected,(name,force,info,next((i for i,(x,y) in enumerate(zip(header+data,expected)) if x!=y),None))
                        digest.update(header);digest.update(data);pixels+=size;accepted+=bool(info[0])
                assert not child.stdout.read(1),'native transcript tail'
                assert child.wait(timeout=60)==0,(child.returncode,(out/'native-stderr.log').read_text()[-2000:])
            finally:
                if child.poll() is None:child.terminate();child.wait(timeout=30)
        check_exports(out)
        return {'cases':len(rows),'decoded_variants':accepted,'pixel_bytes_compared':pixels,'transcript_sha256':digest.hexdigest()}
    control=run([n for n,_,_ in cases],lambda i:(cases[i][1],cases[i][2]));real=None;original=None
    recorded=json.loads((Path(__file__).parent/'fixtures/matrixgame/bitmap-x86-control.json').read_text())
    assert control['cases']==recorded['native_cases'] and control['decoded_variants']==recorded['native_variants']
    assert control['pixel_bytes_compared']==recorded['native_pixel_bytes'] and control['transcript_sha256']==recorded['native_trace_sha256']
    if a.reference_binary:
        selected=[(n,b) for n,b,valid in cases if valid]
        manifest=out/'original-manifest.txt';manifest.write_text(''.join(n+'\n' for n,_ in selected),encoding='utf-8')
        result=subprocess.run([str(a.reference_binary.resolve()),str(manifest),str(out)],capture_output=True,timeout=90)
        assert result.returncode==0,(result.returncode,result.stderr[-1000:])
        wanted=bytearray()
        for _,b in selected:
            w,h,kind,legacy,_=decode(b)
            wanted+=transcript(w,h,1 if kind in (0,3) else CHANNELS[kind],2 if kind==3 else 1,legacy) if legacy is not None else bytes(40)
        assert result.stdout==wanted,('original legacy mismatch',len(result.stdout),len(wanted),next((i for i,(x,y) in enumerate(zip(result.stdout,wanted)) if x!=y),None))
        unsupported=next(n for n,b in selected if decode(b)[2]==4)
        false_success=subprocess.run([str(a.reference_binary.resolve()),'--file-result',unsupported],capture_output=True,timeout=30)
        assert false_success.returncode==0 and false_success.stdout==b'1',(false_success.returncode,false_success.stdout)
        original={'classic_cases':len(selected),'trace_sha256':hashlib.sha256(wanted).hexdigest(),'original_file_false_success':True}
        assert original['classic_cases']==recorded['classic_cases'] and original['trace_sha256']==recorded['classic_trace_sha256']
        old_dds=out/'original.dds'
        result=subprocess.run([str(a.reference_binary.resolve()),'--dds',str(out/'rgba.png'),str(old_dds)],capture_output=True,timeout=30)
        assert result.returncode==0 and old_dds.read_bytes()==(out/'rgba.dds').read_bytes()
        original['dds_bytes_sha256']=hashlib.sha256(old_dds.read_bytes()).hexdigest()
    if a.package:
        with a.package.open('rb') as stream,mmap.mmap(stream.fileno(),0,access=mmap.ACCESS_READ) as mapped:
            selected=[e for e in entries(mapped) if e[0].lower().endswith('.png')]
            real=run([e[0] for e in selected],lambda i:(payload(mapped,selected[i]),True),a.package)
    result={'scope':'CBitmap/FilePNG CPU pixels; no GPU or engine execution','controlled':control,'real':real,'original':original}
    (out/'oracle.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))


if __name__=='__main__':main()
