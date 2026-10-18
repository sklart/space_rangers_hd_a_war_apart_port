// SPDX-License-Identifier: GPL-2.0-or-later
// Same legal-domain trace runs against original x86 and native SRemindCore.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include "CReminder.hpp"
#ifndef MATRIXGAME_ORIGINAL_REMINDER
#include "vector_geometry.hpp"
#include "config_unicode.hpp"
#endif
using Base::SRemindCore;
#define CHECK(e) do {if(!(e)){std::fprintf(stderr,"CHECK %d: %s\n",__LINE__,#e);std::exit(3);}}while(0)
std::ofstream trace;
void u32(std::uint32_t x){trace.write(reinterpret_cast<const char*>(&x),4);}
void i64(std::int64_t x){trace.write(reinterpret_cast<const char*>(&x),8);}
std::uint32_t state=0x73521;
std::uint32_t draw(){state=state*1664525u+1013904223u;return state;}
struct Node {
  enum Mode {Plain,SelfDeleteFalse,SelfDeleteTrue,DownOnly,Rearm,Fault,Recurse,ReleaseGeometry,Reuse};
  unsigned id,calls{};Mode mode{Plain};
  SRemindCore core;
#ifndef MATRIXGAME_ORIGINAL_REMINDER
  std::unique_ptr<srhd_awa::matrixgame::VectorGeometry> geometry;
#endif
  explicit Node(unsigned i):id(i),core(Callback,reinterpret_cast<std::uintptr_t>(this)){}
  static bool Callback(std::uintptr_t data) {
    auto* p=reinterpret_cast<Node*>(data);++p->calls;
    if(trace.is_open()){u32(1);u32(p->id);i64(SRemindCore::gtime);i64(SRemindCore::ctime);i64(p->core.time);}
    const auto mode=p->mode;
    if(mode==SelfDeleteFalse||mode==SelfDeleteTrue){delete p;return mode==SelfDeleteTrue;}
    if(mode==DownOnly)p->core.Down();
#ifndef MATRIXGAME_ORIGINAL_REMINDER
    if(mode==Rearm){p->core.Down();p->core.Use(50);}
    if(mode==Reuse){const auto id=p->id;p->~Node();new(p)Node(id);p->core.Use(50);}
    if(mode==Fault)throw std::runtime_error("controlled resource cleanup failure");
    if(mode==Recurse){bool rejected=false;try{SRemindCore::Takt(1);}catch(const std::logic_error&){rejected=true;}CHECK(rejected);}
    if(mode==ReleaseGeometry)p->geometry.reset();
#endif
    return false;
  }
};
bool Linked(const Node& n){for(auto* p=SRemindCore::first;p;p=p->next)if(p==&n.core)return true;return false;}
void SelfDelete(bool dead) {
  SRemindCore::StaticInit();
  Node a(0),c(2);auto b=std::make_unique<Node>(1);
  b->mode=dead?Node::SelfDeleteTrue:Node::SelfDeleteFalse;
  a.core.Use(0);b->core.Use(0);c.core.Use(0);b.release();
  SRemindCore::Takt(1); // Original round-robin visits the second entry first.
  CHECK(Linked(a)&&Linked(c));CHECK(SRemindCore::first==&a.core&&SRemindCore::last==&c.core);
  CHECK(a.core.next==&c.core&&c.core.prev==&a.core);
}
void SelfDown() {
  SRemindCore::StaticInit();Node a(0),b(1),c(2);b.mode=Node::DownOnly;
  a.core.Use(0);b.core.Use(0);c.core.Use(0);SRemindCore::Takt(1);
  CHECK(!Linked(b)&&Linked(a)&&Linked(c));
  CHECK(a.core.next==&c.core&&c.core.prev==&a.core);
}
void Common(const std::filesystem::path& output) {
  SRemindCore::StaticInit();std::filesystem::create_directories(output);
  trace.open(output/"reminder-trace.bin",std::ios::binary);CHECK(bool(trace));
  std::vector<std::unique_ptr<Node>> nodes;
  for(unsigned i=0;i<12;++i)nodes.push_back(std::make_unique<Node>(i));
  for(unsigned step=0;step<5000;++step) {
    const auto op=draw()%8,index=draw()%12,value=draw();
    if(op<4)nodes[index]->core.Use(int(value%(step%7?401u:40001u)));
    else if(op==4)nodes[index]->core.Down();
    else SRemindCore::Takt(int(value%101));
    u32(0);u32(step);i64(SRemindCore::gtime);i64(SRemindCore::ctime);
    unsigned current=0xffffffffu,count=0;
    for(unsigned i=0;i<nodes.size();++i)if(SRemindCore::current==&nodes[i]->core)current=i;
    for(auto* p=SRemindCore::first;p;p=p->next){CHECK(++count<=12);CHECK(!p->next||p->next->prev==p);}
    u32(current);u32(count);
    for(auto* p=SRemindCore::first;p;p=p->next){unsigned id=12;for(unsigned i=0;i<nodes.size();++i)if(p==&nodes[i]->core)id=i;CHECK(id<12);u32(id);}
    for(const auto& p:nodes)i64(p->core.time);
  }
  trace.close();CHECK(bool(trace));nodes.clear();CHECK(!SRemindCore::first&&!SRemindCore::last&&!SRemindCore::current);
}
#ifndef MATRIXGAME_ORIGINAL_REMINDER
template<class E,class F> void Reject(F f){bool rejected=false;try{f();}catch(const E&){rejected=true;}CHECK(rejected);}
void Native(const char* geometry) {
  SelfDown();SelfDelete(false);SelfDelete(true);
  {
    SRemindCore::StaticInit();Node a(0),b(1),c(2);b.mode=Node::Rearm;
    a.core.Use(0);b.core.Use(0);c.core.Use(0);SRemindCore::Takt(1);
    CHECK(Linked(a)&&Linked(b)&&Linked(c)&&b.core.time==51);
    Reject<std::logic_error>([]{SRemindCore::StaticInit();});
  }
  {
    SRemindCore::StaticInit();Node a(0),c(2);auto b=std::make_unique<Node>(1);b->mode=Node::Reuse;
    a.core.Use(0);b->core.Use(0);c.core.Use(0);SRemindCore::Takt(1);
    CHECK(Linked(a)&&Linked(*b)&&Linked(c)&&b->core.time==51&&b->calls==0);
    CHECK(a.core.next==&c.core&&c.core.next==&b->core);
  }
  {
    SRemindCore::StaticInit();Node node(0);node.mode=Node::Fault;node.core.Use(0);
    Reject<std::runtime_error>([]{SRemindCore::Takt(1);});
    CHECK(SRemindCore::IsIdle()&&Linked(node));
    node.mode=Node::Recurse;SRemindCore::Takt(10);CHECK(!Linked(node)&&node.calls==2);
  }
  {
    SRemindCore::StaticInit();Node node(0);
    if(sizeof(void*)>4)CHECK(node.core.param>UINT32_MAX);
    SRemindCore::gtime=INT32_MAX-10;SRemindCore::ctime=SRemindCore::gtime;
    node.core.Use(30);CHECK(node.core.time==std::int64_t(INT32_MAX)+20);
    SRemindCore::Takt(20);CHECK(!node.calls&&Linked(node));
    SRemindCore::Takt(11);CHECK(node.calls==1&&!Linked(node));
    Reject<std::invalid_argument>([&]{node.core.Use(-1);});
    Reject<std::invalid_argument>([]{SRemindCore::Takt(-1);});
    node.core.handler=nullptr;Reject<std::invalid_argument>([&]{node.core.Use(1);});
    node.core.handler=Node::Callback;SRemindCore::gtime=INT64_MAX-1;
    Reject<std::overflow_error>([&]{node.core.Use(2);});CHECK(!Linked(node));
    node.core.Use(0);Reject<std::overflow_error>([]{SRemindCore::Takt(2);});
  }
  if(geometry) {
    SRemindCore::StaticInit();Node node(0);node.mode=Node::ReleaseGeometry;
    const auto path=::matrixgame::native::ConfigWide(geometry);
    node.geometry=std::make_unique<srhd_awa::matrixgame::VectorGeometry>(srhd_awa::matrixgame::VectorGeometry::Load(path.c_str()));
    CHECK(!node.geometry->Vertices().empty());node.core.Use(30000);
    SRemindCore::Takt(30000);CHECK(node.geometry&&node.calls==0);
    SRemindCore::Takt(1);CHECK(!node.geometry&&node.calls==1&&!Linked(node));
  }
  SRemindCore::StaticInit();
}
#endif
int main(int argc,char** argv) {
  if(argc==2&&std::string(argv[1])=="--self-delete-false"){SelfDelete(false);return 0;}
  if(argc==2&&std::string(argv[1])=="--unlink-self"){SelfDown();return 0;}
  if(argc<2||argc>3)return 2;
  try {
    Common(std::filesystem::u8path(argv[1]));
#ifndef MATRIXGAME_ORIGINAL_REMINDER
    Native(argc==3?argv[2]:nullptr);
#endif
    std::puts("reminder trace PASS operations=5000; battle_executed=false");return 0;
  }catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 2;}
}
