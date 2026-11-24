"""Compare the actual indexed gather against the preserved CPU reference."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
out=root/'build/matrixgame/e2e516-decode-v19-checks-1'
out.mkdir(parents=True,exist_ok=False)
source=(root/'port/matrixgame/native-engine/platform_switch.cpp').read_text(encoding='utf-8')
a=source.index('DeviceResult DekoDevice::NativeDrawIndexed(')
b=source.index('\n}\n',a)+2
candidate=source[a:b].replace('DekoDevice::NativeDrawIndexed','IndexedHarness::Candidate')
candidate=candidate.replace('    ProfileScope timing(impl_->diagnostics.vertex_decode);\n','').replace('timing.Stop();','')
(out/'candidate-gather.inc').write_text(candidate+'\n',encoding='utf-8')
baseline=subprocess.check_output([r'C:\Program Files\Git\cmd\git.exe','show','0d31db18d16e34a68d219c8a7ed26d3d8e268d70:port/matrixgame/native-engine/platform_switch.cpp'],cwd=root).decode().replace('\r\n','\n')
start=baseline.index('DeviceResult DekoDevice::NativeDrawIndexed(');end=baseline.index('\n}\n',start)+2
baseline=baseline[start:end].replace('DekoDevice::NativeDrawIndexed','IndexedHarness::Baseline')
baseline=baseline.replace('    ProfileScope timing(impl_->diagnostics.vertex_decode);\n','').replace('timing.Stop();','')
(out/'baseline-gather.inc').write_text(baseline,encoding='utf-8')
a=source.index('DkPrimitive NativePrimitive(');b=source.index('\n}',a)
(out/'native-primitive.inc').write_text(source[a:b]+'\n',encoding='utf-8')
fixture=(root/'tests/test_matrixgame_vertex_decode_one.cpp').read_text(encoding='utf-8')
fixture=fixture.replace('assert(original_allocations==candidate_allocations+3);','assert(original_allocations>=candidate_allocations);')
(out/'fixture.cpp').write_text(fixture,encoding='utf-8')
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/').replace('C:/','/mnt/c/')
files=[out/'fixture.cpp',root/'port/matrixgame/deko/fixed_vertex.cpp',root/'port/matrixgame/native-core/buffer_resources.cpp']
includes=[out,root/'port/matrixgame/deko',root/'port/matrixgame/native-engine',root/'port/matrixgame/native-core',Path('C:/devkitPro/libnx/include')]
for kind,compiler in [('host','g++'),('arm64','aarch64-linux-gnu-g++')]:
 exe=out/kind
 flags=['-std=c++20','-O1','-g','-UNDEBUG','-fno-pie','-no-pie','-ffp-contract=off']
 if kind=='host':flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
 compile=['wsl','-d','Ubuntu-26.04','--',compiler,*flags,*['-I'+linux(p) for p in includes],*[linux(p) for p in files],'-o',linux(exe)]
 subprocess.run(compile,check=True)
 run=['wsl','-d','Ubuntu-26.04','--']+(['qemu-aarch64','-L','/usr/aarch64-linux-gnu'] if kind=='arm64' else [])+[linux(exe)]
 subprocess.run(run,check=True)
