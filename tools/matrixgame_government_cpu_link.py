"""Stage existing genuine dependencies on WSL's filesystem, then link the new UI owner."""
import json,shutil,subprocess,sys,tempfile
from pathlib import Path
def main():
 data=json.loads(Path(sys.argv[1]).read_text());stage=Path(tempfile.mkdtemp(prefix='matrixgame-government-link-'));objects=[];libs=[]
 for i,name in enumerate(data['objects']):
  dest=stage/f'{i:03d}.o';shutil.copyfile(name,dest);objects.append(str(dest))
 for i,name in enumerate(data.get('libraries',[])):
  dest=stage/f'lib{i:03d}.a';shutil.copyfile(name,dest);libs.append(str(dest))
 response=stage/'objects.rsp';response.write_text('\n'.join(objects)+'\n')
 command=[data['cxx'],*data['link_flags'],'@'+str(response),'-Wl,--gc-sections']
 if libs:command+=['-Wl,--start-group',*libs,'-Wl,--end-group','-lm','-ldl','-pthread']
 else:
  library=Path(data['okgf']);command += [str(library),'-Wl,-rpath,'+str(library.parent),'-lz','-lpng','-ljpeg','-lvorbisfile','-lvorbis','-logg','-lSDL2','-lm','-pthread']
 binary=stage/'government';command += ['-o',str(binary)]
 p=subprocess.run(command,capture_output=True,text=True);print(p.stdout+p.stderr,end='');assert not p.returncode
 shutil.copyfile(binary,data['output']);Path(data['output']).chmod(0o755)
 Path(data['report']).write_text(json.dumps({'status':'PASS','command':command,'staging':str(stage)},indent=2)+'\n')
if __name__=='__main__':main()
