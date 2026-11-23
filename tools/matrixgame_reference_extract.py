"""Extract only the verified reference SDK archives to new, isolated directories."""
import hashlib
import json
import stat
import zipfile
from pathlib import Path, PurePosixPath

ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/matrixgame/windows-reference-rebuild-20261123'
def main():
 report=json.loads((BASE/'downloads/result.json').read_text(encoding='utf-8'))
 destinations={'microsoft.dxsdk.d3dx.9.29.952.8.nupkg':'d3dx','SDL2-devel-2.30.11-VC.zip':'sdl2'}
 output={}
 for name,folder in destinations.items():
  archive=BASE/'downloads'/name;destination=BASE/'deps'/folder
  assert report[name]['status']=='DOWNLOADED_AND_ZIP_CHECKED'
  with archive.open('rb') as data:assert hashlib.file_digest(data,'sha256').hexdigest()==report[name]['sha256']
  if destination.exists():raise RuntimeError('Refusing to overwrite '+str(destination))
  with zipfile.ZipFile(archive) as source:
   for item in source.infolist():
    path=PurePosixPath(item.filename.replace('\\','/'))
    if path.is_absolute() or '..' in path.parts or any(':' in p for p in path.parts) or stat.S_ISLNK(item.external_attr>>16):
     raise ValueError('unsafe archive member '+item.filename)
   destination.mkdir(parents=True)
   source.extractall(destination)
  output[folder]={'archive_sha256':report[name]['sha256'],'files':sum(p.is_file() for p in destination.rglob('*'))}
  print(folder,output[folder]['files'],flush=True)
 (BASE/'deps'/'manifest.json').write_text(json.dumps(output,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
