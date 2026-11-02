"""Package exactly one private diagnostic NRO after current software gates pass."""
import argparse,json,struct,subprocess,sys
from pathlib import Path
from matrixgame_draw_budget_checks import ROOT,sha
BASE=ROOT/'build/matrixgame/e2e544-smoke'
PACKAGE=ROOT/'dist/matrixgame-e2e527-diag'
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def verify_nro(data,icon,nacp,expected_title='MatrixGame diagnostic'):
 assert data[0x10:0x14]==b'NRO0','NRO magic'
 size=struct.unpack_from('<I',data,0x18)[0];assert 0x80<=size<=len(data)
 previous=0
 for i in range(3):
  off,length=struct.unpack_from('<II',data,0x20+i*8)
  assert off%0x1000==0 and off>=previous and off+length<=size,'NRO segment bounds'
  previous=off+length
 assert data[size:size+4]==b'ASET','asset header'
 assert struct.unpack_from('<I',data,size+4)[0]==0
 sections=[]
 for i,expected in enumerate((icon,nacp,None)):
  off,length=struct.unpack_from('<QQ',data,size+8+i*16)
  if expected is None:assert length==0,'unexpected external RomFS dependency';continue
  assert off>=56 and size+off+length<=len(data)
  assert data[size+off:size+off+length]==expected,'embedded asset differs'
  sections.append(dict(offset=size+off,size=length))
 assert sections[0]['offset']+sections[0]['size']<=sections[1]['offset']
 assert len(nacp)==0x4000 and expected_title.encode('utf-8') in nacp,'unexpected diagnostic NACP title'
 return dict(code_size=size,segments=3,embedded_icon=sections[0],embedded_nacp=sections[1],romfs_required=False)
def main():
 global BASE,PACKAGE
 ap=argparse.ArgumentParser();ap.add_argument('--devkitpro',type=Path,default=Path('C:/devkitPro'))
 ap.add_argument('--base',type=Path,default=BASE);ap.add_argument('--output',type=Path,default=PACKAGE)
 ap.add_argument('--name',default='matrixgame-e2e527-diag.nro');ap.add_argument('--audit',type=Path,default=ROOT/'tools/matrixgame_smoke_audit.py')
 ap.add_argument('--title',default='MatrixGame diagnostic')
 ap.add_argument('--readme',type=Path,default=ROOT/'docs/matrixgame-e2e527-first-run.md');args=ap.parse_args()
 BASE=args.base.resolve();PACKAGE=args.output.resolve()
 assert BASE.is_relative_to(ROOT/'build') and PACKAGE.is_relative_to(ROOT/'dist')
 assert Path(args.name).name==args.name and args.name.endswith('.nro')
 subprocess.run([sys.executable,str(args.audit.resolve())],cwd=ROOT,check=True)
 sdk=read(BASE/'sdk/result.json');metadata=read(BASE/'sdk/native-diagnostic-build.json')
 elf=ROOT/sdk['elf_path'];assert sha(elf)==sdk['elf_sha256']
 fonts=[BASE/'sdk/debug-font/original-font.bin',*[BASE/f'sdk/text-font/font{i}.bin' for i in range(4)]]
 image=elf.read_bytes();assets={}
 for font in fonts:
  content=font.read_bytes();assert content and content in image,('font absent from ELF',font)
  assets[str(font.relative_to(ROOT))]=sha(font)
 symbols=(BASE/'sdk/symbols.log').read_text(encoding='utf-8')
 assert '_binary_original_font_bin_start' in symbols
 nacptool=args.devkitpro/'tools/bin/nacptool.exe';elf2nro=args.devkitpro/'tools/bin/elf2nro.exe';icon=args.devkitpro/'libnx/default_icon.jpg'
 assert icon.read_bytes().startswith(b'\xff\xd8')
 PACKAGE.mkdir(parents=True,exist_ok=True);nro=PACKAGE/args.name;nacp=PACKAGE/'control.nacp'
 if nro.exists():raise RuntimeError('Diagnostic NRO already exists; inspect it instead of silently creating another build')
 commands=[[str(nacptool),'--create',args.title,'SRHD AWA port','E2E-5.27',str(nacp)],
           [str(elf2nro),str(elf),str(nro),'--icon='+str(icon),'--nacp='+str(nacp)]]
 for command in commands:subprocess.run(command,cwd=ROOT,check=True)
 validation=verify_nro(nro.read_bytes(),icon.read_bytes(),nacp.read_bytes())
 resources=[]
 for source,destination in [('windows/Space Rangers HD A War Apart/DATA/robots.pkg','DATA/robots.pkg'),
                            ('upstream/matrixgame/MatrixGame/CFG/robots/data.txt','CFG/robots/data.txt'),
                            ('upstream/matrixgame/MatrixGame/CFG/robots/iface.txt','CFG/robots/iface.txt')]:
  path=ROOT/source;assert path.is_file(),source
  fixture=ROOT/'build/matrixgame/e2e519/skin-borrow-windows/runtime'/destination
  assert sha(path)==sha(fixture),('resource differs from real Init fixture',source)
  resources.append(dict(source=str(path),destination='sdmc:/switch/matrixgame-diag/game/'+destination,size=path.stat().st_size,sha256=sha(path),included_in_package=False))
 readme=args.readme.resolve();assert readme.is_file()
 (PACKAGE/'README.md').write_bytes(readme.read_bytes())
 (PACKAGE/'build-inputs.json').write_bytes((BASE/'sdk/native-diagnostic-build.json').read_bytes())
 (PACKAGE/'resources.json').write_text(json.dumps(resources,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 manifest=dict(status='PACKAGED; SWITCH HARDWARE NOT RUN / HARDWARE_PENDING',build_id=metadata['build_id'],
  source_sha256=metadata['source_sha256'],parent=metadata['parent'],nro_sha256=sha(nro),nro_bytes=nro.stat().st_size,
  elf_sha256=sha(elf),embedded_fonts_sha256=assets,nro_validation=validation,
  tools_sha256={str(p):sha(p) for p in (elf2nro,nacptool,icon,Path(__file__))},
  sdk_commands=sdk['commands'],package_commands=commands,resources=resources,
  files_sha256={p.name:sha(p) for p in sorted(PACKAGE.iterdir()) if p.is_file() and p.name not in ('manifest.json','SHA256SUMS.txt')},
  hardware_runs=0,commit=False,push=False)
 (PACKAGE/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 (PACKAGE/'SHA256SUMS.txt').write_text(''.join(sha(p)+'  '+p.name+'\n' for p in sorted(PACKAGE.iterdir()) if p.is_file() and p.name!='SHA256SUMS.txt'),encoding='utf-8')
 print(json.dumps(dict(path=str(nro),sha256=sha(nro),size=nro.stat().st_size),indent=2))
if __name__=='__main__':main()
