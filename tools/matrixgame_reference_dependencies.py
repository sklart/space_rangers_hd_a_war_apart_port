"""Fetch official Windows reference build dependencies into a new workspace directory."""
import hashlib
import json
import urllib.request
import zipfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/matrixgame/windows-reference-rebuild-20261123/downloads'
PACKAGES={
 'microsoft.dxsdk.d3dx.9.29.952.8.nupkg':'https://api.nuget.org/v3-flatcontainer/microsoft.dxsdk.d3dx/9.29.952.8/microsoft.dxsdk.d3dx.9.29.952.8.nupkg',
 'SDL2-devel-2.30.11-VC.zip':'https://www.libsdl.org/release/SDL2-devel-2.30.11-VC.zip',
}
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 report={}
 for name,url in PACKAGES.items():
  target=OUT/name
  if target.exists():raise RuntimeError('Refusing to overwrite '+str(target))
  entry={'url':url,'status':'INCOMPLETE'};report[name]=entry
  try:
   with urllib.request.urlopen(url,timeout=45) as response,target.open('xb') as out:
    while chunk:=response.read(1024*1024):out.write(chunk)
   with zipfile.ZipFile(target) as archive:
    bad=archive.testzip()
    if bad:raise ValueError('bad ZIP member '+bad)
    entry['members']=len(archive.infolist())
   with target.open('rb') as data:entry['sha256']=hashlib.file_digest(data,'sha256').hexdigest()
   entry['bytes']=target.stat().st_size;entry['status']='DOWNLOADED_AND_ZIP_CHECKED'
  except Exception as error:
   entry['status']='FAILED';entry['error']=str(error)
  print(name,entry['status'],flush=True)
 (OUT/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
 if any(x['status']!='DOWNLOADED_AND_ZIP_CHECKED' for x in report.values()):raise SystemExit(1)
if __name__=='__main__':main()
