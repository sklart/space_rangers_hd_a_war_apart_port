// Inject actual calloc/realloc failure into the original packing containers.
#include "BigVB.hpp"
#include "BigIB.hpp"
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <vector>
static int fail_calloc=0,fail_realloc=0;
extern "C" void* __real_calloc(std::size_t,std::size_t);
extern "C" void* __real_realloc(void*,std::size_t);
extern "C" void* __wrap_calloc(std::size_t count,std::size_t size) {
    if(fail_calloc>0 && --fail_calloc==0) return nullptr;
    return __real_calloc(count,size);
}
extern "C" void* __wrap_realloc(void* old,std::size_t size) {
    if(fail_realloc>0 && --fail_realloc==0) return nullptr;
    return __real_realloc(old,size);
}
struct Vertex {float values[8];static constexpr std::uint32_t FVF=0x112;};
int main() {
    using namespace srhd_awa::matrixgame::buffers;
    Context context;Scope scope(context);int failures=0;
    for(int allocation=1;allocation<=2;++allocation) {
        Vertex data[3]{};SBigVBSource<Vertex> source;source.verts=data;source.size=sizeof(data);
        auto* manager=CBigVB<Vertex>::NewBigVB(nullptr);fail_calloc=allocation;bool rejected=false;
        try {manager->AddSource(&source);}catch(const Base::CException&){rejected=true;}
        fail_calloc=0;assert(rejected);++failures;CBigVB<Vertex>::DestroyBigVB(manager);
        std::uint16_t indices[3]{};SBigIBSource isource;isource.inds=indices;isource.size=sizeof(indices);
        auto* imanager=CBigIB::NewBigIB(nullptr);fail_calloc=allocation;rejected=false;
        try {imanager->AddSource(&isource);}catch(const Base::CException&){rejected=true;}
        fail_calloc=0;assert(rejected);++failures;CBigIB::DestroyBigIB(imanager);
    }
    {
        Vertex data[3]{};SBigVBSource<Vertex> a,b;a.verts=b.verts=data;a.size=b.size=sizeof(data);
        auto* manager=CBigVB<Vertex>::NewBigVB(nullptr);manager->AddSource(&a);manager->BeforeDraw();assert(a.Select(manager)==0);
        fail_realloc=1;bool rejected=false;
        try {manager->AddSource(&b);}catch(const Base::CException&){rejected=true;}
        fail_realloc=0;assert(rejected);++failures;manager->BeforeDraw();assert(a.Select(manager)==0);
        CBigVB<Vertex>::DestroyBigVB(manager);context.ClearBindings();
        std::uint16_t indices[3]{};SBigIBSource ia,ib;ia.inds=ib.inds=indices;ia.size=ib.size=sizeof(indices);
        auto* imanager=CBigIB::NewBigIB(nullptr);imanager->AddSource(&ia);imanager->BeforeDraw();assert(ia.Select(imanager)==0);
        fail_realloc=1;rejected=false;
        try {imanager->AddSource(&ib);}catch(const Base::CException&){rejected=true;}
        fail_realloc=0;assert(rejected);++failures;imanager->BeforeDraw();assert(ia.Select(imanager)==0);
        CBigIB::DestroyBigIB(imanager);context.ClearBindings();
    }
    assert(context.AllocatedBytes()==0);
    std::printf("PASS actual packing allocation failure cleanup cases=%d; battle_executed=false\n",failures);
}
