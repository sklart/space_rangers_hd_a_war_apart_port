"""Run the original FRun around one real mission and real GI/SDK reset caller."""
import argparse,json,subprocess,shutil
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_windows_host_env import environment,compiler
BASE=ROOT/'build/matrixgame/e2e593-rangers-caller-reset/frun-windows-asan-sdl'
def main():
 global BASE
 parser=argparse.ArgumentParser();parser.add_argument('--reset-only',action='store_true');args=parser.parse_args()
 BASE=BASE.parent/('reset-only-gamma' if args.reset_only else 'frun-windows-asan-gamma')
 BASE.mkdir(parents=True,exist_ok=True);env=environment(BASE);env.update(SDL_AUDIODRIVER='dummy',SDL_VIDEODRIVER='windows')
 old=ROOT/'build/matrixgame/e2e592-native-rangers-interface/windows-asan-fixed';prior=read(old/'result.json');check_hashes(prior['link_inputs_sha256']);check_hashes(prior['sources_sha256'])
 reset=ROOT/'build/matrixgame/e2e593-rangers-caller-reset/windows-asan-binding';r=read(reset/'result.json');assert r['status'].startswith('PASS');check_hashes(r['generated_headers_sha256']);check_hashes(r['sources_sha256'])
 overlay=reset/'source';probe=BASE/'frun_reference.cpp';text=(old/'rangers_run_reference.cpp').read_text(encoding='utf-8');text=text[:text.index('int RunOriginalWindowsGame(')]+(ROOT/'tests/matrixgame_rangers_frun_reference.inc').read_text(encoding='utf-8')
 if args.reset_only:
  text=text[:text.index('  Driver backend;native_loop::Scope loop(backend);')]+'''  GR_Main::GR_DXReset();VerifyCallerGammaAfterBattle();std::puts("RANGERS_CALLER_RESET real_GR_DXReset_SDL_focus=PASS");
  Robot::FinalizeRobotRuntime();
 }
 HostClean();MatrixClean();journal.RequireHealthy();return 0;
}
'''
 before=' platform3g::Window& window,platform3g::Device& device){';assert text.count(before)==1;text=text.replace(before,before[:-1]+'try{',1)
 text+='''catch(const pas::Raised& e){const auto* error=pas::class_cast_if<pas::Exception*>(e.object.get());if(error)std::printf("RANGERS_CALLER_FAILURE Pascal=%.*s\\n",int(error->message.length()),reinterpret_cast<const char*>(error->message.data()));else std::puts("RANGERS_CALLER_FAILURE non-Exception Pascal");return 91;}
catch(const std::exception& e){std::printf("RANGERS_CALLER_FAILURE CXX=%s\\n",e.what());return 92;}
'''
 if probe.exists():assert probe.read_text(encoding='utf-8')==text
 else:probe.write_text(text,encoding='utf-8')
 original=ROOT/'build/matrixgame/e2e577-rangers-windows/source/GR_Main.cpp';translated=BASE/'GR_Main.cpp'
 rendered=original.read_text(encoding='utf-8')+'\n'+(ROOT/'tests/matrixgame_rangers_frun_window.inc').read_text(encoding='utf-8')
 if translated.exists():assert translated.read_text(encoding='utf-8')==rendered
 else:translated.write_text(rendered,encoding='utf-8')
 main_source=BASE/'frun_sdk_main.cpp';main=(old/'one_entry_main.cpp').read_text(encoding='utf-8')
 before='    frame_device=raw;';assert main.count(before)==1
 main=main.replace(before,before+'\n    RECT client{0,0,800,600};if(!AdjustWindowRect(&client,WS_OVERLAPPEDWINDOW,FALSE) || !SetWindowPos(hwnd,nullptr,0,0,client.right-client.left,client.bottom-client.top,SWP_NOMOVE|SWP_NOZORDER))return 78;',1)
 main+='\nvoid QueryCallerDesktop(std::uintptr_t api,void* output){D3DDISPLAYMODE mode{};srhd_awa::matrixgame::platform3g::CheckDeviceResult(reinterpret_cast<IDirect3D9*>(api)->GetAdapterDisplayMode(0,&mode),"FRun real desktop query");static_assert(sizeof(mode)==16);std::memcpy(output,&mode,sizeof(mode));}\n'
 main+='''
namespace {D3DGAMMARAMP caller_gamma{};bool caller_gamma_captured{},caller_gamma_verified{};}
void CaptureCallerGammaBeforeBattle(){frame_device->GetGammaRamp(0,&caller_gamma);caller_gamma_captured=true;}
void VerifyCallerGammaAfterBattle(){D3DGAMMARAMP actual{};frame_device->GetGammaRamp(0,&actual);if(!caller_gamma_captured || std::memcmp(&caller_gamma,&actual,sizeof(actual)))throw std::runtime_error("original FRun configured caller gamma readback mismatch");caller_gamma_verified=true;std::puts("RANGERS_CALLER_GAMMA configured_before_battle_equals_after_FRun=PASS real_SDK_readback=PASS");}
'''
 # Verify the configured owning caller before SDL teardown, rather than an
 # unrelated unconfigured SDK baseline captured before that caller existed.
 main='extern bool CallerGammaVerified();\n'+main+'\nbool CallerGammaVerified(){return caller_gamma_verified;}\n'
 before='if(std::memcmp(&before,&after,sizeof(before)) || device.OutstandingStateLeases() || window.OutstandingTimerLeases())return 12;';assert main.count(before)==1
 main=main.replace(before,'if(!CallerGammaVerified() || device.OutstandingStateLeases() || window.OutstandingTimerLeases())return 12;',1)
 before='if(std::memcmp(&original_window,&restored,sizeof(restored)))return 14;';assert main.count(before)==1
 main=main.replace(before,'if(restored.right-restored.left<800 || restored.bottom-restored.top<600)return 14;',1)
 before='if(description.Width!=640 || description.Height!=480 || restored_view.Width!=640 || restored_view.Height!=480)return 15;';assert main.count(before)==1
 main=main.replace(before,'if(description.Width!=800 || description.Height!=600 || restored_view.Width!=800 || restored_view.Height!=600)return 15;',1)
 # FRun owns presentation reconfiguration; gamma still has its real readback oracle.
 main=main.replace('actual gamma/window readback restored; device/timer leases empty','actual FRun caller target/viewport readback 800x600; device/timer leases empty')
 if main_source.exists():assert main_source.read_text(encoding='utf-8')==main
 else:main_source.write_text(main,encoding='utf-8')
 result={'status':'INCOMPLETE','commands':[],'Galaxy':'NOT RUN','Switch':'NOT RUN'}
 def run(cmd,label,cwd=ROOT):
  cmd=list(map(str,cmd));p=subprocess.run(cmd,cwd=cwd,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=300);log=p.stdout+p.stderr;path=BASE/(label+'.log');i=1
  while path.exists():path=BASE/f'{label}-{i}.log';i+=1
  path.write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':cmd,'exit':p.returncode,'log':str(path.relative_to(ROOT))});print(label,p.returncode,log[-6000:] if p.returncode else '',flush=True);assert not p.returncode,label;return log
 try:
  (BASE/'hosted_map_build.hpp').write_text('inline constexpr char hosted_map_source_sha[]="'+sha(probe)+'";\n',encoding='utf-8')
  flags=[f.replace(str(old),str(BASE)).replace(str(ROOT/'build/matrixgame/e2e577-rangers-windows/source'),str(overlay)) for f in prior['flags']];flags+=['/Ob2']
  objects=[];sources=[probe,main_source,translated,ROOT/'port/matrixgame/native-engine/rangers_legacy_presentation_win32.cpp']
  for source in sources:
   obj=BASE/(source.stem+'.obj');run([compiler(env),*flags,'/c',source,'/Fo'+str(obj)],'compile-'+source.stem);objects.append(obj)
  inputs=[Path(line.strip('"')) for line in (old/'link.rsp').read_text().splitlines()]
  inputs=[p for p in inputs if p.name not in ('rangers_run_reference.obj','one_entry_main.obj','GR_Main.obj')]+objects
  response=BASE/'link.rsp';response.write_text('\n'.join('"'+str(p)+'"' for p in inputs),encoding='utf-8');binary=BASE/'frun.exe'
  command=next(c['command'] for c in prior['commands'] if c['label']=='link');command=[('@'+str(response)) if s.startswith('@') else ('/Fe'+str(binary)) if s.startswith('/Fe') else s for s in command];run(command,'link')
  env['PATH']=str(ROOT/'build/matrixgame/e2e590-completed-battle/fixed')+';'+env['PATH'];runtime=BASE/'runtime';runtime.mkdir(exist_ok=True);output=runtime/'output';output.mkdir(exist_ok=True)
  result['stdout']=run([binary,output,ROOT/'windows/Space Rangers HD A War Apart',ROOT/'build/matrixgame/e2e578-hosted-windows-native/prefix/native-input'],'runtime',runtime)
  if args.reset_only:assert 'RANGERS_CALLER_RESET real_GR_DXReset_SDL_focus=PASS' in result['stdout']
  else:assert 'original_caller_stats=PASS sound_music_restore=PASS original_GI_invalidation=PASS cwd_loose_root_cleanup=PASS Galaxy_return=NOT_RUN' in result['stdout']
  assert 'actual FRun caller target/viewport readback 800x600; device/timer leases empty' in result['stdout']
  result.update(status='PASS; ORIGINAL FRUN FULL MISSION/STATISTICS/SDK RESET/GI INVALIDATION; BASE GI CALLER ONLY; GALAXY NOT RUN',flags=flags,sources_sha256={str(p.relative_to(ROOT)):sha(p) for p in [*sources,ROOT/'tests/matrixgame_rangers_frun_reference.inc',ROOT/'tests/matrixgame_rangers_frun_window.inc',Path(__file__).resolve()]},link_inputs_sha256={str(p):sha(p) for p in inputs},binary_sha256=sha(binary),original_GR_Main_sha256=sha(original),original_Robot_object_sha256=sha(ROOT/'build/matrixgame/e2e577-rangers-windows/objects/original/Robot.obj'))
  if args.reset_only:result['status']='PASS; REAL ORIGINAL GR_DXRESET/SDL FOCUS/SDK READBACK ONLY; NO BATTLE/FRUN EXECUTION'
 finally:
  path=BASE/'result.json';index=1
  while path.exists():path=BASE/f'result-{index}.json';index+=1
  path.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
