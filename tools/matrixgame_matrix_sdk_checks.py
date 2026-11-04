"""Compare actual devkitA64 Multiply bodies with the Linux AArch64 ABI/QEMU."""
import hashlib,json,re,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
FROZEN='98a03c6a23bcf75eeebc879122d772bc16614c74'
BIN='C:/devkitPro/devkitA64/bin/aarch64-none-elf-'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/').replace('C:/','/mnt/c/')
def main():
 parent=ROOT/'build/matrixgame/e610-matrix-r25-checks/sdk-binary';parent.mkdir(parents=True,exist_ok=True);i=1
 while (parent/str(i)).exists():i+=1
 out=parent/str(i);out.mkdir();j={'status':'INCOMPLETE','commands':[],'scope':'actual SDK leaf machine code, CPU QEMU only; no Switch'}
 def run(cmd,label):
  cmd=list(map(str,cmd));p=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr;(out/(label+'.log')).write_text(log,encoding='utf-8');j['commands'].append({'label':label,'command':cmd,'exit':p.returncode});assert p.returncode==0,(label,log[-2000:]);return log
 source='port/matrixgame/deko/fixed_vertex.cpp';old=out/'frozen_fixed_vertex.cpp';old.write_bytes(subprocess.check_output(['C:/Program Files/Git/cmd/git.exe','show',FROZEN+':'+source]))
 meta=json.loads((ROOT/'build/matrixgame/e610-uv-r24/1/native-diagnostic-build.json').read_text());flags=meta['compiler_profile']['cxx_flags'].split();j['SDK_flags']=flags;j['variants']={}
 try:
  for variant,cpp in [('frozen',old),('candidate',ROOT/source)]:
   obj=out/(variant+'.o');run([BIN+'g++.exe','-I'+str(ROOT/'port/matrixgame/deko'),'-IC:/devkitPro/libnx/include',*flags,'-c',cpp,'-o',obj],variant+'-compile')
   nm=run([BIN+'nm.exe','-S',obj],variant+'-nm');rows=[x for line in nm.splitlines() if len(x:=line.split())==4 and '8MultiplyE' in x[3] and x[2] in ('T','t')];assert len(rows)==1,rows
   offset,size=int(rows[0][0],16),int(rows[0][1],16);assert offset==0;section='.text.'+rows[0][3]
   reloc=run([BIN+'objdump.exe','-r','-j',section,obj],variant+'-relocations');assert 'R_AARCH64' not in reloc
   asm=run([BIN+'objdump.exe','-d','-C','-j',section,obj],variant+'-assembly');assert not re.search(r'\b(?:bl|adr|adrp)\s',asm)
   raw=out/(variant+'.bin');run([BIN+'objcopy.exe','-O','binary','--only-section='+section,obj,raw],variant+'-extract');assert len(raw.read_bytes())==size
   wrapper=out/(variant+'.S');wrapper.write_text('.text\n.p2align 4\n.global SDKMultiply\n.type SDKMultiply,%function\nSDKMultiply:\n.incbin "'+linux(raw)+'"\n.size SDKMultiply,.-SDKMultiply\n.section .note.GNU-stack,"",%progbits\n')
   exe=out/variant;run(['wsl','--exec','aarch64-linux-gnu-g++-15','-std=c++20','-O2','-UNDEBUG','-ffp-contract=off','-fno-pie','-no-pie',linux(ROOT/'tests/test_matrixgame_sdk_matrix.cpp'),linux(wrapper),'-o',linux(exe)],variant+'-link-QEMU-probe')
   log=run(['wsl','--exec','qemu-aarch64','-L','/usr/aarch64-linux-gnu',linux(exe)],variant+'-runtime');assert 'SDK_MATRIX_BINARY cases=7744' in log
   j['variants'][variant]={'raw_sha256':sha(raw),'raw_bytes':size,'object_sha256':sha(obj),'runtime_sha256':sha(out/(variant+'-runtime.log')),'section':section}
  assert (out/'frozen-runtime.log').read_bytes()==(out/'candidate-runtime.log').read_bytes(),'actual SDK matrix bits/flags differ'
  j['status']='PASS ACTUAL SDK MULTIPLY BINARY FROZEN/CANDIDATE 7744 CASES; HARDWARE_PENDING'
 finally:
  j['frozen_commit']=FROZEN;j['source_sha256']={p:sha(ROOT/p) for p in [source,'tests/test_matrixgame_sdk_matrix.cpp','tools/matrixgame_matrix_sdk_checks.py']};j['frozen_cpp_sha256']=sha(old);(out/'result.json').write_text(json.dumps(j,indent=2)+'\n',encoding='utf-8')
 print(json.dumps({'status':j['status'],'variants':j['variants']}))
if __name__=='__main__':main()
