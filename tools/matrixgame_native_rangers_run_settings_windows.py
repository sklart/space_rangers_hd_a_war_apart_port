"""One additional full Run with the real caller settings contract (not null)."""
import subprocess,sys
from matrixgame_standard_nro_audit import ROOT
if __name__=='__main__':
 source=ROOT/'tools/matrixgame_native_rangers_run_windows.py';text=source.read_text(encoding='utf-8')
 replacements={
  "BASE=ROOT/'build/matrixgame/e2e591-native-rangers-run/windows-asan'":"BASE=ROOT/'build/matrixgame/e2e591-native-rangers-run/windows-asan-settings'",
  '   aGalaxyStruct::TPlanetBattleStatistics output{};':'''   auto caller_settings=Robot::RobotSettings;caller_settings.Direct3D=reinterpret_cast<void*>(api);caller_settings.Device=reinterpret_cast<void*>(raw);caller_settings.ScreenWidth=800;caller_settings.ScreenHeight=600;
   const auto caller_before=caller_settings;
   aGalaxyStruct::TPlanetBattleStatistics output{};''',
  'run(0,0,absent,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,&output)':'run(0,0,absent,&caller_settings,nullptr,nullptr,nullptr,nullptr,nullptr,&output)',
  'run(0xffffffffu,0xdeadbeefu,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,&output)':'run(0xffffffffu,0xdeadbeefu,nullptr,&caller_settings,nullptr,nullptr,nullptr,nullptr,nullptr,&output)',
  '   MatrixClean();assert(code==2':'   assert(std::memcmp(&caller_settings,&caller_before,sizeof(caller_settings))==0);std::puts("RANGERS_RUN caller_settings_nonnull=PASS caller_bytes_unchanged=PASS");\n   MatrixClean();assert(code==2',
  '   // A null settings request uses the owned original defaults, which were\n   // deliberately changed by the caller after constructing the binding.':'   // The actual FRun caller supplies a nonnull settings block; preserve every byte.',
  "  sources=[probe,ROOT/'port/matrixgame/native-engine/native_rangers_run.cpp'];objects=[]":"  main_source=BASE/'one_entry_main.cpp';main_text=(fixed/'hosted_main.cpp').read_text(encoding='utf-8');assert main_text.count('entry<2')==1;main_source.write_text(main_text.replace('entry<2','entry<1',1),encoding='utf-8')\n  sources=[probe,ROOT/'port/matrixgame/native-engine/native_rangers_run.cpp',main_source];objects=[]",
  "if p.name!='completed_loop_legacy_audio.obj'":"if p.name not in ('completed_loop_legacy_audio.obj','hosted_main.obj')",
  "pointer_casts=PASS')==2":"pointer_casts=PASS')==1",
  'TWO COMPLETE ORIGINAL MISSION LOSSES':'ONE COMPLETE ORIGINAL MISSION LOSS WITH NONNULL CALLER SETTINGS',
 }
 for before,after in replacements.items():assert text.count(before)==1,before;text=text.replace(before,after,1)
 text="import sys\nsys.path.insert(0,"+repr(str(ROOT/'tools'))+")\n"+text
 out=ROOT/'build/matrixgame/e2e591-native-rangers-run/settings-fixture';out.mkdir(parents=True,exist_ok=True);script=out/'run.py'
 if script.exists():assert script.read_text(encoding='utf-8')==text
 else:script.write_text(text,encoding='utf-8')
 subprocess.run([sys.executable,script],check=True)
