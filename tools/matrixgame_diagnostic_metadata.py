"""Fingerprint diagnostic source inputs; binary SHA is computed separately at runtime/package time."""
import argparse,hashlib,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--engine-source',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);ap.add_argument('--devkitpro',type=Path,required=True);ap.add_argument('--deko-library',choices=('deko3d','deko3dd'),default='deko3d');ap.add_argument('--rangers-host',action='store_true');ap.add_argument('--rangers-archive',type=Path,action='append',default=[]);ap.add_argument('--rangers-manifest',type=Path);ap.add_argument('--build-type',default='');ap.add_argument('--cxx-flags',default='');ap.add_argument('--c-flags',default='');args=ap.parse_args()
 files={str(p.relative_to(ROOT)).replace('\\','/'):sha(p) for p in sorted((ROOT/'port/matrixgame').rglob('*')) if p.is_file() and p.suffix in ('.cpp','.hpp','.h','.inc','.inl','.cmake','.glsl','.txt')}
 for p in [*sorted((ROOT/'tools').glob('matrixgame_*.py')),ROOT/'tests/probe_matrixgame_native_entry.cpp',ROOT/'tests/matrixgame_native_diagnostic_main.cpp']:files[str(p.relative_to(ROOT)).replace('\\','/')]=sha(p)
 engine=json.loads((args.engine_source/'native-engine-source.json').read_text());git=r'C:\Program Files\Git\cmd\git.exe'
 parent=subprocess.check_output([git,'rev-parse','HEAD'],cwd=ROOT,text=True).strip()
 desc=list((args.devkitpro/'msys2/var/lib/pacman/local').glob('deko3d-*/desc'));assert len(desc)==1,desc
 text=desc[0].read_text().splitlines();version=text[text.index('%VERSION%')+1]
 nx_desc=list((args.devkitpro/'msys2/var/lib/pacman/local').glob('libnx-*/desc'));assert len(nx_desc)==1,nx_desc
 nx_text=nx_desc[0].read_text().splitlines();nx_version=nx_text[nx_text.index('%VERSION%')+1]
 manifest={'parent':parent,'own_source_sha256':files,'generated_original_sha256':engine['files_sha256'],'deko_package_version':version,'deko_package_record_sha256':sha(desc[0]),'libnx_package_version':nx_version,'libnx_package_record_sha256':sha(nx_desc[0]),'libnx_archive_sha256':sha(args.devkitpro/'libnx/lib/libnx.a'),'deko_library':args.deko_library,'deko_archive_sha256':sha(args.devkitpro/'libnx/lib'/('lib'+args.deko_library+'.a'))}
 manifest['diagnostic_variants']={'matrixgame-native-diagnostic':{'session_mode':'quick-four-frames','exit':'B or Plus','frame_target':4,'cooperative_deadline_us':120000000},'matrixgame-native-interactive':{'session_mode':'interactive','exit':'Minus+Plus','frame_target':None,'minimum_frames':4,'init_deadline_us':120000000,'loop_deadline_us':120000000,'compile_define':'MATRIXGAME_DIAGNOSTIC_MANUAL=1'}}
 for variant in manifest['diagnostic_variants'].values():variant['frame_measure']='completed visible map presents; loading transition excluded'
 if args.rangers_host:
  assert len(args.rangers_archive)==3 and args.rangers_manifest and args.rangers_manifest.is_file()
  manifest['rangers_host']={'enabled':True,'archives_sha256':{str(p):sha(p) for p in args.rangers_archive},'dependency_manifest':str(args.rangers_manifest),'dependency_manifest_sha256':sha(args.rangers_manifest)}
 manifest['compiler_profile']={'build_type':args.build_type,'cxx_flags':args.cxx_flags,'c_flags':args.c_flags}
 encoded=json.dumps(manifest,sort_keys=True,separators=(',',':')).encode();digest=hashlib.sha256(encoded).hexdigest();build_id=parent[:12]+'-'+digest[:20]
 content=f'''// Generated from exact current source inputs. This is NOT an ELF/NRO digest.
#pragma once
namespace matrixgame_diagnostic_build {{
inline constexpr char Id[]="{build_id}";
inline constexpr char SourceSha256[]="{digest}";
inline constexpr char DekoPackage[]="{version}";
inline constexpr char LibnxPackage[]="{nx_version}";
inline constexpr char DekoLibrary[]="{args.deko_library}";
inline constexpr bool RangersHostEnabled={'true' if args.rangers_host else 'false'};
}}
'''
 args.output.parent.mkdir(parents=True,exist_ok=True)
 if not args.output.is_file() or args.output.read_text()!=content:args.output.write_text(content,encoding='utf-8')
 manifest.update(source_sha256=digest,build_id=build_id)
 args.output.with_suffix('.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
 print(build_id)
if __name__=='__main__':main()
