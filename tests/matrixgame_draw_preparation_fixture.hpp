// Same per-draw CPU order as Record, excluding SDK/device operations.
#include "native_device_state.hpp"
#include "material_uniform_packet.hpp"
namespace p=srhd_awa::matrixgame::platform3g;
namespace m=srhd_awa::matrixgame::materials;
struct DrawData {std::array<std::byte,m::uniform_packet::Bytes> packet;srhd_awa::matrixgame::MeshStencilState stencil;};
DrawData Prepare(const p::NativeRenderState& s,bool points=false,bool lines=false,unsigned samples=0,unsigned quality=0,bool alpha_target=false){
#ifdef PREPARED_DRAW
 const auto draw=s.PrepareDraw(points,lines,samples,quality);const auto stencil=draw.stencil;
#else
 s.ValidateDraw(points,lines,samples,quality);const auto stencil=s.Stencil();
#endif
 srhd_awa::matrixgame::RequireStencilAttachment(stencil,true,75);
 const auto vertex=s.vertex.Prepare();
#ifdef PREPARED_DRAW
 const auto fog=draw.fog;auto combiner=draw.combiner;
#else
 const auto fog=s.PixelFog();
#endif
 if(s.shade!=1 && s.shade!=2)throw m::Incomplete("native material shading mode outside FLAT/GOURAUD");
#ifndef PREPARED_DRAW
 auto combiner=s.combiner.Prepare();
#endif
 m::AlphaTarget(combiner,alpha_target);DrawData out;out.stencil=stencil;
 m::uniform_packet::Write(out.packet,combiner,&vertex,fog,s.shade);return out;
}
