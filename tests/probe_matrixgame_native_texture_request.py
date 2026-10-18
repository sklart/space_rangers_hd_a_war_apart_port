"""Independent texture name/sidecar oracle and optional unchanged x86 comparison."""
import argparse
import hashlib
import json
import mmap
from pathlib import Path, PurePosixPath
import re
import struct
import subprocess
from probe_matrixgame_native_bitmap import make_png
from probe_matrixgame_native_texture import dds, storage
from probe_ui_images_release import entries, payload


def expected(name,files):
    fields=name.split('?');plain=fields[0];flags=0
    for flag in fields[1:]:
        if flag.strip()=='Trans':flags|=16
        if flag.strip()=='Alpha':flags|=32
    side=str(PurePosixPath(plain).with_suffix('.txt'))
    if side.lower() in files and not any(s in side for s in ('pinguin.txt','robotarget.txt')):
        b=files[side.lower()][1]
        try:text=b[2:].decode('utf-16-le') if b[:2]==b'\xff\xfe' else b.decode('utf-8')
        except UnicodeError:return (0,0,'',0)
        for key,bit in (('AlphaTest',16),('AlphaBlend',32),('Compressed',128)):
            value=re.search(r'^\s*'+key+r'\s*=\s*([^\r\n]*)',text,re.M)
            if value and value[1].strip():
                if value[1].strip()=='0':flags&=~bit
                else:flags|=bit
    candidates=[plain]+[plain+'.'+ext for ext in ('png','jpg','bmp','dds','strg')]
    path=next((v for v in candidates if v.lower() in files),None)
    if path is None:return (0,flags,'',0)
    b=files[path.lower()][1];wrapped=b[:4]==b'STRG';loaded=0
    if wrapped==bool(flags&128):
        if wrapped:
            from probe_matrixgame_native_storage import parse_storage
            record=parse_storage(b,include_payloads=True)['records'][0];b=record['columns'][0]['arrays'][0]
        if b[:4]==b'DDS ':loaded=max(1,struct.unpack_from('<I',b,28)[0])
        elif b[:8]==b'\x89PNG\r\n\x1a\n':loaded=1
        elif b[:2]==b'\xff\xd8':loaded=1
    return (1,flags,path,loaded)


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--binary',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--reference-binary',type=Path);p.add_argument('--package',type=Path);p.add_argument('--runner-binary',type=Path);p.add_argument('--qemu-sysroot',type=Path);a=p.parse_args()
    out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);files={}
    def add(name,b):
        file=out/name;file.parent.mkdir(parents=True,exist_ok=True);file.write_bytes(b);files[name.lower()]=(name,b)
    png=make_png(7,5,6,8,0)
    for name in ('plain','override','unicode','pinguin','robotarget','mismatch'):
        add('assets/'+name+'.png',png)
    add('assets/override.txt',b'AlphaTest=0\r\nAlphaBlend=1')
    add('assets/unicode.txt',b'\xff\xfe'+'AlphaBlend=0\r\nAlphaTest=1'.encode('utf-16-le'))
    for name in ('pinguin','robotarget'):add('assets/'+name+'.txt',b'\xffbad config')
    add('assets/mismatch.txt',b'Compressed=1')
    add('assets/wrapped.strg',storage(dds('bc1',7,5,3),True));add('assets/wrapped.txt',b'Compressed=1')
    add('assets/competing.png',png);add('assets/competing.dds',dds('bc3',7,5,3))
    suffixes=('','?Trans','?Alpha','? Alpha ? Trans ','?alpha?trans','?Unknown??Alpha')
    names=[f'assets/{n}{s}' for n in ('plain','plain.png','override','unicode','pinguin','robotarget','mismatch','wrapped','competing.png','competing.dds','missing') for s in suffixes]
    names+=['assets/wrapped.strg?Trans','assets/competing.dds?Alpha']
    command=[str(a.binary.resolve())]
    if a.runner_binary:command+=['-L',str(a.qemu_sysroot.resolve()),str(a.runner_binary.resolve())]
    def run(names,files,package=None):
        manifest=out/'requests.txt';manifest.write_text(''.join(n+'\n' for n in names),encoding='utf-8')
        wanted=[expected(n,files) for n in names]
        def execute(command,reference=False):
            result=subprocess.run(command+[str(manifest)]+([str(package.resolve())] if package else []),cwd=out,capture_output=True,timeout=180)
            assert result.returncode==0,(result.returncode,result.stderr[-2000:]);data=result.stdout;at=0;trace=bytearray();loads=0
            for name,(ok,flags,path,loaded) in zip(names,wanted):
                assert len(data)-at>=12,(name,'short header');header=struct.unpack_from('<3I',data,at);at+=12
                actual=data[at:at+header[2]].decode('utf-8');at+=header[2]
                assert header[:2]==(ok,flags) and actual.replace('\\','/').lower()==path.lower(),(name,header,actual,(ok,flags,path))
                canonical=path.replace('\\','/').lower().encode();trace+=struct.pack('<3I',ok,flags,len(canonical))+canonical
                if not reference:
                    actual_load=struct.unpack_from('<I',data,at)[0];at+=4;assert actual_load==loaded,(name,'load',actual_load,loaded);loads+=bool(actual_load)
            assert at==len(data)
            return dict(cases=len(names),metadata_sha256=hashlib.sha256(trace).hexdigest(),loaded=loads if not reference else None)
        native=execute(command)
        if a.reference_binary:
            original=execute([str(a.reference_binary.resolve())],True);assert original['metadata_sha256']==native['metadata_sha256'];native['original_x86_match']=True
        return native
    controlled=run(names,files);real=None
    if a.package:
        with a.package.open('rb') as f,mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ) as m:
            all_files={e[0].lower():(e[0],payload(m,e)) for e in entries(m) if e[0].lower().endswith(('.png','.jpg','.bmp','.dds','.strg','.txt'))}
            texture_names=[n for n,_ in all_files.values() if n.lower().endswith(('.png','.jpg','.bmp','.dds','.strg'))]
            texture_names+=list(dict.fromkeys(str(PurePosixPath(n).with_suffix('')) for n in texture_names))
            names=[n+s for n in texture_names for s in ('','?Trans','?Alpha')]
            real=run(names,all_files,a.package)
    result=dict(scope='Original texture flags, CFile resolution and native texture load; no GPU',controlled=controlled,real=real)
    (out/'oracle.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))


if __name__=='__main__':main()
