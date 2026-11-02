#include "../port/matrixgame/native-engine/rangers_canvas_pixels.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace srhd_awa::matrixgame::rangers_canvas;
template<class F>void Reject(F&& f){bool rejected=false;try{f();}catch(const std::invalid_argument&){rejected=true;}assert(rejected);}
int main(){
 const auto plan=Layout(520,256,256,1280,720);
 assert(plan.x==280 && plan.y==0 && plan.draw_width==720 && plan.draw_height==720);
 std::vector<std::byte> source(plan.source_bytes,std::byte{0x73});
 for(unsigned y=0;y<256;++y)for(unsigned x=0;x<256;++x){const auto v=std::uint16_t((y<<8)|x);std::memcpy(source.data()+y*520+x*2,&v,2);}
 std::vector<std::byte> target(528*256,std::byte{0x49});const auto saved=source;
 CopyRows(plan,source,520,target,528);assert(source==saved);
 for(unsigned y=0;y<256;++y){
  for(unsigned x=0;x<256;++x){std::uint16_t v{};std::memcpy(&v,target.data()+y*528+x*2,2);assert(v==((y<<8)|x));}
  for(unsigned x=512;x<528;++x)assert(target[y*528+x]==std::byte{0x49});
 }
 const auto unchanged=target;
 Reject([&]{CopyRows(plan,std::span(source).first(source.size()-1),520,target,528);});assert(target==unchanged);
 Reject([&]{CopyRows(plan,source,522,target,528);});assert(target==unchanged);
 Reject([&]{CopyRows(plan,source,520,std::span(target).first(528*255+511),528);});assert(target==unchanged);
 Reject([&]{CopyRows(plan,source,520,target,511);});assert(target==unchanged);
 Reject([]{Layout(-1,256,256,1280,720);});Reject([]{Layout(511,256,256,1280,720);});
 Reject([]{Layout(512,0,256,1280,720);});Reject([]{Layout(40000,16385,1,1280,720);});
 Reject([]{Layout(2,1,16384,16384,1);});
 const auto widescreen=Layout(2048,1024,768,1280,720);assert(widescreen.x==160 && widescreen.y==0 && widescreen.draw_width==960 && widescreen.draw_height==720);
 const auto single=Layout(99,1,1,1280,720);assert(single.source_bytes==2);
 std::puts("RANGERS_CANVAS all_65536_RGB565_words=PASS padded_rows=PASS source_unchanged=PASS rejection_before_write=PASS aspect_fit=PASS");
}
