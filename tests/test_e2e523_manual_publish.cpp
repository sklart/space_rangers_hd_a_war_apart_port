// Real captured RSG64 saves and production transaction; libc fault injection
// distinguishes the two ENOENT sites without guessing the console's failure.
#include "e2e4_quicksave_transaction.hpp"
#include "user_root.hpp"
#include <cassert>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <fcntl.h>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
namespace fs=std::filesystem;
using namespace srhd_awa::platform;
using namespace srhd_awa::platform::e2e4_quicksave;
namespace {
enum class Inject {none,source_open,destination_create};
Inject armed{};
std::string source_path,target_path;
std::vector<char> Bytes(const fs::path& path){
    std::ifstream f(path,std::ios::binary);assert(f);
    return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};
}
}
extern "C" FILE* __real_fopen(const char*,const char*);
extern "C" int __real_open(const char*,int,...);
extern "C" FILE* __wrap_fopen(const char* path,const char* mode){
    if(armed==Inject::source_open && source_path==path && std::string(mode)=="rb"){
        armed=Inject::none;errno=ENOENT;return nullptr;
    }
    return __real_fopen(path,mode);
}
extern "C" int __wrap_open(const char* path,int flags,...){
    mode_t mode{};
    if(flags&O_CREAT){va_list args;va_start(args,flags);mode=va_arg(args,int);va_end(args);}
    if(armed==Inject::destination_create && target_path==path && (flags&O_EXCL)){
        armed=Inject::none;errno=ENOENT;return -1;
    }
    return flags&O_CREAT?__real_open(path,flags,mode):__real_open(path,flags);
}
int main(int argc,char** argv){
    if(argc!=4)return 2;
    const auto old_source=fs::absolute(argv[1]),new_source=fs::absolute(argv[2]),output=fs::absolute(argv[3]);
    assert(!fs::exists(output));
    const auto old_bytes=Bytes(old_source),new_bytes=Bytes(new_source);
    std::string detail;
    assert(ValidateSave(old_source,detail) && ValidateSave(new_source,detail));
    for(bool occupied:{false,true})for(auto failure:{Inject::source_open,Inject::destination_create}){
        const auto root=output/(std::string(occupied?"occupied-":"new-")+(failure==Inject::source_open?"source":"destination"));
        const auto directory=root/"config"/"Save";
        fs::create_directories(directory);
        const auto temp=directory/"save.tmp",target=directory/fs::path(u"Солнце.sav");
        fs::copy_file(new_source,temp);
        if(occupied)fs::copy_file(old_source,target);
        user_root::SetRoot(root.string());
        source_path=temp.string();target_path=target.string();
        bool boundary=false;
        const auto result=ReplaceGameSavePath(temp.u16string().c_str(),target.u16string().c_str(),[&](const char* operation){
            if(std::string(operation)=="manual-publish"){boundary=true;armed=failure;}
            return true;
        });
        assert(boundary && armed==Inject::none && !result.ok);
        assert(result.detail.find(failure==Inject::source_open?"phase=source-open":"phase=destination-create")!=std::string::npos);
        assert(result.detail.find("error_code=2")!=std::string::npos && result.detail.find("copied=0")!=std::string::npos);
        // Actual UTF-8 bytes of Солнце, not a lossy ANSI rendering.
        assert(result.detail.find("d0a1d0bed0bbd0bdd186d0b5")!=std::string::npos);
        assert(Bytes(temp)==new_bytes);
        if(occupied)assert(Bytes(target)==old_bytes);else assert(!fs::exists(target));
        const auto retry=ReplaceGameSavePath(temp.u16string().c_str(),target.u16string().c_str());
        assert(retry.ok && Bytes(target)==new_bytes && ValidateSave(target,detail));
        assert(!fs::exists(temp)); // Production commit releases the closed temporary generation.
        fs::copy_file(new_source,temp); // The next real save supplies a fresh serialized generation.
        // Repeated recovery/re-entry is a real transaction, not an injected OK.
        const auto again=ReplaceGameSavePath(temp.u16string().c_str(),target.u16string().c_str());
        assert(again.ok && Bytes(target)==new_bytes && ValidateSave(target,detail));
        std::printf("%s: exact ENOENT phase, preserved temp/old slot, Unicode publish and repeated entry PASS\n",root.filename().string().c_str());
    }
    assert(Bytes(old_source)==old_bytes && Bytes(new_source)==new_bytes);
    std::puts("Captured original inputs unchanged; console ENOENT cause NOT IDENTIFIED by injected failures");
}
