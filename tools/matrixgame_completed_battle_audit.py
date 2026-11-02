"""Separate genuine full mission defeat from the still absent galaxy session."""
import argparse,json,hashlib,subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_diagnostic_package import verify_nro
from matrixgame_screenshot_png_verify import decode
from matrixgame_legacy_wav_sdk_dependency import members
BASE=ROOT/'build/matrixgame/e2e590-completed-battle'
def latest(folder,pattern):
 p=max(folder.glob(pattern),key=lambda p:p.stat().st_mtime_ns);r=read(p);assert r['status'].startswith('PASS');return r
def fields(line):return dict(part.split('=',1) for part in line.split()[1:])
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);output=parser.parse_args().output
 run=latest(BASE/'fixed','result*.json');assert all(c['exit']==0 for c in run['commands']) and '/UNDEBUG' in run['flags'] and '/DNDEBUG' not in run['flags']
 for key in ('sources_sha256','link_inputs_sha256','outputs_sha256'):check_hashes(run[key])
 assert sha(BASE/'fixed/hosted-map.exe')==run['binary_sha256']
 text=run['stdout'];dialogs=[fields(l) for l in text.splitlines() if l.startswith('COMPLETION_NATURAL_DIALOG ')];results=[fields(l) for l in text.splitlines() if l.startswith('COMPLETION_RESULT ')];loops=[fields(l) for l in text.splitlines() if l.startswith('COMPLETION_LOOP ')]
 assert len(dialogs)==len(results)==len(loops)==2 and dialogs[0]==dialogs[1] and loops[0]==loops[1]
 assert all(r['state']=='2' and int(r['signed_time'])>0 and int(r['robots_destroyed'])==11 and int(r['turrets_destroyed'])==5 and r['original_Robot_global_statistics']=='PASS' and r['galaxy_screen_transition']=='NOT_RUN' for r in results)
 assert all(int(r['map_time'])==1707100 and int(r['ticks'])==67940 for r in dialogs)
 assert all(int(r['draws'])==347 and int(r['pumps'])==348 and int(r['extra_ticks'])==67940 and r['outcome']=='2' and r['focus_recovery']=='PASS' for r in loops)
 for marker in ('COMPLETION original_FlameEnum_high_pointer_cookie=PASS','COMPLETION natural_win_loss_statistics_original_input_exit=PASS','HOSTED_MAP final_host_cleanup=PASS','actual gamma/window readback restored; device/timer leases empty'):assert text.count(marker)==2
 assert text.count('readback_present=PASS')==8 and 'COMPLETION_HOST_EXCEPTION' not in text and 'COMPLETION_WAVE_ERROR' not in text
 images={};rgb_hashes=[]
 for index in range(8):
  p=BASE/f'fixed/runtime/output/map-frame-{index}.png';w,h,rgb=decode(p.read_bytes(),allow_alpha=True);assert (w,h)==(800,600)
  colors=len({rgb[i:i+3] for i in range(0,len(rgb),3)});assert colors>=64;digest=hashlib.sha256(rgb).hexdigest();rgb_hashes.append(digest);images[str(p.relative_to(ROOT))]={'sha256':sha(p),'rgb_sha256':digest,'distinct_rgb':colors}
 assert len(set(rgb_hashes[:4]))==len(set(rgb_hashes[4:]))==4
 profiles={}
 for kind in ('windows','host','arm64'):
  r=latest(BASE/'wav'/kind,'result*.json');assert all(c['exit']==0 for c in r['commands']);check_hashes(r['source_sha256']);check_hashes(r['binary_sha256']);assert 'strict_rejection_unchanged=PASS explicit_total_size_compat=PASS exact_PCM=PASS malformed_bounds=PASS' in r['stdout'];profiles[kind]=r
 compile_profiles={}
 for kind in ('host','arm64'):
  r=latest(BASE/('compile-'+kind),'result*.json');assert r['runtime']=='NOT RUN' and all(c['exit']==0 for c in r['commands']);check_hashes(r['source_sha256']);check_hashes(r['objects_sha256']);compile_profiles[kind]=r
 context=read(BASE/'galaxy-context-fixed/result.json');assert context['status'].startswith('PASS') and all(c['exit']==0 for c in context['commands']);check_hashes(context['sources_sha256']);check_hashes(context['link_inputs_sha256']);assert sha(BASE/'galaxy-context-fixed/context.exe')==context['binary_sha256']
 assert 'galaxy=0 player=0 government_screen=0 Robot_Run_interface=0 Rangers_D3D_device=0 callbacks=1' in context['stdout']
 dependency=read(BASE/'sdk-dependency/result.json');assert dependency['status'].startswith('PASS') and dependency['changed_members']==['EC_CacheSound.o'];check_hashes(dependency['source_sha256']);check_hashes(dependency['dependency_objects_sha256']);assert len(dependency['dependency_objects_sha256'])==378
 archive=ROOT/dependency['archive_path'];assert sha(archive)==dependency['dependency_archive_sha256'] and members(archive)==dependency['archive_members_sha256']
 variants=latest(BASE/'sdk','variants-result*.json');manual=variants['interactive'];assert not variants['quick_strong_undefined'] and not manual['strong_undefined'];assert all(c['exit']==0 for c in [*variants['commands'],*manual['commands']])
 check_hashes(manual['link_inputs_sha256']);check_hashes(manual['protected_sha256']);check_hashes(variants['all_link_inputs_sha256']);check_hashes(variants['embedded_fonts_sha256'])
 for name,value in {**manual['artifacts'],**variants['quick_artifacts']}.items():assert sha(ROOT/name)==value['sha256']
 metadata=read(BASE/'sdk/native-diagnostic-build.json');assert sha(BASE/'sdk/native-diagnostic-build.json')==manual['metadata_sha256'];check_hashes(metadata['own_source_sha256']);check_hashes(metadata['generated_original_sha256'],BASE/'sdk/source');check_hashes(metadata['rangers_host']['archives_sha256'])
 assert metadata['rangers_host']['dependency_manifest_sha256']==sha(BASE/'sdk-dependency/result.json')
 old=read(ROOT/'build/matrixgame/e2e589-original-result/sdk/native-diagnostic-build.json');delta=[name for name,digest in metadata['generated_original_sha256'].items() if old['generated_original_sha256'][name]!=digest];assert delta==['MatrixGame/src/Effects/MatrixEffectFlame.cpp']
 old_flame=(ROOT/'build/matrixgame/e2e589-original-result/sdk/source'/delta[0]).read_text(encoding='utf-8');new_flame=(BASE/'sdk/source'/delta[0]).read_text(encoding='utf-8');assert old_flame.count('    std::uint32_t uvalue;')==1 and old_flame.replace('    std::uint32_t uvalue;','    uintptr_t uvalue;',1)==new_flame
 assert (BASE/'fixed/generated'/delta[0]).read_text(encoding='utf-8')==new_flame
 packages={}
 for target,folder in (('matrixgame-native-interactive','interactive-nro'),('matrixgame-native-diagnostic','diagnostic-nro')):
  nro=BASE/'sdk'/folder/(target+'.nro');nacp=BASE/'sdk'/folder/'control.nacp';elf=BASE/'sdk'/(target+'.elf');assert 'EC_CacheSound.o' in (BASE/'sdk'/('hosted-interactive.map' if folder=='interactive-nro' else 'hosted-diagnostic.map')).read_text(encoding='utf-8')
  validation=verify_nro(nro.read_bytes(),Path('C:/devkitPro/libnx/default_icon.jpg').read_bytes(),nacp.read_bytes());packages[target]={'path':str(nro.relative_to(ROOT)),'sha256':sha(nro),'bytes':nro.stat().st_size,'build_id':manual['build_id'],'validation':validation,'Switch':'NOT RUN'}
 prior=read(ROOT/'reports/matrixgame-e2e589-original-result-evidence.json');check_hashes(prior['protected_game_sha256']);check_hashes(prior['protected_frozen_sha256'])
 for package in prior['packages'].values():assert sha(ROOT/package['path'])==package['sha256']
 font=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json');primary=font['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256'];assert sha(ROOT/'Switch-E2E5-TEST.nro')==font['root_cumulative_NRO_verified_against_documented_hash'];assert sha(Path('D:/logs2.7z'))=='0b6cd8d561c88ddddd58ea16e117a592f91c28b9a2c2146525f554b8453fd994'
 cdb=read(BASE/'debug/run-1.json');assert sha(BASE/'debug/hosted-map-1.exe')==cdb['binary_sha256'] and sha(BASE/'debug/hosted-map-1.map')==cdb['mapping_sha256'];assert b'c0000005' in (BASE/'debug/cdb-1.log').read_bytes()
 wave=BASE/'fixed/runtime/output/failed-wave.bin';assert sha(wave)=='56baed668848829f3f971c697946a2a90532fab057608630f961e1bc4e93d5a8'
 git='C:/Program Files/Git/cmd/git.exe';pins={}
 for name,pin in (('matrixgame','218115a72fa9c5d43269f4ca84a8596a0bcb82f1'),('cpp','57fa689c630193a66fdea6ca4c79a188814991cd'),('okgf','c01aa7a168a6f1772541501074b7bba1b96550ef')):
  folder=ROOT/'upstream'/name;pins[name]=subprocess.check_output([git,'-C',folder,'rev-parse','HEAD'],text=True).strip();assert pins[name]==pin and not subprocess.check_output([git,'-C',folder,'status','--porcelain'],text=True)
 result={'status':'PARTIAL; REAL COMPLETE MISSION DEFEAT AND ORIGINAL GLOBAL RESULT PASS; FULL GALAXY RETURN NOT RUN','windows_execution':run,'natural_dialogs':dialogs,'mission_results':results,'loops':loops,'images':images,'WAV_profiles':profiles,'flame_compile_profiles':compile_profiles,'SDK_dependency':dependency,'variants_build':variants,'packages':packages,'galaxy_context':context,'changed_generated_originals':delta,'clean_upstream_pins':pins,'protected_game_sha256':prior['protected_game_sha256'],'protected_frozen_sha256':prior['protected_frozen_sha256'],'preserved_E589_packages':prior['packages'],'baseline_CDB_sha256':sha(BASE/'debug/cdb-1.log'),'original_failed_WAV_sha256':sha(wave),'full_mission_defeat':'PASS TWICE','full_mission_victory':'NOT RUN','galaxy_transition':'NOT RUN; REAL GALAXY/PLAYER/GOVERNMENT SCREEN AND NATIVE RUN DISPATCH STILL REQUIRED','Switch':'NOT RUN','save_load':'NOT INVOKED; PROTECTED INPUT HASHES VERIFIED','commit':False,'push':False,'audit_source_sha256':sha(Path(__file__).resolve())}
 with output.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2);stream.write('\n')
 print(result['status']);print(results);print(packages)
if __name__=='__main__':main()
