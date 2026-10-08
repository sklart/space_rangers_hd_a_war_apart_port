#include "gi_object.hpp"
#include "image_object.hpp"
#include "package.hpp"
#include "presentation_scene.hpp"
#include "scene_compositor.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {
using srhd_awa::package::Package;
using srhd_awa::platform::gi_object::GIObject;
using srhd_awa::platform::image_object::Kind;
using srhd_awa::platform::image_object::PortableImageObject;
using srhd_awa::platform::presentation_scene::FramebufferFingerprint;
using srhd_awa::platform::presentation_scene::PresentationScene;
using srhd_awa::platform::scene_compositor::Framebuffer;

void U32(std::vector<std::uint8_t>* b, std::size_t at, std::uint32_t value) { for (unsigned i=0;i!=4;++i) (*b)[at+i]=static_cast<std::uint8_t>(value>>(i*8)); }
void BE16(std::vector<std::uint8_t>* b, std::size_t at, std::uint16_t value) { (*b)[at]=static_cast<std::uint8_t>(value>>8); (*b)[at+1]=static_cast<std::uint8_t>(value); }
void BE32(std::vector<std::uint8_t>* b, std::size_t at, std::uint32_t value) { for (unsigned i=0;i!=4;++i) (*b)[at+i]=static_cast<std::uint8_t>(value>>(24-i*8)); }

std::vector<std::uint8_t> Bmp(std::uint8_t red, std::uint8_t green, std::uint8_t blue, bool keyed=false) {
  const int width=keyed?2:1; std::vector<std::uint8_t> b(54+4*((width*3+3)/4)); b[0]='B';b[1]='M';U32(&b,2,b.size());U32(&b,10,54);U32(&b,14,40);U32(&b,18,width);U32(&b,22,1);b[26]=1;b[28]=24;U32(&b,34,b.size()-54);
  if(keyed){b[54]=0;b[55]=0;b[56]=0;b[57]=blue;b[58]=green;b[59]=red;}else{b[54]=blue;b[55]=green;b[56]=red;}return b;
}
std::vector<std::uint8_t> Psd() { std::vector<std::uint8_t> b(44);std::memcpy(b.data(),"8BPS",4);BE16(&b,4,1);BE16(&b,12,4);BE32(&b,14,1);BE32(&b,18,1);BE16(&b,22,8);BE16(&b,24,3);b[40]=255;b[43]=128;return b; }
std::vector<std::uint8_t> Gi(std::uint16_t color) { std::vector<std::uint8_t> b(104);std::memcpy(b.data(),"gi\0",3);U32(&b,4,1);U32(&b,16,2);U32(&b,20,2);U32(&b,24,0xf800);U32(&b,28,0x07e0);U32(&b,32,0x001f);U32(&b,44,1);U32(&b,64,96);U32(&b,68,8);U32(&b,80,2);U32(&b,84,2);for(std::size_t i=96;i<104;i+=2){b[i]=static_cast<std::uint8_t>(color);b[i+1]=static_cast<std::uint8_t>(color>>8);}return b; }
std::vector<std::uint8_t> Gai() { const auto first=Gi(0xf800),second=Gi(0x07e0);constexpr std::size_t dir=64;const auto second_at=dir+first.size(),table=second_at+second.size();std::vector<std::uint8_t>b(table+36);std::memcpy(b.data(),"gai\0",4);U32(&b,4,1);U32(&b,16,2);U32(&b,20,2);U32(&b,24,2);U32(&b,32,table);U32(&b,36,36);U32(&b,48,dir);U32(&b,52,first.size());U32(&b,56,second_at);U32(&b,60,second.size());std::memcpy(b.data()+dir,first.data(),first.size());std::memcpy(b.data()+second_at,second.data(),second.size());U32(&b,table,1);U32(&b,table+8,16);U32(&b,table+16,2);U32(&b,table+20,0);U32(&b,table+24,10);U32(&b,table+28,1);U32(&b,table+32,10);return b; }
void Entry(std::vector<std::uint8_t>* b,std::size_t at,const char* name,std::uint32_t target,std::uint32_t size){std::memset(b->data()+at,0,158);U32(b,at+4,size);std::strncpy(reinterpret_cast<char*>(b->data()+at+8),name,62);std::strncpy(reinterpret_cast<char*>(b->data()+at+71),name,62);U32(b,at+150,target);}
bool WritePackage(const char* path) { const std::vector<std::pair<const char*,std::vector<std::uint8_t>>> items={{"GI.GAI",Gai()},{"S.BMP",Bmp(0,0,255)},{"T.BMP",Bmp(0,255,0,true)},{"A.PSD",Psd()}};constexpr std::size_t root=4,dir=12+4*158;std::size_t cursor=root+dir;std::vector<std::uint8_t>b(cursor);U32(&b,0,root);U32(&b,root,dir);U32(&b,root+4,4);U32(&b,root+8,158);for(std::size_t i=0;i<items.size();++i){const auto target=cursor;b.resize(cursor+4+items[i].second.size());U32(&b,cursor,items[i].second.size());std::memcpy(b.data()+cursor+4,items[i].second.data(),items[i].second.size());Entry(&b,root+12+i*158,items[i].first,static_cast<std::uint32_t>(target),static_cast<std::uint32_t>(items[i].second.size()));cursor=b.size();}FILE*f=std::fopen(path,"wb");if(!f)return false;const bool ok=std::fwrite(b.data(),1,b.size(),f)==b.size();std::fclose(f);return ok; }
bool Same(const FramebufferFingerprint&a,const FramebufferFingerprint&b){return a.crc32==b.crc32&&a.fnv64==b.fnv64&&a.bytes==b.bytes;}
}

int main() {
  constexpr const char* path="build/test_m21_presentation.pkg"; if(!WritePackage(path))return 1; Package package;std::string error;
  GIObject gi(&package);PortableImageObject simple(&package),trans(&package),alpha(&package);bool ok=package.Open(path,&error);const bool opened=ok;
  gi.SetPosition(0,0);gi.SetLayer(0);simple.SetPosition(0,0);simple.SetLayer(10);trans.SetPosition(0,1);trans.SetLayer(20);alpha.SetPosition(2,0);alpha.SetLayer(30);
  const bool loaded=ok&&gi.LoadResource("GI.GAI",&error)&&simple.Load(Kind::Simple,"S.BMP","",&error)&&trans.Load(Kind::Trans,"T.BMP","",&error)&&alpha.Load(Kind::Alpha,"A.PSD","",&error);ok=loaded;
  PresentationScene scene;const bool entries=ok&&scene.AddGI("gi",&gi,&error)&&scene.AddImage("simple",&simple,&error)&&scene.AddImage("trans",&trans,&error)&&scene.AddImage("alpha",&alpha,&error)&&scene.EntryCount()==4;ok=entries;
  const std::vector<std::uint16_t> initial(4*3,0x001f);std::vector<std::uint16_t> pixels=initial;Framebuffer fb{pixels.data(),4,3,4};FramebufferFingerprint first{},repeat{},second{};
  const bool frame_a=ok&&scene.Render(fb,&error)&&scene.ComputeFramebufferFingerprint(fb,&first,&error);const bool placement=frame_a&&pixels[0]==0x001f&&pixels[4]==0xf800&&pixels[5]==0x07e0&&pixels[2]!=0;ok=placement;
  const auto once=pixels;pixels=initial;const bool repeat_ok=ok&&scene.Render(fb,&error)&&scene.ComputeFramebufferFingerprint(fb,&repeat,&error)&&pixels==once&&Same(first,repeat);ok=repeat_ok;
  const bool advanced=gi.Update(10,&error);pixels=initial;const bool frame_b=ok&&advanced&&scene.Render(fb,&error)&&scene.ComputeFramebufferFingerprint(fb,&second,&error)&&!Same(first,second)&&pixels[4]==0x07e0;const bool oracle_match=frame_b&&first.crc32==0x1a829653u&&first.fnv64==0x658ac816b5479db3ull&&first.bytes==24&&second.crc32==0x383a8729u&&second.fnv64==0x69ffb049d0071ebbull&&second.bytes==24;ok=oracle_match;
  std::remove(path);if(!ok){std::fprintf(stderr,"M21 PRESENTATION FAIL: open=%d load=%d entries=%d frame_a=%d placement=%d repeat=%d advanced=%d frame_b=%d oracle=%d pixels=",opened,loaded,entries,frame_a,placement,repeat_ok,advanced,frame_b,oracle_match);for(auto pixel:pixels)std::fprintf(stderr," %04x",pixel);std::fprintf(stderr," error=%s\n",error.c_str());return 1;}std::printf("M21 PRESENTATION PASS frame_a_crc32=%08x frame_a_fnv64=%016llx frame_b_crc32=%08x frame_b_fnv64=%016llx bytes=%zu\n",first.crc32,(unsigned long long)first.fnv64,second.crc32,(unsigned long long)second.fnv64,first.bytes);return 0;
}
