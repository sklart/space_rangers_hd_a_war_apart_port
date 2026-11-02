"""Review saved E591-E610 evidence and package two freshly built E610 NROs."""
import argparse,json,re,shutil,subprocess,zipfile
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_diagnostic_package import verify_nro
GIT='C:/Program Files/Git/cmd/git.exe'

def history(output):
 stages=[]
 for n in range(591,611):
  files=sorted((ROOT/'reports').glob(f'matrixgame-e2e{n}-*'))
  records=[]
  for p in files:
   item={'path':str(p.relative_to(ROOT)),'sha256':sha(p),'bytes':p.stat().st_size}
   if p.suffix=='.json':item['historical_status']=read(p).get('status','NO TOP-LEVEL STATUS')
   records.append(item)
  assert records,n
  stages.append({'stage':f'E{n}','records':records})
 data={'status':'HISTORICAL E591-E610 RECORDS REVIEWED; VERSION SCOPES RETAINED','stages':stages,
  'interpretation':{'E598':'Historical leak failure retained; E599/E601 supply later cleanup evidence','E600':'Partial snapshot/seed limits retained in report; E601 is later host cleanup/seed evidence','E602_E608':'Windows natural defeat and Gov return; victory not proven','E610':'SOFTWARE PASS / HARDWARE PENDING; final gates verified separately'},'runtime_tests_rerun':False}
 with output.open('x',encoding='utf-8') as f:json.dump(data,f,indent=2);f.write('\n')
 print(data['status'])

def package(build,commit,output,readme=None,rangers_root='sdmc:/switch/matrixgame-diag/rangers'):
 build=build.resolve();output=output.resolve();assert build.is_relative_to(ROOT/'build') and output.is_relative_to(ROOT/'dist') and output.suffix=='.zip'
 assert not output.exists();folder=output.with_suffix('');assert not folder.exists()
 gate=read(build/'result.json');metadata=read(build/'native-diagnostic-build.json');assert gate['status'].startswith('PASS') and gate['new_NRO'] and all(c['exit']==0 for c in gate['commands'])
 assert re.fullmatch('[0-9a-f]{40}',commit) and metadata['parent']==commit
 assert subprocess.check_output([GIT,'rev-parse',commit],text=True).strip()==commit
 check_hashes(metadata['own_source_sha256']);check_hashes(metadata['generated_original_sha256'],build/'source');check_hashes(metadata['rangers_host']['archives_sha256']);check_hashes(gate['all_link_inputs_sha256']);check_hashes(gate['protected_previous_NRO_sha256'])
 for name,info in gate['artifacts'].items():assert sha(ROOT/name)==info['sha256']
 variants={};nm='C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe'
 checks={
  'native_hid_poll':'SwitchInput::Poll(', 'native_hold_reset':'SwitchInput::CancelHeldInput(', 'router_warp':'Router::Warp(',
  'rangers_ui_owner':'RangersNativeInput::RangersNativeInput(', 'ui_context':'RangersUiContext::RangersUiContext(',
  'caller_lease':'RangersCallerPresentation::RangersCallerPresentation(', 'native_rgb565_presenter':'NativeRangersCanvas::NativeRangersCanvas(',
  'campaign_session':'RangersCampaignSession::RangersCampaignSession(', 'GR_WinMessage':'GR_Main::GR_WinMessage('}
 folder.mkdir(parents=True)
 for entry,name in (('matrixgame-native-diagnostic','matrixgame-e610-quick-smoke.nro'),('matrixgame-native-interactive','matrixgame-e610-interactive.nro')):
  source=ROOT/gate['variants'][entry]['NRO'];dest=folder/name;shutil.copyfile(source,dest);assert sha(source)==sha(dest)
  elf=build/(entry+'.elf');symbols=subprocess.check_output([nm,'-C',str(elf)],text=True);assert not any(' U ' in line for line in symbols.splitlines())
  present={key:any(needle in line and not re.search(r'\bU\b',line) for line in symbols.splitlines()) for key,needle in checks.items()}
  assert present['native_hid_poll'] and present['router_warp']
  nacp=source.parent/'control.nacp';validation=verify_nro(dest.read_bytes(),Path('C:/devkitPro/libnx/default_icon.jpg').read_bytes(),nacp.read_bytes())
  variants[name]={'sha256':sha(dest),'bytes':dest.stat().st_size,'elf_sha256':sha(elf),'elf_build_id':gate['variants'][entry]['elf_build_id'],'nro_build_id':gate['variants'][entry]['nro_build_id'],'nro_validation':validation,'retained_symbols':present,'entry':'standalone original native Init/Draw; no Campaign/Gov entry','runtime':'HARDWARE PENDING'}
  assert dest.read_bytes()[0x40:0x60].hex()==variants[name]['nro_build_id']
 template=readme or ROOT/'docs/matrixgame-e610-switch-test-readme.ru.md';(folder/'README.ru.md').write_bytes(template.read_bytes())
 dependency=read(ROOT/gate['dependency_result_path']);assert sha(ROOT/gate['dependency_result_path'])==gate['dependency_result_sha256']
 build_info={'source_commit':commit,'build_id':metadata['build_id'],'source_sha256':metadata['source_sha256'],'sdk_versions':{'libnx':metadata['libnx_package_version'],'deko3d':metadata['deko_package_version']},'deko_library':metadata['deko_library'],'dependency_manifest_sha256':gate['dependency_result_sha256'],'original_dependency_sha256':dependency['dependency_archive_sha256'],'commands':gate['commands'],'build_result_sha256':sha(build/'result.json'),'build_metadata_sha256':sha(build/'native-diagnostic-build.json')}
 (folder/'build-info.json').write_text(json.dumps(build_info,indent=2)+'\n',encoding='utf-8')
 external=[]
 for source,dest in (('windows/Space Rangers HD A War Apart/DATA/robots.pkg','game/DATA/robots.pkg'),('upstream/matrixgame/MatrixGame/CFG/robots/data.txt','game/CFG/robots/data.txt'),('upstream/matrixgame/MatrixGame/CFG/robots/iface.txt','game/CFG/robots/iface.txt')):
  p=ROOT/source;external.append({'destination':'sdmc:/switch/matrixgame-diag/'+dest,'bytes':p.stat().st_size,'sha256':sha(p),'included':False})
 manifest={'status':'SOFTWARE PASS / HARDWARE PENDING','source_commit':commit,'build_id':metadata['build_id'],'source_sha256':metadata['source_sha256'],'variants':variants,'external_resources':external,'additional_external_resources':'rangers/install.txt, install_russian.txt, CFG and DATA from the same legal test copy; packages named by install files','hardware_runs':0,'Campaign_Gov_entry':False,'victory':'NOT PROVEN','full_native_UI_pump_invoked':False,'coverage_note':'Standalone calls native HID/MatrixGame input; it does not construct RangersNativeInput/UiContext/Caller/Campaign owners. Retention alone is not execution.','files_sha256':{p.name:sha(p) for p in sorted(folder.iterdir()) if p.is_file()}}
 manifest['rangers_resource_root']=rangers_root
 manifest['additional_external_resources']=rangers_root+'/install.txt, install_russian.txt, CFG and DATA from the same legal test copy; packages named by install files'
 (folder/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 hashes={p.name:sha(p) for p in sorted(folder.iterdir()) if p.is_file()};(folder/'SHA256SUMS.txt').write_text(''.join(digest+'  '+name+'\n' for name,digest in hashes.items()),encoding='utf-8')
 expected={*hashes,'SHA256SUMS.txt'};assert len(expected)==6
 with zipfile.ZipFile(output,'x',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as archive:
  for name in sorted(expected):archive.write(folder/name,name)
 with zipfile.ZipFile(output) as archive:
  assert set(archive.namelist())==expected and archive.testzip() is None
  for name in expected:assert archive.read(name)==(folder/name).read_bytes()
 check_hashes(hashes,folder)
 result={'status':'VERIFIED E610 TWO-NRO PACKAGE; HARDWARE PENDING','source_commit':commit,'build_id':metadata['build_id'],'zip':str(output.relative_to(ROOT)),'zip_sha256':sha(output),'zip_bytes':output.stat().st_size,'files_sha256':{p.name:sha(p) for p in sorted(folder.iterdir())},'variants':variants,'package':str(folder.relative_to(ROOT))}
 with (build/'freeze-package-result.json').open('x',encoding='utf-8') as f:json.dump(result,f,indent=2);f.write('\n')
 print(json.dumps(result,indent=2))

def main():
 parser=argparse.ArgumentParser();sub=parser.add_subparsers(dest='mode',required=True)
 h=sub.add_parser('history');h.add_argument('--output',type=Path,required=True)
 p=sub.add_parser('package');p.add_argument('--build',type=Path,required=True);p.add_argument('--source-commit',required=True);p.add_argument('--output',type=Path,default=ROOT/'dist/matrixgame-e610-switch-test.zip');p.add_argument('--readme',type=Path);p.add_argument('--rangers-root',default='sdmc:/switch/matrixgame-diag/rangers')
 args=parser.parse_args()
 if args.mode=='history':history(args.output)
 else:package(args.build,args.source_commit,args.output,args.readme,args.rangers_root)
if __name__=='__main__':main()
