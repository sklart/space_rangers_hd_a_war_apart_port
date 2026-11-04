#include "stencil_state.hpp"
// Benchmark/semantic forwarding boundary: execute the real production validator.
#if defined(__GNUC__)
__attribute__((noinline))
#elif defined(_MSC_VER)
__declspec(noinline)
#endif
void StencilProbe(const srhd_awa::matrixgame::MeshStencilState& state){state.Validate();}
