#!/usr/bin/env python3
"""Read-only M21 inventory for image references in SRHD DAT containers."""
from __future__ import annotations
import argparse, json, pathlib, struct, zlib
from collections import Counter

RECORD=158
def u32(b, at=0): return struct.unpack_from('<I', b, at)[0]
def text(b): return b.split(b'\0',1)[0].decode('cp1251','replace')
def entries(blob):
    active=set()
    def walk(offset,prefix=''):
        if offset in active or offset+12>len(blob): return
        active.add(offset); size,count,record=struct.unpack_from('<III',blob,offset)
        if record!=RECORD or size<12 or offset+size>len(blob): active.remove(offset); return
        for i in range(count):
            at=offset+12+i*RECORD
            if at+RECORD>len(blob): break
            stored,data_size=struct.unpack_from('<II',blob,at); name=text(blob[at+71:at+134]); kind=u32(blob,at+134); flags=u32(blob,at+142); target=u32(blob,at+150)
            if flags: continue
            path=(prefix+'/' if prefix else '')+name
            if kind==3: yield from walk(target,path)
            else: yield path,kind,data_size,target
        active.remove(offset)
    yield from walk(u32(blob))
def payload(blob, entry):
    _,kind,size,target=entry
    at=target+4
    if kind!=2: return blob[at:at+size]
    out=bytearray(); remain=size
    while remain:
        if at+4>len(blob): return b''
        packed=u32(blob,at); at+=4
        if packed<8 or at+packed>len(blob): return b''
        block=blob[at:at+packed]; at+=packed
        if block[:4]!=b'ZL02': return b''
        want=min(remain,65536)
        if u32(block,4)!=want: return b''
        try: decoded=zlib.decompress(block[8:])
        except zlib.error: return b''
        if len(decoded)!=want: return b''
        out+=decoded; remain-=want
    return bytes(out)
def classify(line):
    lower=line.lower()
    for name in ('graphbuf','anim','gai','alpha','trans','simple'):
        if name in lower: return name.title() if name!='gai' else 'GAI'
    return None
def scan(path):
    blob=path.read_bytes(); refs=[]; entry_count=0; decoded_count=0
    for e in entries(blob):
        entry_count+=1
        if not e[0].lower().endswith(('.txt','.cfg','.ini','.style','.dat')): continue
        data=payload(blob,e)
        if not data: continue
        decoded_count+=1
        for raw in data.decode('cp1251','replace').splitlines():
            mode=classify(raw)
            if mode and ('image' in raw.lower() or ',' in raw): refs.append({'container':path.name,'entry':e[0],'mode':mode,'text':raw.strip()})
    return refs, {'path':str(path),'entries':entry_count,'decoded_config_entries':decoded_count}
def image_candidates(path):
    try: blob=path.read_bytes()
    except OSError: return []
    result=[]
    for path_name, _, size, _ in entries(blob):
        suffix=pathlib.PurePosixPath(path_name).suffix.lower()
        if suffix in ('.bmp','.png','.jpg','.jpeg','.psd'):
            result.append({'resource':path_name,'extension':suffix,'source_bytes':size})
    return sorted(result,key=lambda item:(item['resource'].lower(),item['source_bytes']))
def main():
    ap=argparse.ArgumentParser(); ap.add_argument('game_root',type=pathlib.Path); ap.add_argument('--json',type=pathlib.Path); ns=ap.parse_args()
    cfg=ns.game_root/'CFG'; candidates=[cfg/'Main.dat',cfg/'Eng'/'Lang.dat',cfg/'Rus'/'Lang.dat']
    refs=[]; stats=[]
    for p in candidates:
        if p.exists():
            found, detail=scan(p); refs.extend(found); stats.append(detail)
    refs.sort(key=lambda r:(r['mode'],r['text'],r['container'],r['entry']))
    encrypted=any(item['entries']==0 for item in stats)
    packages=[ns.game_root/'DATA'/'common.pkg']
    candidates=[]
    for package in packages:
        if package.exists(): candidates.extend(image_candidates(package))
    candidate_counts=Counter(item['extension'] for item in candidates)
    selected=[]
    for extension in sorted(candidate_counts):
        selected.append(next(item for item in candidates if item['extension']==extension))
    result={'containers':stats,'references':refs,'counts':dict(sorted(Counter(r['mode'] for r in refs).items())),'unique_resources':len({r['text'] for r in refs}),'release_presence':bool(refs),'status':'UNSUPPORTED_ENCRYPTED_CONFIG' if encrypted else ('PRESENT' if refs else 'NOT_PRESENT'),'unclassified_candidate_counts':dict(sorted(candidate_counts.items())),'selected_unclassified_candidates':selected}
    print(json.dumps(result,ensure_ascii=False,indent=2))
    if ns.json: ns.json.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__': main()
