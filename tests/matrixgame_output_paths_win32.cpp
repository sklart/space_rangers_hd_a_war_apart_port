// Real Shell allocation/path read and a caller-specified private scratch folder.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <objbase.h>
#include "output_paths_backend.hpp"
#include <cassert>
#include <cstdio>
namespace paths=srhd_awa::matrixgame::output_paths;
int wmain(int argc,wchar_t** argv){
    if(argc!=2)return 2;
    const auto initialized=::CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(FAILED(initialized))return 3;
    struct ComOwner{~ComOwner(){::CoUninitialize();}} com;
    for(int entry=0;entry<4;++entry){
        paths::FolderItem item;
        const auto result=paths::PersonalLocation(item);
        if(result!=0){std::fprintf(stderr,"actual personal folder query failed: %ld\n",static_cast<long>(result));return 4;}
        wchar_t path[260]{};
        paths::PersonalPath(item,path,260);assert(path[0] && wcslen(path)<260);
        bool duplicate=false;try{paths::PersonalLocation(item);}catch(const std::logic_error&){duplicate=true;}
        assert(duplicate);
    }
    // Never call the full PathToOutputFiles here: its original Windows path
    // creates Documents/SpaceRangersHD. This test writes only its private arg.
    assert(::GetFileAttributesW(argv[1])==INVALID_FILE_ATTRIBUTES);
    assert(paths::CreateFolder(argv[1]));
    assert((::GetFileAttributesW(argv[1])&FILE_ATTRIBUTE_DIRECTORY)!=0);
    assert(!paths::CreateFolder(argv[1]));
    std::puts("actual Shell PIDL ownership/reentry/path read and private directory create/occupied result PASS; full Documents output function NOT EXECUTED");
    return 0;
}
