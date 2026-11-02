"""WSL-local staging avoids random NTFS seeks by the original-game linker."""
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
def main():
    manifest=json.loads(Path(sys.argv[1]).read_text());stage=Path(tempfile.mkdtemp(prefix='matrixgame-sound-link-'));objects=[]
    for index,name in enumerate(manifest['objects']):
        destination=stage/f'{index:03d}.o';shutil.copyfile(name,destination);objects.append(destination)
    response=stage/'objects.rsp';response.write_text('\n'.join(map(str,objects))+'\n')
    archives=[]
    for index,name in enumerate(manifest.get('extra_archives',[])):
        destination=stage/f'extra-{index:03d}.a';shutil.copyfile(name,destination);archives.append(str(destination))
    library=Path(manifest['okgf']);output=stage/'rangers-sound'
    command=['g++',*manifest.get('link_flags',[]),'@'+str(response),'-Wl,--gc-sections',str(library),'-Wl,-rpath,'+str(library.parent),'-lz','-lpng','-ljpeg','-lvorbisfile','-lvorbis','-logg','-lSDL2','-lm','-pthread','-o',str(output)]
    if archives:command[command.index('-Wl,--gc-sections')+1:command.index('-Wl,--gc-sections')+1]=['-Wl,--start-group',*archives,'-Wl,--end-group']
    result=subprocess.run(command,capture_output=True,text=True);print(result.stdout+result.stderr,end='');assert result.returncode==0
    destination=Path(manifest['output']);shutil.copyfile(output,destination);destination.chmod(0o755)
    Path(manifest['link_report']).write_text(json.dumps({'command':command,'staging':str(stage),'status':'PASS'},indent=2)+'\n')
if __name__=='__main__':main()
