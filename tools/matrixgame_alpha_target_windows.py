"""Actual Windows HAL A8 versus same native GLSL R8; no Switch hardware."""
import json,hashlib,subprocess,sys,argparse
sys.stdout.reconfigure(encoding="utf-8")
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/matrixgame/e2e526-render-textures/windows-alpha'
def main():
 global OUT
 ap=argparse.ArgumentParser();ap.add_argument('--output',type=Path,default=OUT);OUT=ap.parse_args().output.resolve()
 OUT.mkdir(parents=True,exist_ok=True);j={'status':'INCOMPLETE','commands':[]}
 sources=['tests/matrixgame_alpha_target_windows.cpp','port/matrixgame/deko/material_combiner.cpp']
 inputs=sources+['tests/matrixgame_material_gl_reference.hpp','port/matrixgame/deko/material.frag.glsl','port/matrixgame/deko/alpha_target.hpp','port/matrixgame/deko/material_combiner.hpp',str(Path(__file__).relative_to(ROOT))]
 inputs+=['port/matrixgame/deko/pixel_fog.hpp','port/matrixgame/deko/legacy_raster_viewport.hpp','port/matrixgame/deko/fixed_vertex.hpp']
 try:
  vs='C:/Program Files/Microsoft Visual Studio/2022/Enterprise/Common7/Tools/VsDevCmd.bat'
  cmd='call "'+vs+'" -no_logo -arch=x86 -host_arch=x64 && cl /nologo /EHsc /std:c++20 /O2 /UNDEBUG /DNOMINMAX /DWIN32_LEAN_AND_MEAN /I"'+str(ROOT/'port/matrixgame/deko')+'" /I"'+str(ROOT/'port/matrixgame/native-engine')+'" '
  cmd+=' '.join('"'+str(ROOT/p)+'"' for p in sources)+' /Fo"'+OUT.as_posix()+'/" /Fe"'+str(OUT/'alpha.exe')+'" /link d3d9.lib user32.lib gdi32.lib opengl32.lib'
  script=OUT/'build.cmd';script.write_text(cmd+'\n')
  for label,command in [('build',['cmd.exe','/d','/c',script]),('runtime',[OUT/'alpha.exe',ROOT/'port/matrixgame/deko/material.frag.glsl'])]:
   p=subprocess.run(list(map(str,command)),cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');(OUT/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8');j['commands'].append({'label':label,'command':list(map(str,command)),'exit':p.returncode});print(label,p.returncode,(p.stdout+p.stderr)[-4000:],flush=True);assert p.returncode==0
   if label=='runtime':j['stdout']=p.stdout
  j['source_sha256']={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in inputs};j['status']='PASS';j['Switch_hardware']='HARDWARE_PENDING'
 finally:(OUT/'result.json').write_text(json.dumps(j,indent=2)+'\n')
if __name__=='__main__':main()
