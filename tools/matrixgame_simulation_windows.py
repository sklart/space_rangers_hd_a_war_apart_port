"""Actual original Init/Takt/Draw/Deinit on D3D9 HAL; private output only."""
import hashlib,json,os,subprocess,sys,uuid
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/matrixgame/e2e532-simulation/windows'
sys.stdout.reconfigure(encoding='utf-8')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 prior=json.loads((ROOT/'build/matrixgame/e2e528-minimap/windows-frame/result.json').read_text())
 source=ROOT/'build/matrixgame/e2e532-simulation/sdk/source'
 j={'status':'INCOMPLETE','commands':[],'Switch_hardware':'NOT RUN'}
 def run(command,label,cwd=ROOT):
  if isinstance(command,str):
   script=OUT/(label+'.cmd');script.write_text(command+'\n');cmd=['cmd.exe','/d','/c',script]
  else:cmd=command
  p=subprocess.run(cmd,cwd=cwd,capture_output=True,text=True,encoding='utf-8',errors='replace',env=dict(os.environ,MATRIXGAME_UNION_OUTPUT=str(OUT)),timeout=240)
  (OUT/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8')
  j['commands'].append({'label':label,'command':list(map(str,cmd)),'exit':p.returncode})
  print(label,p.returncode,(p.stdout+p.stderr)[-6000:] if p.returncode or label=='runtime' else '',flush=True)
  if p.returncode:raise RuntimeError(label+' failed')
  return p.stdout
 try:
  harness=OUT/'driver.cpp';outputs=OUT/('private-output-'+uuid.uuid4().hex)
  text=(ROOT/'tests/matrixgame_windows_frame_init.cpp').read_text()
  text=text.replace('#include <cstdio>','#include "Interface/CInterface.h"\n#include "input.hpp"\n#include <cstdio>',1)
  text=text.replace('void CaptureOriginalMapFrame();','void CaptureOriginalMapFrame();\nvoid TaktOriginalNativeGame(int);')
  marker='    using namespace srhd_awa::matrixgame;';assert text.count(marker)==1
  text=text.replace(marker,marker+'\n    output_paths::NativeScope private_outputs(L"'+outputs.as_posix()+'");')
  marker='        DrawOriginalWindowsFrame(CaptureOriginalMapFrame);';assert text.count(marker)==1
  text=text.replace(marker,'''        const auto start_time=g_MatrixMap->GetTime();
        if(!g_MatrixMap->IsPaused())throw std::runtime_error("original begin dialog must pause simulation");
        TaktOriginalNativeGame(25);
        if(g_MatrixMap->GetTime()!=start_time)throw std::runtime_error("paused original simulation advanced");
        DrawOriginalWindowsFrame(nullptr);
        g_IFaceList->PressHintButton(HINT_OK); // Same original callback as Return in CFormMatrixGame::Keyboard.
        if(g_MatrixMap->IsPaused())throw std::runtime_error("original begin dialog OK did not unpause");
        g_MatrixMap->m_Cursor.SetPos(400,300);
        for(int frame=0;frame<10;++frame){
            TaktOriginalNativeGame(25);
            if(g_MatrixMap->GetTime()!=start_time+(frame+1)*25)
                throw std::runtime_error("actual original simulation time did not advance");
            DrawOriginalWindowsFrame(frame==9?CaptureOriginalMapFrame:nullptr);
        }
        std::printf("REAL_SIMULATION steps=10 elapsed=%d original_Takt_and_Draw=returned\\n",g_MatrixMap->GetTime()-start_time);''')
  harness.write_text(text)
  template=prior['commands'][0]['command'].split(' /c ')[0]
  # Existing Windows objects use the unchanged original declarations; compile
  # the new extracted Takt against the same declarations, not a model header.
  objects=[]
  for path in (source/'MatrixGame/src/OriginalNativeTakt.cpp',harness):
   obj=OUT/(path.stem+'.obj');run(template+' /c "'+str(path)+'" /Fo"'+str(obj)+'"','build-'+path.stem);objects.append(obj)
  old_harness=ROOT/'build/matrixgame/e2e528-minimap/windows-frame/matrixgame_windows_frame_init_output.obj'
  link=next(c['command'] for c in prior['commands'] if c['label']=='link');assert str(old_harness) in link
  link=link.replace(str(old_harness),str(objects[1]))
  link=link.replace(str(ROOT/'build/matrixgame/e2e528-minimap/windows-frame/windows-game.exe'),str(OUT/'simulation.exe'))
  link=link.replace(str(ROOT/'build/matrixgame/e2e528-minimap/windows-frame/windows-game.map'),str(OUT/'simulation.map'))
  link+=' "'+str(objects[0])+'"';run(link,'link',ROOT/'build/matrixgame/e2e519/original-init-windows')
  cwd=json.loads((ROOT/'build/matrixgame/e2e522/windows-frame/result.json').read_text())['cwd']
  stdout=run([str(OUT/'simulation.exe'),str(OUT)],'runtime',cwd)
  assert stdout.count('REAL_SIMULATION steps=10 elapsed=250 original_Takt_and_Draw=returned')==2
  assert stdout.count('original CGame::Init returned: ready=1')==2
  j.update(status='PASS',stdout=stdout,real_Init_Takt_Draw_Deinit_sessions=2,real_Takt_steps=20,
           binary_sha256=sha(OUT/'simulation.exe'),native_output_root=str(outputs),
           source_sha256={str(p.relative_to(ROOT)):sha(p) for p in (source/'MatrixGame/src/OriginalNativeTakt.cpp',harness,ROOT/'tools/matrixgame_simulation_windows.py')})
 finally:(OUT/'result.json').write_text(json.dumps(j,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
