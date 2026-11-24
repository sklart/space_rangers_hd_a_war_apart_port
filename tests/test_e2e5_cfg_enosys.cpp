#include "e2e5_cfg_bootstrap.hpp"
#include <cassert>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sys/stat.h>
namespace fs=std::filesystem;
namespace cfg=srhd_awa::platform::e2e5_cfg_bootstrap;
static unsigned chmod_calls{};
extern "C" int fchmod(int,mode_t) noexcept {++chmod_calls;errno=ENOSYS;return -1;}
static std::string Read(const fs::path& path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char** argv){
 assert(argc==2);const fs::path root=argv[1];assert(!fs::exists(root));
 fs::create_directories(root/"game");fs::create_directories(root/"user");
 const auto source=root/"game/cfg.txt",target=root/"user/CFG.TXT";
 const std::string bytes=std::string(70000,'A')+"\nResolution=1280,720\n";
 {std::ofstream f(source,std::ios::binary);f<<bytes;}
 auto initial=cfg::Inspect(source.string(),target.string());assert(initial.problem.empty());
 cfg::Create(initial);assert(initial.created && Read(target)==bytes && Read(source)==bytes && chmod_calls==0);
 auto existing=cfg::Inspect(source.string(),target.string());assert(existing.used_existing);
 cfg::Create(existing);assert(Read(target)==bytes && chmod_calls==0);
 const auto race=root/"user/race.txt";auto racing=cfg::Inspect(source.string(),race.string());
 {std::ofstream f(race);f<<"preserve";}
 cfg::Create(racing);assert(!racing.created && racing.problem=="copy-failed" && Read(race)=="preserve");
 const auto empty=root/"user/empty.txt";{std::ofstream f(empty);}
 auto old_empty=cfg::Inspect(source.string(),empty.string());
 assert(old_empty.problem=="target-empty-or-unreadable" && fs::file_size(empty)==0);
}
