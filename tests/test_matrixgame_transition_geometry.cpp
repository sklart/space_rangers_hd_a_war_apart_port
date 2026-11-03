// Original Takt/Clear bodies, authored CFG geometry. CPU animation/ownership
// only; SDK screenshot upload/draw remains a separate hardware gate.
#ifdef NDEBUG
#error Transition checks require assertions
#endif
#include "transition_geometry.hpp"
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <map>
#include <vector>
#include <cmath>
#include <cstdio>
#include <cstring>
namespace t=srhd_awa::matrixgame::transition;
struct Vec2 {float x,y;Vec2 operator*(float s)const{return {x*s,y*s};}Vec2& operator+=(Vec2 v){x+=v.x;y+=v.y;return *this;}};
struct Geom {Vec2 pos,dir;struct Vertex {struct {float x,y,z,w;}p;} verts[4];};
struct Texture {};
unsigned frees{},texture_frees{};int g_MatrixHeap{};
void HFree(void* p,int){if(p){++frees;std::free(p);}}
struct Cache {void Destroy(Texture* p){assert(p);++texture_frees;delete p;}} cache;Cache* g_Cache=&cache;
struct Map {unsigned m_Flags{};} map;Map* g_MatrixMap=&map;
int g_ScreenX{},g_ScreenY{};
#define MMFLAG_TRANSITION 1u
#define RESETFLAG(flags,mask) ((flags)&=~(mask))
#define DTRACE() ((void)0)
class CTransition {
public:
 Texture* m_Tex{};Geom* m_Geom{};int m_GeomCnt{};
 void Clear();void Takt(int);void OriginalClear();~CTransition(){Clear();}
};
#include "original-takt.inc"
#include "candidate-clear.inc"
#include "baseline-clear.inc"
struct Block {std::vector<Geom> geometry;int ParCount()const{return int(geometry.size());}};
using Blocks=std::map<std::wstring,Block>;
template<class F>void Reject(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}assert(caught);}
int main(int argc,char** argv){
 assert(argc==3);std::ifstream input(argv[1]);assert(input);Blocks tables;unsigned layouts{};input>>layouts;assert(layouts==2);
 for(unsigned table=0;table<layouts;++table){int w,h,n;input>>w>>h>>n;assert(input && n>0);auto& b=tables[std::to_wstring(w)+L"."+std::to_wstring(h)];b.geometry.resize(n);
  for(auto& g:b.geometry){input>>g.dir.x>>g.dir.y;for(auto& v:g.verts)input>>v.p.x>>v.p.y;assert(input);}}
 auto lookup=[&](const std::wstring& name)->Block*{auto p=tables.find(name);return p==tables.end()?nullptr:&p->second;};
 // Frozen r9 behavior at 1280x720: no exact authored block, empty Takt cannot
 // clear the transition flag, even after 500 real 100-ms algorithm steps.
 assert(!lookup(L"1280.720"));{CTransition old;map.m_Flags=1;g_ScreenX=1280;g_ScreenY=720;for(unsigned i=0;i<500;++i)old.Takt(100);assert(map.m_Flags==1 && old.m_GeomCnt==0);}
 std::ofstream trace(argv[2]);assert(trace);trace<<"width,height,tick,remaining,transition\n";unsigned cases{};
 for(auto size:std::array<std::array<int,2>,6>{{{1024,768},{800,600},{640,480},{1280,720},{1920,1080},{1023,777}}}){
  g_ScreenX=size[0];g_ScreenY=size[1];auto layout=t::ResolveGeometry(g_ScreenX,g_ScreenY,lookup);
  if(cases<2)assert(layout.width==g_ScreenX && layout.height==g_ScreenY && layout.x_scale==1 && layout.y_scale==1);
  else assert(layout.width==1024 && layout.height==768);
  CTransition animation;animation.m_GeomCnt=layout.block->ParCount();animation.m_Geom=static_cast<Geom*>(std::calloc(animation.m_GeomCnt,sizeof(Geom)));assert(animation.m_Geom);animation.m_Tex=new Texture;
  for(int i=0;i<animation.m_GeomCnt;++i){auto& dst=animation.m_Geom[i];const auto& src=layout.block->geometry[i];dst=src;dst.dir.x*=layout.x_scale;dst.dir.y*=layout.y_scale;
   for(unsigned j=0;j<4;++j){dst.verts[j].p.x*=layout.x_scale;dst.verts[j].p.y*=layout.y_scale;
    assert(std::abs(dst.verts[j].p.x/g_ScreenX-src.verts[j].p.x/layout.width)<2e-6f);assert(std::abs(dst.verts[j].p.y/g_ScreenY-src.verts[j].p.y/layout.height)<2e-6f);}
   if(layout.x_scale==1 && layout.y_scale==1)assert(std::memcmp(&dst,&src,sizeof(Geom))==0);}
  map.m_Flags=1;unsigned tick{};trace<<g_ScreenX<<','<<g_ScreenY<<",0,"<<animation.m_GeomCnt<<",1\n";
  while(map.m_Flags && tick<500){animation.Takt(100);++tick;trace<<g_ScreenX<<','<<g_ScreenY<<','<<tick<<','<<animation.m_GeomCnt<<','<<map.m_Flags<<'\n';}
  assert(!map.m_Flags && !animation.m_Geom && !animation.m_Tex && tick>0 && tick<100);++cases;
 }
 {CTransition partial;partial.m_Geom=static_cast<Geom*>(std::calloc(1,sizeof(Geom)));partial.m_GeomCnt=1;partial.OriginalClear();assert(partial.m_Geom && partial.m_GeomCnt==1);const auto before=frees;partial.Clear();assert(!partial.m_Geom && !partial.m_GeomCnt && frees==before+1);partial.Clear();assert(frees==before+1);}
 Reject([&]{t::ResolveGeometry(0,720,lookup);});Reject([&]{t::ResolveGeometry(1280,720,[](const std::wstring&)->Block*{return nullptr;});});
 Block empty;Reject([&]{t::ResolveGeometry(1280,720,[&](const std::wstring& name)->Block*{return name==L"1280.720"?&empty:lookup(name);});});
 assert(texture_frees==cases && frees==cases+1);
 std::printf("TRANSITION_GEOMETRY PASS layouts=2 cases=%u original_empty_500_ticks_stuck exact_layouts_preserved scaled_authored_vertices_velocity original_Takt_completes partial_Clear_fixed missing_empty_rejected CPU_ONLY NO_GPU_CAPTURE_OR_DRAW\n",cases);
}
