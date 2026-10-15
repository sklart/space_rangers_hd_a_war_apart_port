"""Probe actual generated callbacks and loader failures; ARM64 syntax on Windows.

Run from the repository root with native Python (ARM64 SDK) and with Linux
Python (g++ with ASan/UBSan). Private generated outputs stay under build/.
"""
from pathlib import Path
import sys, subprocess
root = Path.cwd()
sys.path.insert(0, str(root / 'tools'))
from e2e_source_overrides import generated_source
out = root / 'build/e2e53/hardware-followup-generated'
for name in ['EC_Buf.cpp', 'EC_Mem.cpp', 'fIntroduction.cpp', 'Globals.cpp']:
    generated_source(root / 'upstream/cpp/src' / name, out / name,
                     audio_on=True, e2e5_profile=True)

def body(name, start, end):
    s = (out / name).read_text(encoding='utf-8')
    s = s[s.index(start):s.index(end, s.index(start))]
    return s[s.index('{'):].strip()

scroll = body('fIntroduction.cpp', '    void TfIntroduction::ScrollBackground(',
              '    void TfIntroduction::ContinueMouseEnter(')
load = body('EC_Buf.cpp', '    void TBufEC::LoadFromFileChunk(',
            '    // Reads only the remaining file bytes;')
cache = (root/'upstream/cpp/src/EC_Cache.cpp').read_text(encoding='utf-8')
trim = cache[cache.index('    void TCacheEC::TrimToBudget('):cache.index('    // PendingLoads owns')]
trim = trim[trim.index('{'):].strip()
recovery = (root/'port/switch/platform/e2e5_memory_recovery.hpp').read_text(encoding='utf-8')
recovery = recovery[recovery.index('namespace srhd_awa::platform::e2e5_memory_recovery {'):]
code = r'''
#include <cstdint>
#include <cassert>
#include <cstdio>
#include <string>
#include <string_view>
#include <stdexcept>
#include <vector>
#include <initializer_list>
using namespace std::literals;
namespace pas { using AnsiString=std::string; int imod(int x,int y) {return x%y;} }
namespace srhd_awa::platform::e2e_stage { std::string last; void Log(const char* s) {last=s;} }
namespace pas {void critical_enter(int) {} void critical_leave(int) {}}
namespace GR_DX {void EvictTextureCaches(bool) {}}
struct TCacheControlEC {int RetainCount=0; TCacheControlEC* NextBoundControl=nullptr;};
struct TCacheDataEC {TCacheDataEC* PrevData=nullptr; TCacheControlEC* FirstBoundControl=nullptr; int ResidentBytes=0; bool freed=false;};
struct Cache {int CacheLock=0,ResidentBytes=0; TCacheDataEC* LeastRecentData=nullptr;
    void RemoveAndFreeData(TCacheDataEC* d) {d->freed=true;}
    void TrimToBudget(int BudgetBytes) TRIM
};
namespace GR_Main {Cache* GlobalCache=nullptr;}
RECOVERY
struct Point {int X,Y;};
namespace ClassesImports {Point Point(int x,int y) {return {x,y};}}
namespace MMSystem {std::uint32_t tick; auto timeGetTime() {return tick;}}
namespace SysUtilsImports {void Sleep(unsigned) {}}
struct Panel {Point ClientSize{1280,720}, LocalPosition{}; void SetPosition(Point p) {LocalPosition=p;}};
namespace GI_MessageLoop {using TObjectGI=Panel;}
struct Timer {std::int32_t UserData;};
struct Intro {
    Panel a,b; int BackgroundScrollOffset=0;
    Panel* GetByName(std::u16string_view name) {return name==u"ImageFon1" ? &a : &b;}
    void Scroll(Timer* Timer, int) SCROLL
};
namespace EC_File {
struct TFileEC { int depth=0,reads=0,fail_read=0; unsigned position=0; bool fail_size=false,fail_diagnostic=false,fail_all_reads=false;
    std::string GetFileName() {if(fail_diagnostic) throw std::bad_alloc(); return "DATA/Gov/2FeiAnim1.gai";}
    void AcquireReadWriteHandle() {++depth;}
    void ReleaseHandle() {--depth;}
    unsigned GetPointer() {return position;}
    void SetPointer(unsigned p,int) {position=p;}
    void ReadBuffer(void*, int bytes) {if (++reads==fail_read || fail_all_reads) throw std::runtime_error("read failure"); position+=bytes;}
};}
namespace EC_Mem {void* AddPointerOffset(void* p,int n) {return static_cast<char*>(p)+n;}}
struct Buf {EC_File::TFileEC* file; std::vector<char> bytes; void* Data=nullptr;
    void Clear() {bytes.clear(); Data=nullptr;}
    void SetSize(int n) {if(file->fail_size && (!GR_Main::GlobalCache || GR_Main::GlobalCache->ResidentBytes>123)) throw std::bad_alloc(); bytes.resize(n); Data=bytes.data();}
    void Load(EC_File::TFileEC* SourceFile,int ByteCount) LOAD
};
int main() {
  for (unsigned origin : {0u,0xffffff00u}) for(int interval : {16,17,40,60,143,1000}) {
    Intro m; Timer timer{static_cast<std::int32_t>(origin)};
    MMSystem::tick=origin; m.Scroll(nullptr,0); assert(m.BackgroundScrollOffset==0);
    for(int elapsed=interval; elapsed<200000; elapsed+=interval) {
      MMSystem::tick=origin+elapsed; m.Scroll(&timer,0);
      assert(m.BackgroundScrollOffset==(elapsed/40)%1280);
      assert(m.a.LocalPosition.X==-(elapsed/40)%1280);
      assert(m.b.LocalPosition.X==1280-(elapsed/40)%1280);
    }
  }
  for(int failure : {0,1,2,3,4,5,6,7,8}) {
    TCacheControlEC held{1}; TCacheDataEC retained{nullptr,&held,123}, unused{&retained,nullptr,512};
    Cache cache{0,635,&unused}; GR_Main::GlobalCache=failure>=5 ? &cache : nullptr;
    EC_File::TFileEC f; f.fail_size=failure==1 || failure==5; f.fail_read=failure==2 || failure==4 || failure==6 ? 1 : failure==3 || failure==7 ? 2 : 0;
    f.fail_diagnostic=failure==4;
    f.fail_all_reads=failure==8;
    Buf b{&f}; bool thrown=false;
    try {b.Load(&f,524300);} catch(...) {thrown=true;}
    assert(f.depth==0); assert(thrown==((failure>0 && failure<5) || failure==8));
    if(thrown) {assert(b.Data==nullptr); assert(srhd_awa::platform::e2e_stage::last.find(failure==4 ? "diagnostic-unavailable" : "requested_bytes=524300")!=std::string::npos);}
    else {assert(b.bytes.size()==524300); assert(f.position==524300);}
    if(failure>=5) {assert(unused.freed); assert(!retained.freed); assert(cache.ResidentBytes==123);}
  }
  puts("actual callback: 25 px/s, jitter, wrap, tiled positions PASS; actual loader/cache trim: failures recover with rewind, retained entries protected, terminal errors propagate, handles balanced PASS");
}
'''.replace('SCROLL',scroll).replace('LOAD',load).replace('TRIM',trim).replace('RECOVERY',recovery)
(out / 'followup.cpp').write_text(code,encoding='utf-8')
if sys.platform == 'win32':
    b=root/'port/switch/build/e2e-game-combined-experiment'
    flags=['-std=gnu++20','-D__SWITCH__','-march=armv8-a+crc+crypto','-mtune=cortex-a57','-mtp=soft','-fPIE','-O2','-fno-strict-aliasing','-DE2E_DIAGNOSTICS=0','-DE2E3_TRACE=1','-DE2E5_DISABLE_DISK_SAVE=1','-DE2E4_AUDIO_ON=1','-DE2E4_QUICKSAVE_EXPERIMENT=1']
    for p in [Path('C:/devkitPro/libnx/include'),Path('C:/devkitPro/portlibs/switch/include'),b/'runtime-overlay',root/'port/switch/platform',b/'source-overlay',root/'upstream/cpp/src',root/'upstream/cpp/runtime',root/'upstream/okgf/include']:
        flags+=['-I'+str(p)]
    for name in ['EC_Buf.cpp','EC_Mem.cpp','fIntroduction.cpp','Globals.cpp']:
        subprocess.run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-g++.exe',*flags,'-fsyntax-only',str(out/name)],check=True)
        print(name,'ARM64 syntax PASS',flush=True)
    for name in ['e2e_threads.cpp', 'package.cpp']:
        subprocess.run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-g++.exe',*flags,'-fsyntax-only',str(root/'port/switch/platform'/name)],check=True)
        print(name,'ARM64 syntax PASS',flush=True)
else:
    subprocess.run(['g++','-std=c++20','-O1','-g','-fsanitize=address,undefined',str(out/'followup.cpp'),'-o',str(out/'followup')],check=True)
    subprocess.run([str(out/'followup')],check=True)
