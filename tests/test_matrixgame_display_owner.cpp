#include "native_display_owner.hpp"
#include <cassert>
#include <cstdio>
#include <stdexcept>
#include <vector>
struct Backend {
    int fail{},step{};std::vector<int> release;
    void Acquire(){if(++step==fail)throw std::runtime_error("SDK fault injection");}
    void InitializeService(){Acquire();} void OpenDisplay(){Acquire();}
    void CreateLayer(){Acquire();} void ConfigureLayer(){Acquire();}
    void CreateWindow(){Acquire();} void ConfigureWindow(){Acquire();}
    void CloseWindow()noexcept{release.push_back(4);} void CloseLayer()noexcept{release.push_back(3);}
    void CloseDisplay()noexcept{release.push_back(2);} void ExitService()noexcept{release.push_back(1);}
};
int main(){
    for(int fail=0;fail<=6;++fail){
        Backend b;b.fail=fail;bool caught=false;
        try{srhd_awa::matrixgame::NativeDisplayOwner owner(b);assert(!fail);assert(b.release.empty());}
        catch(const std::runtime_error&){caught=true;}
        assert(caught==bool(fail));
        const std::vector<std::vector<int>> expected{{4,3,2,1},{},{1},{2,1},{3,2,1},{3,2,1},{4,3,2,1}};
        assert(b.release==expected[fail]);
    }
    std::puts("DISPLAY_OWNER PASS six acquisition/config failures reverse cleanup exactly once; no SDK runtime");
}
