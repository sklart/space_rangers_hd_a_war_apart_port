"""Replace just the real legacy WAV loader object in a new SDK host archive."""
import json,os,shutil,subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
BASE=ROOT/'build/matrixgame/e2e590-completed-battle/sdk-dependency'
OLD=ROOT/'build/matrixgame/e2e573-rangers-data/sdk-final'

def members(path):
 import hashlib
 data=path.read_bytes();assert data[:8]==b'!<arch>\n';offset=8;names=b'';result={}
 while offset<len(data):
  header=data[offset:offset+60];assert len(header)==60 and header[58:]==b'`\n';name=header[:16].decode('ascii').strip();size=int(header[48:58]);body=data[offset+60:offset+60+size];assert len(body)==size
  if name=='//':names=body
  elif name not in ('/','/SYM64/'):
   if name.startswith('/'):
    start=int(name[1:]);end=names.index(b'/\n',start);name=names[start:end].decode()
   else:name=name.rstrip('/')
   assert name not in result;result[name]=hashlib.sha256(body).hexdigest()
  offset+=60+size+(size%2)
 assert offset==len(data);return result

def main():
 BASE.mkdir(parents=True,exist_ok=True);prior=read(OLD/'result.json');check_hashes(prior['dependency_objects_sha256'])
 old_archive=OLD/'dependencies/liboriginal-rangers-host-data-cleanup.a';assert sha(old_archive)==prior['dependency_archive_sha256']
 flags=read(ROOT/'port/switch/build/e2e-game-perf515/compile-flags.txt');mapping=read(ROOT/'port/switch/build/e2e-game-perf515/effective-source-map.json')
 original=ROOT/mapping['upstream/cpp/src/EC_CacheSound.cpp'];text=original.read_text(encoding='utf-8');before='            static_cast<std::size_t>(SourceBuffer->Position));';assert text.count(before)==1
 text=text.replace(before,'            static_cast<std::size_t>(SourceBuffer->Position),\n            srhd_awa::platform::e2e4_wav::RiffSizePolicy::LegacyTotalBytes);',1)
 source=BASE/'EC_CacheSound.cpp'
 if source.exists():assert source.read_text(encoding='utf-8')==text
 else:source.write_text(text,encoding='utf-8')
 result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN'};env=dict(os.environ,DEVKITPRO='C:/devkitPro',DEVKITA64='C:/devkitPro/devkitA64')
 def run(command,label):
  command=list(map(str,command));p=subprocess.run(command,cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr;path=BASE/(label+'.log');index=1
  while path.exists():path=BASE/f'{label}-{index}.log';index+=1
  path.write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode,'log':str(path.relative_to(ROOT))});print(label,p.returncode,log[-4000:] if p.returncode else '',flush=True);assert not p.returncode
 try:
  obj=BASE/'EC_CacheSound.o';run([flags['compiler'],*flags['flags'],'-c',source,'-o',obj],'compile-original-WAV-loader')
  archive=BASE/'liboriginal-rangers-host-wave-compat.a';assert not archive.exists();shutil.copyfile(old_archive,archive)
  run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-ar.exe','rs',archive,obj],'replace-one-archive-member')
  old_members=members(old_archive);new_members=members(archive);assert set(old_members)==set(new_members)
  changed=[k for k in old_members if old_members[k]!=new_members[k]];assert changed==['EC_CacheSound.o'] and new_members['EC_CacheSound.o']==sha(obj)
  objects=dict(prior['dependency_objects_sha256']);key=next(k for k in objects if Path(k).name=='EC_CacheSound.o');del objects[key];objects[str(obj.relative_to(ROOT))]=sha(obj)
  result.update(status='PASS; SDK LEGACY WAV LOADER COMPILED; ONE OF 378 ORIGINAL HOST OBJECTS REPLACED; NOT RUN',dependency_objects_sha256=objects,dependency_archive_sha256=sha(archive),archive_path=str(archive.relative_to(ROOT)),old_archive_sha256=sha(old_archive),archive_members_sha256=new_members,changed_members=changed,source_sha256={str(p.relative_to(ROOT)):sha(p) for p in (source,original,ROOT/'port/switch/platform/e2e4_wav.hpp',ROOT/'tools/e2e_source_overrides.py',Path(__file__).resolve())})
 finally:
  with (BASE/'result.json').open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2);stream.write('\n')
 print(result['status'])
if __name__=='__main__':main()
