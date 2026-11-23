"""Fetch and safely unpack isolated source dependencies for the MSVC reference."""
import hashlib,json,tarfile,urllib.request
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/matrixgame/windows-reference-rebuild-20261123'
PACKAGES={
 'zlib-1.3.2.tar.gz':'https://zlib.net/zlib-1.3.2.tar.gz',
 'libpng-1.6.48.tar.gz':'https://github.com/pnggroup/libpng/releases/download/v1.6.48/libpng-1.6.48.tar.gz',
 'libjpeg-turbo-3.1.0.tar.gz':'https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/3.1.0/libjpeg-turbo-3.1.0.tar.gz',
}
def main():
 downloads=BASE/'source-downloads';sources=BASE/'source-deps'
 if downloads.exists() or sources.exists():raise RuntimeError('source dependency output already exists')
 downloads.mkdir(parents=True);sources.mkdir()
 report={}
 for filename,url in PACKAGES.items():
  target=downloads/filename;entry={'url':url,'status':'INCOMPLETE'};report[filename]=entry
  try:
   with urllib.request.urlopen(url,timeout=45) as response,target.open('xb') as out:
    while chunk:=response.read(1024*1024):out.write(chunk)
   with target.open('rb') as data:entry['sha256']=hashlib.file_digest(data,'sha256').hexdigest()
   with tarfile.open(target,'r:gz') as archive:
    for item in archive.getmembers():
     if item.issym() or item.islnk() or not (item.isfile() or item.isdir()):raise ValueError('unexpected archive entry '+item.name)
    archive.extractall(sources,filter='data')
   entry['status']='DOWNLOADED_AND_EXTRACTED';entry['bytes']=target.stat().st_size
  except Exception as error:entry['status']='FAILED';entry['error']=str(error)
  print(filename,entry['status'],flush=True)
 (BASE/'source-dependencies-result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
 if any(v['status']!='DOWNLOADED_AND_EXTRACTED' for v in report.values()):raise SystemExit(1)
if __name__=='__main__':main()
