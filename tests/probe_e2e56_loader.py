"""Exercise ownership cleanup in the actual generated loading worker."""
from pathlib import Path
import sys
import subprocess
sys.path.insert(0,str(Path('tools').resolve()))
from e2e_source_overrides import generated_source
out=Path('build/e2e56/loader-probe');out.mkdir(parents=True,exist_ok=True)
p=generated_source(Path('upstream/cpp/src/fLoad.cpp'),out/'fLoad.cpp',audio_on=True,e2e5_profile=True)
s=p.read_text(encoding='utf-8');a=s.index('    void TCacheLoader_Execute(');b=s.index('\n    }',a)+len('\n    }')
body=s[a:b]
code=r'''
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>
#include <new>
namespace pas {
template<class F>struct ScopeExit{F f;ScopeExit(F x):f(x){}~ScopeExit(){f();}};
struct List{std::vector<void*> entries;};
int list_count(List* l){return l->entries.size();}
template<class T>T* list_at(List* l,int i){return static_cast<T*>(l->entries.at(i));}
void list_put(List* l,int i,void* p){l->entries.at(i)=p;}
template<class T>void free(T* p){delete p;}
template<class T>struct Range{T i,end;bool next(T& v){v=i;return i++<=end;}};
template<class T>Range<T>for_to(T a,T b){return {a,b};}
auto view(const std::u16string& s){return std::u16string_view(s);}
}
namespace srhd_awa::platform::e2e_clock{unsigned long long Microseconds(){return 0;}}
namespace srhd_awa::platform::e2e5_failure_diagnostics{void Write(const char*){}}
namespace aGalaxy{struct GalaxyType{bool Destroying=false;};GalaxyType* Galaxy=nullptr;}
namespace GR_Main{bool ExitScreenLoop=false;}
namespace SysUtilsImports{void Sleep(unsigned){assert(false);}}
namespace EC_Cache{
int alive=0,failed_index=-1,calls=0;
struct TCacheControlEC{
 std::u16string CacheKey=u"Bm.Test";int index;
 TCacheControlEC(int i):index(i){++alive;}~TCacheControlEC(){--alive;}
 void AcquireData(){++calls;if(index==failed_index)throw std::bad_alloc();}void Release(){}
};}
struct TCacheLoader{
 pas::List* PendingLoads=nullptr;int CompletedLoadCount=0;bool Flag18=false,stop=false;
 bool IsStopRequested(){return stop;}
};
BODY
int main(){
 for(int failure:{-1,0,2})for(bool stop:{false,true}){
  TCacheLoader loader;loader.stop=stop;loader.PendingLoads=new pas::List;
  EC_Cache::failed_index=failure;EC_Cache::calls=0;
  for(int i=0;i<5;++i)loader.PendingLoads->entries.push_back(new EC_Cache::TCacheControlEC(i));
  bool failed=false;try{TCacheLoader_Execute(&loader);}catch(const std::bad_alloc&){failed=true;}
  assert(!EC_Cache::alive&&!loader.PendingLoads);
  assert(failed==(!stop&&failure>=0));
  assert(loader.CompletedLoadCount==((stop||failure<0)?5:failure));
  assert(EC_Cache::calls==(stop?0:failure<0?5:failure+1));
 }
}
'''.replace('BODY',body)
(out/'loader.cpp').write_text(code,encoding='utf-8')
subprocess.run(['g++','-std=c++20','-O1','-g','-fsanitize=address,undefined',str(out/'loader.cpp'),'-o',str(out/'loader')],check=True)
subprocess.run([str(out/'loader')],check=True)
print('loading worker actual body: successful / first / middle failed allocation / cancellation cleanup PASS')
