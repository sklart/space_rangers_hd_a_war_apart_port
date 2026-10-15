"""Execute generated music lock guards and name-keyboard gesture gate."""
from pathlib import Path
import subprocess,sys
root=Path.cwd();sys.path.insert(0,str(root/'tools'))
from e2e_source_overrides import generated_source
out=root/'build/e2e55';out.mkdir(parents=True,exist_ok=True)
def source(name):
    return generated_source(root/'upstream/cpp/src'/name,out/name,audio_on=True,e2e5_profile=True).read_text()
def function(text, signature):
    begin=text.index(signature);end=text.index('\n    }',begin)+len('\n    }')
    return text[begin:end]
music=source('GR_Music.cpp')
getter=function(music,'    pas::WideString TMusicUnit::GetFileName() {')
control=function(music,'    void TMusicControl_Execute(TMusicControl* Self) {')
settings=source('fGameSettings2.cpp')
arm=function(settings,'    void E2EArmNameKeyboard(')
gate=function(settings,'    void TfGameSettings2::PlayerNameMouseDown(')
gate=gate[gate.index('        auto* field'):gate.index('            auto* edit')]
code=r'''
#include <cassert>
#include <cstdint>
#include <string>
#include <string_view>
#include <mutex>
#include <thread>
#include <new>
#include <utility>
using namespace std::string_view_literals;
namespace pas {
using WideString=std::string;
template<class F> struct ScopeExit {F f; ScopeExit(F x):f(x){} ~ScopeExit(){f();}};
void critical_enter(std::recursive_mutex* m){m->lock();}
void critical_leave(std::recursive_mutex* m){m->unlock();}
}
namespace WindowsSdk {struct TPoint{int X,Y;};const unsigned INFINITE=~0u;
void WaitForSingleObject(unsigned,unsigned){} void SetEvent(unsigned){} }
namespace SysUtilsImports {void Sleep(unsigned){} }
int recoveries=0;
namespace srhd_awa::platform::e2e5_failure_diagnostics {
void Write(const char*) {++recoveries;} void Memory(const char*) {}
}
namespace GR_Music {
struct File {bool fail=false;std::string GetFileName(){if(fail) throw std::bad_alloc();return "track.dat";}};
struct Stream {File* SourceFile;};
struct TMusicUnit {
 std::recursive_mutex mutex; std::recursive_mutex* DecodeLock=&mutex;
 GR_Music::Stream* Stream; void* Buffer=nullptr;unsigned CompletionEvent=0,StartPlaybackEvent=0;
 int stopped=0;bool IsRunning(){return true;} void RequestStop(){++stopped;}
 pas::WideString GetFileName();
};
GETTER
struct TMusicControl {
 TMusicUnit* Current;TMusicUnit* Queued;std::string CurrentFileName;
 std::recursive_mutex mutex;std::recursive_mutex* ControlLock=&mutex;int checks=0;
 unsigned CompletionEvent=1;bool IsStopRequested(){return ++checks>2;}
};
}
namespace srhd_awa::platform::e2e4_game_audio {bool IsStreamPrimed(void*){return true;}}
namespace GR_Music { CONTROL }
void Unlocked(std::recursive_mutex& m) {
 bool ok=false;std::thread other([&]{ok=m.try_lock();if(ok)m.unlock();});other.join();assert(ok);
}
namespace GI_MessageLoop {struct TObjectGI {
 int UserState=0;bool ContainsPoint(WindowsSdk::TPoint p){return p.X>=10&&p.X<30&&p.Y>=20&&p.Y<40;}
};}
namespace fGameSettings2 {
struct TfGameSettings2 {
 GI_MessageLoop::TObjectGI field;int opens=0;
 auto* GetByName(std::u16string_view){return &field;} void SetFocusedControl(void*){}
 void Release(GI_MessageLoop::TObjectGI* Sender,WindowsSdk::TPoint Point) {
 GATE ++opens; }
 }
};
ARM
}
int main(){
 using namespace GR_Music;
 File file;Stream stream{&file};TMusicUnit a,b;a.Stream=b.Stream=&stream;
 file.fail=true;
 try {a.GetFileName();assert(false);}catch(const std::bad_alloc&){} Unlocked(a.mutex);
 TMusicControl c;c.Current=&a;c.Queued=&b;
 TMusicControl_Execute(&c);
 assert(recoveries==1 && c.Current==&a && c.Queued==&b && c.CurrentFileName.empty() && a.stopped==0);
 Unlocked(c.mutex);Unlocked(b.mutex);
 file.fail=false;c.checks=0;TMusicControl_Execute(&c);
 assert(c.Current==&b && c.Queued==&a && c.CurrentFileName=="track.dat" && a.stopped==1);Unlocked(c.mutex);
 fGameSettings2::TfGameSettings2 screen;GI_MessageLoop::TObjectGI panel;
 // Focused name receives unrelated releases; none may open the keyboard.
 screen.Release(&panel,{50,50});screen.Release(&screen.field,{50,50});assert(screen.opens==0);
 fGameSettings2::E2EArmNameKeyboard(&screen.field,0,{15,25});
 screen.Release(&screen.field,{50,50});assert(screen.opens==0); // drag outside
 fGameSettings2::E2EArmNameKeyboard(&screen.field,0,{15,25});
 screen.Release(&screen.field,{15,25});screen.Release(&screen.field,{15,25});assert(screen.opens==1);
 // A new outside press cancels any previously armed field.
 fGameSettings2::E2EArmNameKeyboard(&screen.field,0,{15,25});
 screen.Release(&panel,{50,50});screen.Release(&screen.field,{15,25});assert(screen.opens==1);
}
'''.replace('GETTER',getter).replace('CONTROL',control).replace('GATE',gate).replace('ARM',arm)
(out/'guards.cpp').write_text(code)
subprocess.run(['g++','-std=c++20','-O1','-g','-pthread','-fsanitize=address,undefined',str(out/'guards.cpp'),'-o',str(out/'guards')],check=True)
subprocess.run([str(out/'guards')],check=True)
print('actual generated getter/control exception unlock, transactional switch/retry, name focus/drag/duplicate-release gate PASS')
