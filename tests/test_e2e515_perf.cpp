#include "e2e515_perf.hpp"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
using namespace srhd_awa::platform;
std::thread::id producer;
std::atomic<bool> check_io{};
extern "C" size_t __real_fwrite(const void*,size_t,size_t,FILE*);
extern "C" int __real_fflush(FILE*);
extern "C" size_t __wrap_fwrite(const void* p,size_t a,size_t b,FILE* f){
 assert(!check_io||std::this_thread::get_id()!=producer);return __real_fwrite(p,a,b,f);
}
extern "C" int __wrap_fflush(FILE* f){
 assert(!check_io||std::this_thread::get_id()!=producer);return __real_fflush(f);
}
int main(int argc,char**argv){
 assert(argc==3);const auto root=std::filesystem::path(argv[1]);
 std::filesystem::create_directories(root);
 {std::ofstream f(root/"perf-mode.txt");f<<argv[2];}
 {std::ofstream f(root/"scene-mode.txt");f<<"on\n";}
 producer=std::this_thread::get_id();perf515::Initialize(argv[1]);check_io=true;
 if(std::string(argv[2])=="off")assert(perf515::Mode()==0);else assert(perf515::Mode()==2);
 {
   perf515::Scope outer("test_outer");
   {perf515::Scope inner("test_inner",true);std::this_thread::sleep_for(std::chrono::milliseconds(2));}
   perf515::Frame(11);perf515::Record("logical_tick");perf515::TickBatch(900,20);perf515::Frame(11);
 }
 std::vector<std::uint16_t> pixels(640*360,0xf800);
 perf515::SceneImage("compose-before",16,1,&pixels,&pixels,pixels.data(),1280,640,360,100,100,301,0,0);
 // The writer must own a copy: the caller may immediately reuse/free its buffer.
 std::fill(pixels.begin(),pixels.end(),0x001f);
 for(int i=0;i<200;++i)
  perf515::SceneImage("compose-after",16,1,&pixels,&pixels,pixels.data(),1280,640,360,100,100,301,i,0);
 // Overload must drop samples, never block the game thread for filesystem I/O.
 for(int i=0;i<100000;++i)perf515::Record("pressure");
 std::this_thread::sleep_for(std::chrono::milliseconds(1100));
 perf515::Shutdown();check_io=false;
 std::ifstream input(root/"logs/perf515.csv");const std::string csv((std::istreambuf_iterator<char>(input)),{});
 if(perf515::Mode()){
  assert(csv.find("test_inner,")!=csv.npos&&csv.find("test_outer,")!=csv.npos);
  assert(csv.find("anchor,")!=csv.npos&&csv.find("heartbeat,")!=csv.npos);
 }else assert(csv.find("frame,")==csv.npos&&csv.find("test_outer,")==csv.npos);
 assert(std::filesystem::file_size(root/"logs/perf515.csv")<16*1024*1024);
 unsigned snapshots{};bool red_copy{};
 for(const auto& entry:std::filesystem::directory_iterator(root/"logs")) {
  if(entry.path().extension()!=".ppm")continue;
  ++snapshots;
  std::ifstream image(entry.path(),std::ios::binary);std::string header;
  std::getline(image,header);assert(header=="P6");
  std::getline(image,header);assert(header=="640 360");
  std::getline(image,header);assert(header=="255");
  unsigned char rgb[3]{};image.read(reinterpret_cast<char*>(rgb),3);
  if(rgb[0]==255 && rgb[1]==0 && rgb[2]==0)red_copy=true;
  assert(std::filesystem::file_size(entry.path())==640*360*3+15);
 }
 if(perf515::Mode()){assert(snapshots>0 && snapshots<=48 && red_copy);}
 else assert(!snapshots);
 std::puts("PASS async producer I/O guard, overload, spans, heartbeat, mode and shutdown; host only");
}
