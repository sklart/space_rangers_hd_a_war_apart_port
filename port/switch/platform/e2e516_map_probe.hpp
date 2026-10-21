#pragma once
// Opt-in host observation of real ship controls. No game-state mutations.
#if defined(E2E5_HOST_SDL_WINDOW)
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
namespace srhd_awa::platform::map516 {
struct Rect { int l,t,r,b; };
struct Ship { int id; double x,y; const void* control; Rect rect; };
inline std::array<Ship,128> ships{};
inline unsigned count{},frame{},rows{};
inline int turn{},film{},mode{};
inline const void* pixels{};
inline int pitch{},width{},height{};
inline bool enabled{},sample{};
inline FILE* file{};
inline std::uint64_t Hash(Rect r) {
  if(!pixels)return 0;
  r.l=std::clamp(r.l,0,width);r.r=std::clamp(r.r,0,width);
  r.t=std::clamp(r.t,0,height);r.b=std::clamp(r.b,0,height);
  std::uint64_t h=UINT64_C(14695981039346656037);
  for(int y=r.t;y<r.b;++y)for(int x=r.l*2;x<r.r*2;++x) {
    h^=static_cast<const unsigned char*>(pixels)[y*pitch+x];h*=UINT64_C(1099511628211);
  }
  return h;
}
inline void Row(const char* phase,const Ship& s,Rect r,std::uint64_t before=0,int dirty=-1) {
  if(!file || rows++>=50000)return;
  std::fprintf(file,"%s,%u,%d,%d,%d,%d,%.9f,%.9f,%p,%d,%d,%d,%d,%llu,%llu,%d\n",
    phase,frame,turn,film,mode,s.id,s.x,s.y,s.control,r.l,r.t,r.r,r.b,
    static_cast<unsigned long long>(before),static_cast<unsigned long long>(Hash(r)),dirty);
  std::fflush(file);
}
inline void Begin(int t,int f,int m,const void* p,int stride,int w,int h) {
  static bool initialized{};static int previous_turn{-1},previous_mode{-1};static unsigned pending{};
  if(!initialized){initialized=true;enabled=std::getenv("E2E516_MAP_PROBE");
    if(enabled)file=std::fopen("map516.csv","wx");}
  if(!enabled)return;
  if(t!=previous_turn || m!=previous_mode)pending=2;
  previous_turn=t;previous_mode=m;
  turn=t;film=f;mode=m;++frame;sample=pending>0 || frame%60==0;
  if(pending)--pending;
  count=0;pixels=p;pitch=stride;width=w;height=h;
}
inline void Add(int id,double x,double y,const void* control,Rect r,int dirty) {
  if(!enabled || !sample || count==ships.size())return;
  auto& s=ships[count++];s={id,x,y,control,r};Row("model-invalidation",s,r,0,dirty);
}
inline void Phase(const char* phase) {
  if(!enabled || !sample)return;
  for(unsigned i=0;i<count;++i)Row(phase,ships[i],ships[i].rect);
}
inline void Present(const void* p,int stride,int w,int h) {
  if(!enabled || !sample)return;
  const auto* old=pixels;const int old_pitch=pitch;
  pixels=p;pitch=stride;width=w;height=h;Phase("present-input");pixels=old;pitch=old_pitch;
}
struct Draw {
  const Ship* ship{};Rect rect{};std::uint64_t before{};
  Draw(const void* control,Rect r) {
    if(!enabled || !sample)return;
    for(unsigned i=0;i<count;++i)if(ships[i].control==control){ship=&ships[i];rect=r;before=Hash(r);break;}
  }
  ~Draw(){if(ship)Row("ship-draw",*ship,rect,before);}
};
}
#endif
