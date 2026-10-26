import pathlib,sys,mmap,json,hashlib
sys.path.insert(0,'tests')
from probe_ui_images_release import entries,payload
from probe_matrixgame_native_storage import parse_storage
p=pathlib.Path('windows/Space Rangers HD A War Apart/DATA/robots.pkg')
with p.open('rb') as f,mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ) as data:
    found=[e for e in entries(data) if e[0].replace('\\','/').lower().endswith('/desert1_3e.cmap')]
    assert len(found)==1,[e[0] for e in found]
    b=payload(data,found[0]);j=parse_storage(b);full=parse_storage(b,include_payloads=True);tables={r['name']:{c['name']:c for c in r['columns']} for r in full['records']};import struct
    union=struct.unpack('<256i',tables['texunions']['Data']['arrays'][0]);cell=next(n for n in union if n>=0);source_id=struct.unpack_from('<i',tables['bottom']['Data']['arrays'][cell])[0];source_name=tables['strings']['String']['arrays'][source_id].decode('utf-16-le').rstrip('\0');names={e[0].replace('\\','/').lower():e for e in entries(data)};key=source_name.replace('\\','/').lower();entry=names.get(key) or names.get(key+'.png');assert entry,source_name;png=payload(data,entry);pathlib.Path('build/matrixgame/e2e521/font/map-first-source.png').write_bytes(png);source={'path':entry[0],'sha256':hashlib.sha256(png).hexdigest(),'id':source_id}
    result={'path':found[0][0],'sha256':hashlib.sha256(b).hexdigest(),'uid':j['uid'],'records':j['records'],'first_source_png':source}
pathlib.Path('build/matrixgame/e2e521/font/map-inventory.json').write_text(json.dumps(result,indent=2))
print(json.dumps({'path':result['path'],'tables':[{ 'name':r['name'],'columns':[{ 'name':c['name'],'arrays':len(c['lengths']),'first_lengths':c['lengths'][:3]} for c in r['columns']]} for r in result['records'] if r['name'] in ('texunions','bottom','bitmaps')]}))
