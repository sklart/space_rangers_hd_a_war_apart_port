// Real D3D9 D24S8 and both production shaders. Read stencil through an eight-bit
// D3D color resolve, independently of direct GL stencil readback.
#define MATRIXGAME_FOG_DRIVER_ONLY
#include "matrixgame_fog_windows.cpp"
namespace m=srhd_awa::matrixgame;
struct StencilGolden {unsigned enabled,two_sided,clockwise,depth_pass,initial,reference,read_mask,write_mask;unsigned cw[4],ccw[4];unsigned value,color_pass;};
static_assert(sizeof(StencilGolden)==72);
std::vector<std::byte> Raw(std::span<const mat::Vertex> vertices){std::vector<std::byte> out;
    for(const auto& v:vertices){auto append=[&](auto value){const auto* p=reinterpret_cast<const std::byte*>(&value);out.insert(out.end(),p,p+sizeof(value));};
        for(auto f:v.position)append(f);auto byte=[](float x){return unsigned(std::lround(x*255));};append((byte(v.diffuse[3])<<24)|(byte(v.diffuse[0])<<16)|(byte(v.diffuse[1])<<8)|byte(v.diffuse[2]));}
    return out;
}
mat::Color Unpack(unsigned c){return {float((c>>16)&255)/255,float((c>>8)&255)/255,float(c&255)/255,float(c>>24)/255};}
std::vector<mat::Vertex> Triangle(bool cw){std::vector<mat::Vertex> v(3);v[0].position={-8,-8,.25f,1};v[1].position={24,-8,.25f,1};v[2].position={-8,24,.25f,1};for(auto& a:v)a.diffuse=Unpack(0xff31a7d3);if(!cw)std::swap(v[1],v[2]);return v;}
int main(int argc,char** argv){assert(argc==3);std::setvbuf(stdout,nullptr,_IONBF,0);Hal hal;MaterialGlReference gl(argv[1]);std::ofstream golden(argv[2],std::ios::binary);assert(golden);
    std::ofstream scenes(std::filesystem::path(argv[2]).parent_path()/"shadow-scenes.bin",std::ios::binary);assert(scenes);
    mat::FixedVertexState vertex;vertex.Layout(0x44);vertex.Viewport({0,0,16,16,0,1});const auto uniforms=vertex.Prepare();const mat::PixelFogUniforms fog;
    mat::Combiner combiner;combiner.Set(0,1,2);combiner.Set(0,2,0);combiner.Set(0,4,2);combiner.Set(0,5,0);const auto material=combiner.Prepare();
    unsigned cases=0,mismatches=0,model_mismatches=0,flat_cases=0,flat_mismatches=0,max_color_delta=0,shadow_cases=0,shadow_mismatches=0,shadow_max_delta=0;
    auto check=[&](m::MeshStencilState stencil,bool cw,unsigned initial,bool depth_pass,bool depth_enabled=true){
        auto vertices=Triangle(cw);const auto raw=Raw(vertices);m::MeshRasterState raster;raster.depth_write=false;raster.depth_compare=depth_pass?m::RasterCompare::Always:m::RasterCompare::Never;
        raster.depth_test=depth_enabled;MaterialMeshOptions options;options.stencil=stencil;options.clear_stencil=initial;
        const auto sdk_color=hal.Draw(0x44,raw,20,mat::Identity(),fog,raster,options);const auto gpu_color=gl.DrawMesh(uniforms,material,fog,vertices,raster,16,16,options);const auto gpu_stencil=gl.ReadStencil();const auto sdk_stencil=hal.ReadStencil();
        bool differs=false;for(unsigned y=2;y<7;++y)for(unsigned x=2;x<7;++x){const auto i=y*16+x;differs|=sdk_stencil[i]!=gpu_stencil[i] || sdk_color[i]!=gpu_color[i];}
        if(differs){if(mismatches<10)std::printf("stencil mismatch case=%u cw=%u two=%u ref=%08x read=%08x write=%08x init=%u HAL=%u GL=%u color=%08x/%08x\n",cases,cw,stencil.two_sided,stencil.reference,stencil.read_mask,stencil.write_mask,initial,sdk_stencil[68],gpu_stencil[68],sdk_color[68],gpu_color[68]);++mismatches;}
        depth_pass|=!depth_enabled;const auto model=m::EvaluateStencil(stencil,cw,depth_pass,std::uint8_t(initial));const bool actual_pass=sdk_color[68]==0xff31a7d3u;
        if(model.value!=sdk_stencil[68] || model.color_pass!=actual_pass)++model_mismatches;
        const auto a=stencil.clockwise,b=stencil.counterclockwise;
        const StencilGolden g{unsigned(stencil.enabled),unsigned(stencil.two_sided),unsigned(cw),unsigned(depth_pass),initial,stencil.reference,stencil.read_mask,stencil.write_mask,{a.fail,a.depth_fail,a.pass,a.compare},{b.fail,b.depth_fail,b.pass,b.compare},sdk_stencil[68],unsigned(actual_pass)};golden.write(reinterpret_cast<const char*>(&g),sizeof(g));++cases;
    };
    for(unsigned op=1;op<=8;++op)for(unsigned branch=0;branch<3;++branch)for(bool two:{false,true})for(bool cw:{false,true})for(unsigned initial:{0u,1u,127u,254u,255u})for(unsigned mask:{0u,15u,255u}){
        m::MeshStencilState s;s.enabled=true;s.two_sided=two;s.clockwise={op,op,op,branch?8u:1u};const auto other=op%8+1;s.counterclockwise={other,other,other,branch?8u:1u};s.reference=std::array<unsigned,6>{0,1,128,255,256,511}[cases%6];s.write_mask=mask;check(s,cw,initial,branch!=1);
    }
    for(unsigned compare=1;compare<=8;++compare)for(bool two:{false,true})for(bool cw:{false,true})for(unsigned initial:{0u,127u,255u})for(unsigned reference:{0u,128u,256u})for(unsigned mask:{0u,15u,255u}){
        m::MeshStencilState s;s.enabled=true;s.two_sided=two;s.clockwise={3,6,7,compare};s.counterclockwise={2,8,5,9-compare};s.reference=reference;s.read_mask=mask;s.write_mask=0x155;check(s,cw,initial,true);
    }
    for(bool cw:{false,true})for(bool pass:{false,true}){m::MeshStencilState off;off.two_sided=true;off.clockwise={2,2,2,1};check(off,cw,137,pass);}
    for(bool cw:{false,true}){m::MeshStencilState s;s.enabled=true;s.two_sided=true;s.reference=137;s.clockwise=s.counterclockwise={1,2,3,8};check(s,cw,0,false,false);}
    // Distinct vertex RGB/alpha and variable RHW, culling and strips/fans.
    for(unsigned shade:{1u,2u})for(unsigned primitive:{4u,5u,6u})for(bool variable_w:{false,true})for(bool reverse:{false,true})for(unsigned cull:{1u,2u,3u}){
        std::vector<mat::Vertex> v(4);v[0].position={-4,-4,.25f,1};v[1].position={28,-4,.25f,1};v[2].position={-4,28,.25f,1};v[3].position={28,28,.25f,1};
        for(unsigned i=0;i<4;++i){v[i].diffuse=Unpack(std::array<unsigned,4>{0xffe03123,0x8031c541,0x2f273fd1,0xcb8129b7}[i]);if(variable_w)v[i].position[3]=1.f/(1+.6f*i);}
        if(primitive==4)v={v[0],v[1],v[2],v[2],v[1],v[3]};else if(primitive==6)v={v[0],v[1],v[3],v[2]};
        if(reverse){if(primitive==4){std::swap(v[1],v[2]);std::swap(v[4],v[5]);}else std::reverse(v.begin(),v.end());}
        const auto raw=Raw(v);m::MeshRasterState raster;raster.depth_test=false;MaterialMeshOptions options;options.shade=shade;options.cull=cull;options.primitive=primitive;
        const auto sdk=hal.Draw(0x44,raw,20,mat::Identity(),fog,raster,options),gpu=gl.DrawMesh(uniforms,material,fog,v,raster,16,16,options);
        unsigned delta=0;for(unsigned y=2;y<14;++y)for(unsigned x=2;x<14;++x)for(unsigned c=0;c<4;++c)delta=std::max(delta,unsigned(std::abs(int((sdk[y*16+x]>>(c*8))&255)-int((gpu[y*16+x]>>(c*8))&255))));
        if(delta>2){if(flat_mismatches<10)std::printf("flat mismatch shade=%u primitive=%u variableW=%u reverse=%u cull=%u delta=%u HAL=%08x GL=%08x\n",shade,primitive,variable_w,reverse,cull,delta,sdk[136],gpu[136]);++flat_mismatches;}max_color_delta=std::max(max_color_delta,delta);++flat_cases;
    }
    // Source-derived DrawShadows sequence with controlled fixture geometry and
    // fragment alpha. This is not execution of a complete map/DrawShadows body.
    for(bool two:{false,true})for(unsigned alpha:{0u,7u,8u,255u}){
        std::vector<mat::Vertex> volume=Triangle(true);volume[0].position={0,1,.25f,1};volume[1].position={15,1,.25f,1};volume[2].position={0,15,.25f,1};
        auto back=volume;for(auto& v:back)v.position[0]+=3;std::swap(back[1],back[2]);volume.insert(volume.end(),back.begin(),back.end());
        m::MeshRasterState r;r.depth_write=false;r.blend_enable=true;r.source=m::RasterBlendFactor::Zero;r.destination=m::RasterBlendFactor::One;
        MaterialMeshOptions o;o.shade=1;o.cull=two?1:3;o.stencil.enabled=true;o.stencil.two_sided=two;o.stencil.clockwise={1,1,7,8};o.stencil.counterclockwise={1,1,8,8};
        auto pass=[&](const std::vector<mat::Vertex>& v,unsigned primitive,const mat::Uniforms& u){o.primitive=primitive;const auto raw=Raw(v);auto sdk=hal.Draw(0x44,raw,20,mat::Identity(),fog,r,o,&u);auto gpu=gl.DrawMesh(uniforms,u,fog,v,r,16,16,o);o.preserve=true;return std::pair{sdk,gpu};};
        pass(volume,4,material);
        if(!two){o.cull=2;o.stencil.clockwise.pass=8;pass(volume,4,material);}
        // Default projected-stencil block: REPLACE with alpha test and no color writes.
        o.cull=3;o.stencil.two_sided=false;o.stencil.reference=1;o.stencil.clockwise={1,1,3,8};r.color_write=0;r.alpha_test=true;r.alpha_reference=8;r.alpha_compare=m::RasterCompare::GreaterEqual;
        std::vector<mat::Vertex> quad(4);quad[0].position={3,3,.25f,1};quad[1].position={12,3,.25f,1};quad[2].position={3,12,.25f,1};quad[3].position={12,12,.25f,1};for(auto& v:quad)v.diffuse=Unpack((alpha<<24)|0x00ffffffu);pass(quad,5,material);
        // Original final quad: ref1 <= stencil, KEEP, SRCALPHA, MODULATE factor C0.
        r.depth_test=false;r.color_write=15;r.source=m::RasterBlendFactor::SourceAlpha;r.destination=m::RasterBlendFactor::InverseSourceAlpha;o.shade=2;o.stencil.clockwise={1,1,1,4};
        quad[0].position={-.5f,-.5f,0,1};quad[1].position={15.5f,-.5f,0,1};quad[2].position={-.5f,15.5f,0,1};quad[3].position={15.5f,15.5f,0,1};for(auto& v:quad)v.diffuse={1,1,1,1};
        auto combine=combiner;combine.Factor(0xc0c0c0c0);combine.Set(0,1,4);combine.Set(0,2,0);combine.Set(0,3,3);combine.Set(0,4,4);combine.Set(0,5,0);combine.Set(0,6,3);
        const auto colors=pass(quad,5,combine.Prepare());const auto a=hal.ReadStencil(),b=gl.ReadStencil();bool bad=a!=b;
        for(unsigned i=0;i<256;++i)for(unsigned c=0;c<4;++c){const auto delta=unsigned(std::abs(int((colors.first[i]>>(c*8))&255)-int((colors.second[i]>>(c*8))&255)));shadow_max_delta=std::max(shadow_max_delta,delta);bad|=delta>1;}
        const unsigned meta[]{unsigned(two),alpha};scenes.write(reinterpret_cast<const char*>(meta),sizeof(meta));scenes.write(reinterpret_cast<const char*>(colors.first.data()),1024);scenes.write(reinterpret_cast<const char*>(colors.second.data()),1024);scenes.write(reinterpret_cast<const char*>(a.data()),256);scenes.write(reinterpret_cast<const char*>(b.data()),256);
        if(bad){++shadow_mismatches;unsigned ds=0,dc=0;for(unsigned i=0;i<256;++i){ds+=a[i]!=b[i];if(colors.first[i]!=colors.second[i]){if(dc<2)std::printf("shadow pixel=%u color HAL=%08x GL=%08x stencil=%u/%u\n",i,colors.first[i],colors.second[i],a[i],b[i]);++dc;}}std::printf("shadow sequence mismatch two=%u alpha=%u stencil_pixels=%u color_pixels=%u\n",two,alpha,ds,dc);}++shadow_cases;
    }
    std::printf("actual stencil HAL/GL cases=%u mismatches=%u CPU_model_mismatches=%u; flat/Gouraud/cull/list/strip/fan cases=%u mismatches=%u max_color_delta=%u; shadow_sequences=%u mismatches=%u max_color_delta=%u; SWITCH_HARDWARE_NOT_RUN\n",cases,mismatches,model_mismatches,flat_cases,flat_mismatches,max_color_delta,shadow_cases,shadow_mismatches,shadow_max_delta);
    return mismatches || model_mismatches || flat_mismatches || shadow_mismatches?1:0;
}
