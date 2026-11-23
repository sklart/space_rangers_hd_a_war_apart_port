"""Prepare official Ogg/Vorbis source archives in an isolated directory."""
import hashlib,json,tarfile,urllib.request
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/windows-reference-rebuild-20261123'
PACKAGES={
 'libogg-1.3.5.tar.gz':'https://downloads.xiph.org/releases/ogg/libogg-1.3.5.tar.gz',
 'libvorbis-1.3.7.tar.gz':'https://downloads.xiph.org/releases/vorbis/libvorbis-1.3.7.tar.gz',
}
def main():
 downloads=BASE/'audio-downloads';sources=BASE/'audio-sources'
 if downloads.exists() or sources.exists():raise RuntimeError('audio dependency output already exists')
 downloads.mkdir();sources.mkdir();report={}
 for name,url in PACKAGES.items():
  target=downloads/name;entry={'url':url,'status':'INCOMPLETE'};report[name]=entry
  try:
   with urllib.request.urlopen(url,timeout=45) as response,target.open('xb') as out:
    while chunk:=response.read(1024*1024):out.write(chunk)
   with target.open('rb') as data:entry['sha256']=hashlib.file_digest(data,'sha256').hexdigest()
   with tarfile.open(target,'r:gz') as archive:
    for item in archive.getmembers():
     if item.issym() or item.islnk() or not (item.isfile() or item.isdir()):raise ValueError('unexpected archive entry '+item.name)
    archive.extractall(sources,filter='data')
   entry.update(status='DOWNLOADED_AND_EXTRACTED',bytes=target.stat().st_size)
  except Exception as error:entry.update(status='FAILED',error=str(error))
  print(name,entry['status'],flush=True)
 (BASE/'audio-dependencies-result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
 if any(v['status']!='DOWNLOADED_AND_EXTRACTED' for v in report.values()):raise SystemExit(1)
if __name__=='__main__':main()
