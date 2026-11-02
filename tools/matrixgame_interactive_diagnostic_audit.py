"""Audit session policy execution and two distinct standard diagnostic packages."""
import argparse
import json
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_diagnostic_package import verify_nro
BASE=ROOT/'build/matrixgame/e2e586-interactive-diagnostic'
def latest(folder,pattern):
 for p in sorted(folder.glob(pattern),key=lambda p:p.stat().st_mtime_ns,reverse=True):
  result=read(p)
  if result['status'].startswith('PASS'):return result
 raise AssertionError('no completed PASS in '+str(folder))
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);output=parser.parse_args().output
 profiles={}
 for kind in ('windows','host','arm64'):
  record=latest(BASE/kind,'result*.json');assert all(c['exit']==0 for c in record['commands']);check_hashes(record['source_sha256'])
  assert 'transition_excluded=PASS exit_states=PASS' in record['stdout'];binary=BASE/kind/('session.exe' if kind=='windows' else 'session');assert sha(binary)==record['binary_sha256'];profiles[kind]=record
 variants=latest(BASE/'sdk','variants-result*.json');manual=variants['interactive'];assert not variants['quick_strong_undefined'] and not manual['strong_undefined']
 assert all(c['exit']==0 for c in [*variants['commands'],*manual['commands']]);check_hashes(manual['link_inputs_sha256']);check_hashes(manual['protected_sha256']);check_hashes(variants['all_link_inputs_sha256']);check_hashes(variants['embedded_fonts_sha256'])
 for name,value in {**manual['artifacts'],**variants['quick_artifacts']}.items():assert sha(ROOT/name)==value['sha256']
 metadata=read(BASE/'sdk/native-diagnostic-build.json');assert sha(BASE/'sdk/native-diagnostic-build.json')==manual['metadata_sha256'];check_hashes(metadata['own_source_sha256']);check_hashes(metadata['generated_original_sha256'],BASE/'sdk/source');check_hashes(metadata['rangers_host']['archives_sha256'])
 plans=metadata['diagnostic_variants'];assert plans['matrixgame-native-diagnostic']['frame_target']==4 and plans['matrixgame-native-interactive']['frame_target'] is None and plans['matrixgame-native-interactive']['loop_deadline_us']==120000000
 before=read(ROOT/'build/matrixgame/e2e583-robot-orders/sdk/native-diagnostic-build.json');assert metadata['generated_original_sha256']==before['generated_original_sha256']
 entries=variants['entry_compile_commands'];assert len(entries)==2
 for entry in entries:
  output_name=entry.get('output','') or entry['command'];is_manual='matrixgame-native-interactive.dir' in output_name;assert ('MATRIXGAME_DIAGNOSTIC_MANUAL=1' in entry['command'])==is_manual
 packages={}
 for target,folder,title,mode in (('matrixgame-native-interactive','interactive-nro','MatrixGame diagnostic manual','interactive'),('matrixgame-native-diagnostic','diagnostic-nro','MatrixGame diagnostic','quick-four-frames')):
  elf=BASE/'sdk'/(target+'.elf');nro=BASE/'sdk'/folder/(target+'.nro');nacp=BASE/'sdk'/folder/'control.nacp'
  assert nacp.read_bytes()[:0x200].split(b'\0')[0].decode('utf-8')==title
  package=verify_nro(nro.read_bytes(),Path('C:/devkitPro/libnx/default_icon.jpg').read_bytes(),nacp.read_bytes())
  assert mode.encode() in elf.read_bytes()
  packages[mode]={'path':str(nro.relative_to(ROOT)),'sha256':sha(nro),'bytes':nro.stat().st_size,'build_id':manual['build_id'],'nacp_title':title,'package_validation':package,'Switch':'NOT RUN'}
 assert packages['interactive']['sha256']!=packages['quick-four-frames']['sha256']
 previous=read(ROOT/'reports/matrixgame-e2e583-robot-orders-evidence.json');check_hashes(previous['protected_game_sha256']);check_hashes(previous['protected_frozen_sha256'])
 old=ROOT/'build/matrixgame/e2e583-robot-orders/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro';assert sha(old)=='53db0a16a8f187fc8dd50a55be20e2e3bd3335d6f86e80c02c13e164a9664233'
 font=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json');primary=font['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256'];assert sha(ROOT/'Switch-E2E5-TEST.nro')==font['root_cumulative_NRO_verified_against_documented_hash'];assert sha(Path('D:/logs2.7z'))=='0b6cd8d561c88ddddd58ea16e117a592f91c28b9a2c2146525f554b8453fd994'
 git='C:/Program Files/Git/cmd/git.exe';pins={}
 for name,pin in (('matrixgame','218115a72fa9c5d43269f4ca84a8596a0bcb82f1'),('cpp','57fa689c630193a66fdea6ca4c79a188814991cd'),('okgf','c01aa7a168a6f1772541501074b7bba1b96550ef')):
  folder=ROOT/'upstream'/name;pins[name]=subprocess.check_output([git,'-C',folder,'rev-parse','HEAD'],text=True).strip();assert pins[name]==pin and not subprocess.check_output([git,'-C',folder,'status','--porcelain'],text=True)
 result={'status':'PASS; TWO STANDARD SDK DIAGNOSTIC NROS; SESSION POLICY WINDOWS/ASAN/ARM64 EXECUTED; SWITCH NOT RUN','policy_profiles':profiles,'variants_build':variants,'packages':packages,'clean_upstream_pins':pins,'protected_game_sha256':previous['protected_game_sha256'],'protected_frozen_sha256':previous['protected_frozen_sha256'],'E583_NRO_preserved':sha(old),'original_generated_sources_unchanged':True,'Switch':'NOT RUN','remaining':['full native/deko Init and visible map frames','physical controls, manual selection/movement/arcade/firing','hardware image/audio/F9/focus/recovery','full battle outcome and return'],'commit':False,'push':False,'audit_source_sha256':sha(Path(__file__).resolve())}
 with output.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2);stream.write('\n')
 print(result['status']);print(packages)
if __name__=='__main__':main()
