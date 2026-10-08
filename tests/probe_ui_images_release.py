#!/usr/bin/env python3
"""Read-only M21 inventory for image references in SRHD DAT containers."""
from __future__ import annotations
import argparse, json, pathlib, re, struct, zlib
from collections import Counter

RECORD=158
BLOCK_DAT_SEED_KEY=0xb1e8c689
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
def dat_xor(data, seed):
    out=bytearray(data); state=seed
    for i in range(len(out)):
        state=16807*(state%127773)-2836*(state//127773)
        if state<=0: state+=2147483647
        out[i]^=(state-1)&0xff
    return bytes(out)

def decode_dat(path, seed_key=BLOCK_DAT_SEED_KEY):
    blob=path.read_bytes()
    if len(blob)<16: raise ValueError('DAT header is truncated')
    byte_count=u32(blob)^0x7db6c99d^0xc83fcbf3
    if byte_count!=len(blob)-8: raise ValueError('DAT byte count mismatch')
    expected=u32(blob,8); seed=u32(blob,12)^seed_key
    if seed>=0x80000000: seed-=0x100000000
    encoded=dat_xor(blob[16:],seed)
    if zlib.crc32(encoded)&0xffffffff!=expected: raise ValueError('DAT inner CRC mismatch')
    if encoded.startswith(b'ZL01'):
        if len(encoded)<8: raise ValueError('DAT ZL01 header is truncated')
        wanted=u32(encoded,4); decoded=zlib.decompress(encoded[8:])
        if len(decoded)!=wanted: raise ValueError('DAT ZL01 size mismatch')
        return decoded
    return zlib.decompress(encoded)

def block_entries(decoded):
    def wide(at):
        end=at
        while end+1<len(decoded) and decoded[end:end+2]!=b'\0\0': end+=2
        if end+1>=len(decoded): raise ValueError('unterminated UTF-16 string')
        return decoded[at:end].decode('utf-16le','strict'),end+2
    def block(at,prefix):
        if at+5>len(decoded): raise ValueError('truncated block header')
        sorted_index=decoded[at]; count=struct.unpack_from('<I',decoded,at+1)[0]; at+=5
        if sorted_index not in (0,1) or count>1000000: raise ValueError('invalid block header')
        for _ in range(count):
            if sorted_index:
                if at+8>len(decoded): raise ValueError('truncated sorted block entry')
                at+=8
            if at>=len(decoded): raise ValueError('truncated block entry')
            kind=decoded[at]; at+=1; name,at=wide(at); path=prefix+'/'+name if prefix else name
            if kind==1:
                value,at=wide(at); yield path,name,value
            elif kind==2:
                at=yield from block(at,path)
            elif kind!=0: raise ValueError('unknown block entry kind')
        return at
    yield from block(0,'')

RESOURCE=re.compile(r'(?i)(?:DATA[\\/])?[^\s\[\]{};,"\']+\.(?:bmp|png|jpe?g|psd|gi|gai)')
IMAGE_MODES={'simple':'Simple','trans':'Trans','alpha':'Alpha','gi':'GI','gai':'GAI','anim':'Anim','graphbuf':'GraphBuf'}
def data_entries(decoded):
    def wide(at):
        end=at
        while end+1<len(decoded) and decoded[end:end+2]!=b'\0\0': end+=2
        if end+1>=len(decoded): raise ValueError('unterminated data UTF-16 string')
        return decoded[at:end].decode('utf-16le','strict'),end+2
    def node(at,prefix):
        if at+4>len(decoded): raise ValueError('truncated data node')
        count=u32(decoded,at); at+=4
        if count>1000000: raise ValueError('invalid data node count')
        for _ in range(count):
            if at>=len(decoded): raise ValueError('truncated data entry')
            kind=decoded[at]; at+=1; name,at=wide(at); path=prefix+'/'+name if prefix else name
            if kind==1:
                file_name,at=wide(at); yield path,file_name
            elif kind==2:
                at=yield from node(at,path)
            else: raise ValueError('unknown data entry kind')
        return at
    yield from node(0,'')

def split_image(value):
    first,sep,rest=value.partition(',')
    mode=IMAGE_MODES.get(first.strip().lower())
    if mode: return mode,rest.strip()
    return 'Simple',value.strip()
def scan(path):
    decoded=decode_dat(path); refs=[]
    for block_path,name,value in block_entries(decoded):
        if name.casefold()=='image':
            mode,key=split_image(value)
            refs.append({'container':path.name,'entry':block_path,'name':name,'mode':mode,'resource_key':key,'resource':key,'option':value})
            continue
        context=' '.join((block_path,name,value)); mode=classify(context) or 'Unclassified'
        for found in RESOURCE.findall(value):
            refs.append({'container':path.name,'entry':block_path,'name':name,'mode':mode,'resource_key':found.replace('\\','/'),'resource':found.replace('\\','/') ,'option':value})
    return refs, {'path':str(path),'decoded_bytes':len(decoded),'entries':sum(1 for _ in block_entries(decoded))}
def image_candidates(path):
    try: blob=path.read_bytes()
    except OSError: return []
    result=[]
    for path_name, _, size, _ in entries(blob):
        suffix=pathlib.PurePosixPath(path_name).suffix.lower()
        if suffix in ('.bmp','.png','.jpg','.jpeg','.psd'):
            result.append({'resource':path_name,'extension':suffix,'source_bytes':size})
    return sorted(result,key=lambda item:(item['resource'].lower(),item['source_bytes']))

def locate_resources(data_root, wanted):
    """Return package/source metadata for exactly the config-selected paths.

    This deliberately reads package indexes only.  It neither extracts image bytes
    nor treats an arbitrary image in a package as a configured UI resource.
    """
    remaining={item.casefold() for item in wanted}
    locations={}
    scanned=[]
    for package in sorted(data_root.glob('*.pkg'),key=lambda p:p.name.casefold()):
        scanned.append(package.name)
        if not remaining:
            continue
        for name, _, size, _ in entries(package.read_bytes()):
            key=name.replace('\\','/').casefold()
            if key in remaining:
                locations[key]={'package':package.name,'path':name,'source_bytes':size}
                remaining.remove(key)
    return scanned,locations
def main():
    ap=argparse.ArgumentParser(); ap.add_argument('game_root',type=pathlib.Path); ap.add_argument('--json',type=pathlib.Path); ns=ap.parse_args()
    cfg=ns.game_root/'CFG'; candidates=[cfg/'Main.dat',cfg/'Eng'/'Lang.dat',cfg/'Rus'/'Lang.dat']
    refs=[]; stats=[]
    for p in candidates:
        if p.exists():
            found, detail=scan(p); refs.extend(found); stats.append(detail)
    cache_paths={}
    cache_file=cfg/'CacheData.dat'
    if cache_file.exists():
        for key,file_name in data_entries(decode_dat(cache_file,0xea8f3f37)):
            cache_paths[key.casefold()]=file_name.replace('\\','/')
    for ref in refs:
        key=ref['resource_key'].casefold()
        ref['resource']=cache_paths.get(key,cache_paths.get(key.replace('.','/'),ref['resource_key']))
    refs.sort(key=lambda r:(r['mode'],r['resource'].lower(),r['option'],r['container'],r['entry']))
    packages=[ns.game_root/'DATA'/'common.pkg']
    candidates=[]
    for package in packages:
        if package.exists(): candidates.extend(image_candidates(package))
    candidate_counts=Counter(item['extension'] for item in candidates)
    selected=[]
    for extension in sorted(candidate_counts):
        selected.append(next(item for item in candidates if item['extension']==extension))
    static_refs=[r for r in refs if pathlib.PurePosixPath(r['resource']).suffix.lower() in ('.bmp','.png','.jpg','.jpeg','.psd')]
    static_counts=Counter(r['mode'] for r in static_refs)
    selected_static=[]
    for mode in ('Simple','Trans','Alpha'):
        choices=sorted((r for r in static_refs if r['mode']==mode),key=lambda r:(r['resource'].casefold(),r['resource_key'].casefold(),r['entry'].casefold()))
        if choices: selected_static.append({'mode':mode,'resource':choices[0]['resource'],'resource_key':choices[0]['resource_key'],'option':choices[0]['option'],'entry':choices[0]['entry']})
    modes=('Simple','Trans','Alpha')
    unresolved_refs=sorted(({'mode':r['mode'],'resource_key':r['resource_key'],'entry':r['entry'],'option':r['option']} for r in refs if r['mode'] in modes and r['resource']==r['resource_key']),key=lambda r:(r['mode'],r['resource_key'].casefold(),r['entry'].casefold()))
    selected_paths=[item['resource'] for item in selected_static]
    scanned_packages,locations=locate_resources(ns.game_root/'DATA',selected_paths)
    for item in selected_static:
        item['source_location']=locations.get(item['resource'].casefold())
    mode_ref_counts=Counter(r['mode'] for r in refs if r['mode'] in modes)
    unresolved_by_mode=Counter(r['mode'] for r in unresolved_refs)
    result={'containers':stats,'cache_entries':len(cache_paths),'references':refs,'counts':dict(sorted(Counter(r['mode'] for r in refs).items())),'static_counts':dict(sorted(static_counts.items())),'static_mode_reference_counts':dict(sorted(mode_ref_counts.items())),'selected_static_resources':selected_static,'unresolved_static_refs':unresolved_refs,'unresolved_static_counts':dict(sorted(unresolved_by_mode.items())),'release_static_presence':{mode: any(r['mode']==mode for r in static_refs) for mode in modes},'packages_scanned_for_selected_resources':scanned_packages,'unique_resources':len({r['resource'].lower() for r in refs}),'release_presence':bool(refs),'status':'PRESENT' if refs else 'NOT_PRESENT','unclassified_candidate_counts':dict(sorted(candidate_counts.items())),'selected_unclassified_candidates':selected}
    print(json.dumps(result,ensure_ascii=False,indent=2))
    if ns.json: ns.json.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__': main()
