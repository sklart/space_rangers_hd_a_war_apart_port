// Differential probe plus independent byte-coverage model. No GPU emulation.
#include "buffer_resources.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
using namespace srhd_awa::matrixgame::buffers;
static bool deny_heap=false;
void* operator new(std::size_t n){if(deny_heap)throw std::bad_alloc();if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
template<class E,class F>void rejects(F action,const char* message){bool caught=false;try{action();}catch(const E& e){caught=true;assert(std::strcmp(e.what(),message)==0);}assert(caught);}
int main(){
    Context c(1024);Scope scope(c);auto* b=c.CreateVertex(64,0x112,Pool::Default,true);assert(b);
    std::array<bool,64> initialized{};std::array<std::byte,64> bytes{};
    std::uint32_t rng=0x842519;unsigned cases=0,queries=0;std::uint64_t hash=0,version=0;
    auto update=[&](unsigned start,unsigned size,MapMode mode){
        auto* p=static_cast<std::byte*>(b->Map(start,size,mode));
        if(mode==MapMode::Discard){initialized.fill(false);bytes.fill(std::byte{});}
        assert(b->Locked()&&b->Version()==version&&!b->Initialized(0,0));
        rejects<std::logic_error>([&]{b->Map(0,0);},"buffer already mapped");
        rejects<std::logic_error>([&]{b->Storage();},"mapped buffer cannot be consumed");
        for(unsigned i=0;i<size;++i){rng=rng*1664525u+1013904223u;p[i]=std::byte(rng>>24);bytes[start+i]=p[i];initialized[start+i]=true;}
        deny_heap=true;b->Unmap();deny_heap=false;++version;++cases;
        assert(!b->Locked()&&b->Version()==version);
        assert(std::memcmp(b->Storage().data(),bytes.data(),64)==0);
        for(unsigned off=0;off<=65;++off)for(unsigned len=0;len<=65;++len){
            bool expected=off<=64&&len<=64-off;
            if(expected)for(unsigned i=0;i<len;++i)expected=expected&&initialized[off+i];
            assert(b->Initialized(off,len)==expected);++queries;
        }
        for(auto x:bytes)hash=(hash^std::to_integer<unsigned>(x))*1099511628211ull;
    };
    // First/later intervals, subsets, adjacency, holes, merging and Discard.
    for(auto range: {std::array<unsigned,2>{8,8},{40,8},{10,3},{41,2},{16,8},{24,16},{0,8},{48,16},{0,64}})update(range[0],range[1],MapMode::Normal);
    for(unsigned i=0;i<1024;++i){rng=rng*1664525u+1013904223u;unsigned off=rng%64;rng=rng*1664525u+1013904223u;unsigned len=1+rng%(64-off);update(off,len,i%31==0?MapMode::Discard:i%3==0?MapMode::NoOverwrite:MapMode::Normal);}
    rejects<std::logic_error>([&]{b->Unmap();},"buffer not mapped");
    rejects<std::out_of_range>([&]{b->Map(64,1);},"buffer map range");
    rejects<std::out_of_range>([&]{b->Map(1,0);},"buffer map range");
    rejects<std::invalid_argument>([&]{b->Map(0,1,static_cast<MapMode>(99));},"buffer map mode");
    auto* s=c.CreateVertex(64,0x112);assert(s);
    rejects<std::invalid_argument>([&]{s->Map(0,1,MapMode::NoOverwrite);},"dynamic buffer map required");
    rejects<std::invalid_argument>([&]{s->Map(0,1,MapMode::Discard);},"dynamic buffer map required");
    deny_heap=true;bool oom=false;try{s->Map(0,1);}catch(const std::bad_alloc&){oom=true;}deny_heap=false;
    assert(oom&&!s->Locked()&&s->Version()==0&&!s->Initialized(0,1));
    s->Map(0,0);deny_heap=true;s->Unmap();deny_heap=false;assert(s->Initialized(0,64)&&s->Version()==1);
    s->Release();c.BindVertex(b,0,32);auto snapshot=c.CaptureBindings();b->Release();c.ClearBindings();
    assert(c.AllocatedBytes()==64&&snapshot.CanRestore()&&snapshot.Restore());snapshot={};c.ClearBindings();assert(c.AllocatedBytes()==0);
    std::printf("UNMAP cases=%u queries=%u version=%llu checksum=%llu allocation_failure=1 allocation_free_unmap=1 retained_binding=1\n",cases,queries,(unsigned long long)version,(unsigned long long)hash);
}
