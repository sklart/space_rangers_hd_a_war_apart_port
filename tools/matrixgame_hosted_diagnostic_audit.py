"""Joint diagnostic NRO provenance and read-only default resource plan."""
import argparse
import json
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_diagnostic_package import verify_nro
from matrixgame_rangers_sound_checks import GAME
BASE=ROOT/'build/matrixgame/e2e575-hosted-diagnostic/sdk'
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
 sdk=read(BASE/'result.json');assert sdk['status'].startswith('PASS') and all(c['exit']==0 for c in sdk['commands'])
 check_hashes(sdk['link_inputs_sha256']);check_hashes(sdk['protected_sha256'])
 for name,value in sdk['artifacts'].items():assert sha(ROOT/name)==value['sha256']
 metadata=read(BASE/'native-diagnostic-build.json');assert sha(BASE/'native-diagnostic-build.json')==sdk['metadata_sha256']
 check_hashes(metadata['own_source_sha256']);check_hashes(metadata['generated_original_sha256'],BASE/'source')
 host=metadata['rangers_host'];assert host['enabled'];check_hashes(host['archives_sha256']);assert sha(Path(host['dependency_manifest']))==host['dependency_manifest_sha256']
 assert 'MATRIXGAME_DIAGNOSTIC_RANGERS_HOST:BOOL=ON' in (BASE/'CMakeCache.txt').read_text()
 dep=read(Path(host['dependency_manifest']));check_hashes(dep['dependency_objects_sha256'])
 symbols=(ROOT/next(c['log'] for c in sdk['commands'] if c['label']=='symbols')).read_text()
 assert not sdk['strong_undefined'] and all(name in symbols for name in sdk['required_linked_symbols'])
 assert 'AArch64' in sdk['elf_header'] and 'DYN (Position-Independent Executable file)' in sdk['elf_header']
 image=(BASE/'matrixgame-native-diagnostic.elf').read_bytes();nro=(BASE/'diagnostic-nro/matrixgame-native-diagnostic.nro').read_bytes()
 assert metadata['build_id'].encode() in nro and b'B.RangersHostSession' in nro and b'D.RangersHostSession' in nro
 fonts=[BASE/'debug-font/original-font.bin',*[BASE/f'text-font/font{i}.bin' for i in range(4)]]
 for path in fonts:assert path.read_bytes() in image and path.read_bytes() in nro
 validation=verify_nro(nro,Path('C:/devkitPro/libnx/default_icon.jpg').read_bytes(),(BASE/'diagnostic-nro/control.nacp').read_bytes())
 # Production host/lifetime code is unchanged from the actual E574 host runs.
 cpu={name:read(ROOT/f'build/matrixgame/e2e574-rangers-host-session/{name}/result.json') for name in ('host','asan')}
 for name,result in cpu.items():
  assert result['status'].startswith('PASS');check_hashes(result['source_sha256']);check_hashes(result['new_objects_sha256']);check_hashes(result['reused_objects_sha256']);check_hashes(result['protected_game_sha256'])
 prefix=read(ROOT/'build/matrixgame/e2e574-rangers-host-session/native-prefix-final/result.json')
 assert prefix['status'].startswith('PASS');check_hashes(prefix['source_sha256']);check_hashes(prefix['objects_sha256']);check_hashes(prefix['archives_sha256']);check_hashes(prefix['native_fixture_sha256'])
 # The default entry uses Russian, no selected mods and a fresh private user root.
 state=ROOT/'build/matrixgame/e2e573-rangers-data/host-final/russian/owned/state.txt'
 lines=state.read_text().splitlines();assert lines[1]=='russian' and lines[2]==''
 resources=[]
 def add(source,destination):
  assert source.is_file(),source
  resources.append({'source':str(source),'destination':destination,'bytes':source.stat().st_size,'sha256':sha(source),'included_in_NRO':False})
 for source,dest in [(GAME/'DATA/robots.pkg','DATA/robots.pkg'),(ROOT/'upstream/matrixgame/MatrixGame/CFG/robots/data.txt','CFG/robots/data.txt'),(ROOT/'upstream/matrixgame/MatrixGame/CFG/robots/iface.txt','CFG/robots/iface.txt')]:add(source,'sdmc:/switch/matrixgame-diag/game/'+dest)
 for relative in ['INSTALL.TXT','INSTALL_RUSSIAN.TXT','CFG/Main.dat','CFG/CacheData.dat','CFG/Rus/Lang.dat','Mods/ModCFG.txt']+[line[2:].replace('\\','/') for line in lines if line.startswith('0:')]:add(GAME/relative,'sdmc:/switch/matrixgame-diag/rangers/'+relative)
 assert len(resources)==26,len(resources)
 plan={'status':'READ-ONLY COPY PLAN; NO FILE TRANSFER PERFORMED','profile':'default Russian/no selected mods; original install package order retained','matrix_root':'sdmc:/switch/matrixgame-diag/game','rangers_root':'sdmc:/switch/matrixgame-diag/rangers','private_user_root':'current run log directory/rangers-user','resource_files':resources,'bytes_for_two_default_roots':sum(r['bytes'] for r in resources),'resources_bundled':False}
 plan_path=args.output.with_name(args.output.stem+'-resources.json');encoded=json.dumps(plan,ensure_ascii=False,indent=2)+'\n'
 if plan_path.exists():assert plan_path.read_text(encoding='utf-8')==encoded
 else:plan_path.write_text(encoded,encoding='utf-8')
 old=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json')
 protected={name:value['sha256'] for name,value in old['protected'].items() if value.get('status')!='UNAVAILABLE'}
 protected.update({name:value['sha256'] for name,value in old['extra_frozen_artifacts_verified'].items()});check_hashes(protected)
 primary=old['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256']
 assert sha(ROOT/'Switch-E2E5-TEST.nro')==old['root_cumulative_NRO_verified_against_documented_hash']
 git='C:/Program Files/Git/cmd/git.exe';pins={}
 for name,pin in (('matrixgame','218115a72fa9c5d43269f4ca84a8596a0bcb82f1'),('cpp','57fa689c630193a66fdea6ca4c79a188814991cd'),('okgf','c01aa7a168a6f1772541501074b7bba1b96550ef')):
  path=ROOT/'upstream'/name;pins[name]=subprocess.check_output([git,'-C',path,'rev-parse','HEAD'],text=True).strip();assert pins[name]==pin and not subprocess.check_output([git,'-C',path,'status','--porcelain'],text=True)
 result={'status':'PASS; JOINT ORIGINAL INIT/DEKO/RANGERS DIAGNOSTIC STANDARD NRO; HARDWARE_PENDING','sdk':sdk,'upstream_pins_clean':pins,'nro_validation':validation,
  'embedded_fonts_sha256':{str(path.relative_to(ROOT)):sha(path) for path in fonts},'resource_plan':str(plan_path),'resource_plan_sha256':sha(plan_path),'resource_files':len(resources),
  'prior_cpu_profiles_verified':{name:result['binary_sha256'] for name,result in cpu.items()},'prior_native_prefix_verified':prefix['binary_sha256'],
  'scope':'SDK compile/link/standard NRO validation and current-source E574 host/lifetime evidence. This combined NRO has not executed; no full successful native Init/map or GPU/audio/input validation.',
  'remaining':['manual execution of combined NRO and precise next Init/map boundary','ARM64/QEMU and Windows execution of new hosted chain','Switch graphics/audio/HID/focus/F9/recovery and first complete native map frames'],
  'Switch':'NOT RUN','commit':False,'push':False,'audit_source_sha256':sha(Path(__file__).resolve())}
 with args.output.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2);stream.write('\n')
 print(result['status']);print(json.dumps(sdk['artifacts'],indent=2));print('resource files',len(resources))
if __name__=='__main__':main()
