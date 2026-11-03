#ifdef NDEBUG
#error Keep benchmark assertions
#endif
#include "matrixgame_decode_target_fixture.hpp"
#include <chrono>
int main(){using Clock=std::chrono::steady_clock;mat::FixedVertexState s;s.Layout(0x112);auto input=Payload(0x112,s.Stride(),1);std::vector<mat::Vertex> out(2048);
 std::uint64_t hash=1469598103934665603ull;auto start=Clock::now();
 for(unsigned i=0;i<10000000;++i){const float x=float(i%1024)/1024;std::memcpy(input.data(),&x,4);ProbeDecode(s,out[i%out.size()],input,s.Stride());hash=(hash^std::bit_cast<unsigned>(out[i%out.size()].position[0]))*1099511628211ull;}
 auto decode=std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-start).count();
 IndexedHarness h;h.data.state.Edit().vertex.Layout(0x112);const auto stride=s.Stride()+4;auto bytes=Payload(0x112,stride,2048);std::vector<unsigned> indices(6144);for(unsigned i=0;i<indices.size();++i)indices[i]=i%2048;
 buffers::Context context;buffers::Scope scope(context);Bind(context,bytes,0x112,stride,indices,buffers::IndexWidth::U16);start=Clock::now();
 for(unsigned i=0;i<4096;++i)assert(h.Gather(4,0,0,2048,0,2048)==0);
 auto gather=std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-start).count();hash^=h.data.checksum;context.ClearBindings();assert(context.AllocatedBytes()==0);
 std::printf("DECODE_PROFILE direct_vertices=10000000 indexed_draws=4096 decoded_vertices=8388608 decode_us=%lld gather_us=%lld checksum=%llu CPU_GATHER_ONLY_NOT_GPU\n",(long long)decode,(long long)gather,(unsigned long long)hash);
}
