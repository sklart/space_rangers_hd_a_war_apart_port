// Actual pinned BigVB/BigIB bodies. GPU reference uses a real Windows D3D9
// device; native tests consume owned staging. Neither executes a battle.
#include "BigVB.hpp"
#include "BigIB.hpp"
#ifndef MATRIXGAME_ORIGINAL_BIG_BUFFER_REFERENCE
#include "buffer_geometry.hpp"
#include "vector_geometry.hpp"
#include "packed_vector.hpp"
#include "config_unicode.hpp"
#endif
#include <array>
#include <bit>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <span>
#include <stdexcept>
#include <vector>

struct Vertex {
    float position[3],normal[3],uv[2];
    static constexpr std::uint32_t FVF=0x112;
};
static_assert(sizeof(Vertex)==32 && offsetof(Vertex,normal)==12 && offsetof(Vertex,uv)==24);
template class CBigVB<Vertex>;
template struct SBigVBSource<Vertex>;
struct Range {std::int32_t base;std::uint32_t first,count,minimum,vertices;std::int32_t surface{};};
static std::vector<std::byte> transcript;
static void write_u32(std::uint32_t value) {
    for(int i=0;i<4;++i) transcript.push_back(std::byte((value>>(i*8))&255));
}
static void blob(const void* data,std::size_t count) {
    const auto* bytes=static_cast<const std::byte*>(data);transcript.insert(transcript.end(),bytes,bytes+count);
}
template<class Error,class F> static void rejects(F call) {
    bool rejected=false;try {call();} catch(const Error&) {rejected=true;}assert(rejected);
}
struct Snapshot {std::vector<Vertex> vertices;std::vector<std::uint16_t> indices;};
#ifdef MATRIXGAME_ORIGINAL_BIG_BUFFER_REFERENCE
IDirect3DDevice9* g_D3DD=nullptr;
struct Device {
    HWND window{};IDirect3D9* api{};
    Device() {
        window=CreateWindowExW(0,L"STATIC",L"MatrixGame original buffer reference",WS_POPUP,0,0,16,16,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        if(!window) throw std::runtime_error("reference hidden window");
        api=Direct3DCreate9(D3D_SDK_VERSION);if(!api) throw std::runtime_error("reference D3D9 unavailable");
        D3DPRESENT_PARAMETERS params{};params.Windowed=TRUE;params.SwapEffect=D3DSWAPEFFECT_DISCARD;
        params.hDeviceWindow=window;params.BackBufferWidth=params.BackBufferHeight=16;
        params.BackBufferFormat=D3DFMT_UNKNOWN;params.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
        auto result=api->CreateDevice(0,D3DDEVTYPE_NULLREF,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&params,&g_D3DD);
        if(FAILED(result)) result=api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&params,&g_D3DD);
        if(FAILED(result)) throw std::runtime_error("reference D3D9 device unavailable");
    }
    void Clear() {ASSERT_DX(g_D3DD->SetStreamSource(0,nullptr,0,0));ASSERT_DX(g_D3DD->SetIndices(nullptr));}
    ~Device(){if(g_D3DD)g_D3DD->Release();if(api)api->Release();if(window)DestroyWindow(window);}
};
static Snapshot capture(std::span<const Range>) {
    Snapshot result;IDirect3DVertexBuffer9* vb=nullptr;IDirect3DIndexBuffer9* ib=nullptr;
    UINT offset=0,stride=0;ASSERT_DX(g_D3DD->GetStreamSource(0,&vb,&offset,&stride));assert(vb&&stride==32);
    ASSERT_DX(g_D3DD->GetIndices(&ib));assert(ib);D3DVERTEXBUFFER_DESC vd{};D3DINDEXBUFFER_DESC id{};
    ASSERT_DX(vb->GetDesc(&vd));ASSERT_DX(ib->GetDesc(&id));assert(!offset&&vd.Size%32==0&&id.Size%2==0);
    result.vertices.resize(vd.Size/32);result.indices.resize(id.Size/2);void* bytes=nullptr;
    ASSERT_DX(vb->Lock(0,0,&bytes,0));std::memcpy(result.vertices.data(),bytes,vd.Size);ASSERT_DX(vb->Unlock());
    ASSERT_DX(ib->Lock(0,0,&bytes,0));std::memcpy(result.indices.data(),bytes,id.Size);ASSERT_DX(ib->Unlock());
    vb->Release();ib->Release();return result;
}
#else
namespace buffers=srhd_awa::matrixgame::buffers;
static Snapshot capture(std::span<const Range> ranges) {
    std::vector<srhd_awa::matrixgame::MeshRange> draws;
    for(auto range:ranges) draws.push_back({range.surface,range.base,range.first,range.count,range.minimum,range.vertices});
    const auto geometry=buffers::CaptureBoundVO(buffers::Current(),draws);
    Snapshot result;result.vertices.resize(geometry.vertices.size());
    std::memcpy(result.vertices.data(),geometry.vertices.data(),geometry.vertices.size()*sizeof(Vertex));
    result.indices=geometry.indices;return result;
}
static void resource_contract() {
    rejects<std::logic_error>([]{(void)buffers::Current();});
    buffers::Context context(512),other;buffers::Scope scope(context);
    rejects<std::logic_error>([&]{buffers::Scope invalid(other);});
    rejects<std::invalid_argument>([&]{context.CreateVertex(-1,Vertex::FVF);});
    rejects<std::invalid_argument>([&]{context.CreateIndex(3,buffers::IndexWidth::U16);});
    auto* vertex=context.CreateVertex(128,Vertex::FVF,buffers::Pool::Default,true);assert(vertex);
    assert(!vertex->Initialized(0,1));
    rejects<std::logic_error>([&]{vertex->Unmap();});
    rejects<std::out_of_range>([&]{vertex->Map(129,1);});
    auto* bytes=static_cast<std::byte*>(vertex->Map(32,32));std::memset(bytes,7,32);
    rejects<std::logic_error>([&]{vertex->Map(0,1);});
    rejects<std::logic_error>([&]{(void)vertex->Storage();});vertex->Unmap();
    assert(vertex->Version()==1&&vertex->Initialized(32,32)&&!vertex->Initialized(0,1));
    bytes=static_cast<std::byte*>(vertex->Map(0,32,buffers::MapMode::NoOverwrite));std::memset(bytes,3,32);vertex->Unmap();
    assert(vertex->Initialized(0,64));
    bytes=static_cast<std::byte*>(vertex->Map(64,32,buffers::MapMode::Discard));std::memset(bytes,9,32);vertex->Unmap();
    assert(!vertex->Initialized(0,1)&&vertex->Initialized(64,32));
    rejects<std::invalid_argument>([&]{other.BindVertex(vertex,0,32);});
    context.BindVertex(vertex,0,32);vertex->Release();assert(context.AllocatedBytes()==128);
    context.ClearBindings();assert(context.AllocatedBytes()==0);
    auto* fixed=context.CreateVertex(64,Vertex::FVF);assert(fixed);
    rejects<std::invalid_argument>([&]{fixed->Map(0,0,buffers::MapMode::Discard);});fixed->Release();
    assert(!context.CreateVertex(513,Vertex::FVF));assert(context.AllocatedBytes()==0);
    auto* indices=context.CreateIndex(12,buffers::IndexWidth::U32);assert(indices);
    context.BindIndex(indices);indices->Release();
    fixed=context.CreateVertex(96,Vertex::FVF);assert(fixed);context.BindVertex(fixed,0,32);
    rejects<std::invalid_argument>([&]{(void)buffers::ValidateBoundVO(context,{});});context.ClearBindings();fixed->Release();
    assert(context.AllocatedBytes()==0);
    fixed=context.CreateVertex(96,Vertex::FVF);indices=context.CreateIndex(6,buffers::IndexWidth::U16);
    assert(fixed&&indices);context.BindVertex(fixed,0,32);context.BindIndex(indices);
    const std::array<std::uint16_t,3> triangle{0,1,2};
    std::memcpy(indices->Map(0,0),triangle.data(),6);indices->Unmap();
    const srhd_awa::matrixgame::MeshRange draw{0,0,0,3,0,3};
    rejects<std::logic_error>([&]{buffers::ValidateBoundVO(context,std::span(&draw,1));});
    std::memset(fixed->Map(0,64),0,64);fixed->Unmap();
    rejects<std::logic_error>([&]{buffers::ValidateBoundVO(context,std::span(&draw,1));});
    std::memset(fixed->Map(64,32),0,32);fixed->Unmap();
    const auto key=buffers::ValidateBoundVO(context,std::span(&draw,1));
    std::memset(fixed->Map(0,32),0,32);
    rejects<std::logic_error>([&]{buffers::ValidateBoundVO(context,std::span(&draw,1));});fixed->Unmap();
    const auto changed=buffers::ValidateBoundVO(context,std::span(&draw,1));
    assert(key.vertex_id==changed.vertex_id&&key.vertex_version!=changed.vertex_version);
    fixed->Release();indices->Release();context.ClearBindings();assert(context.AllocatedBytes()==0);
}
#endif
static void record(std::span<const Range> ranges,const std::vector<Vertex>& expected_vertices,
                   const std::vector<std::uint16_t>& expected_indices) {
    const auto actual=capture(ranges);assert(actual.vertices.size()>=expected_vertices.size()&&actual.indices.size()>=expected_indices.size());
    // Only written live bytes are an oracle. Unwritten padding in real D3D9
    // allocations is undefined and is deliberately excluded from comparison.
    assert(!std::memcmp(actual.vertices.data(),expected_vertices.data(),expected_vertices.size()*32));
    assert(!std::memcmp(actual.indices.data(),expected_indices.data(),expected_indices.size()*2));
    write_u32(static_cast<std::uint32_t>(actual.vertices.size()));write_u32(static_cast<std::uint32_t>(actual.indices.size()));
    write_u32(static_cast<std::uint32_t>(ranges.size()));
    for(auto range:ranges) {write_u32(std::uint32_t(range.base));write_u32(range.first);write_u32(range.count);write_u32(range.minimum);write_u32(range.vertices);write_u32(std::uint32_t(range.surface));}
    write_u32(static_cast<std::uint32_t>(expected_vertices.size()));blob(expected_vertices.data(),expected_vertices.size()*32);
    write_u32(static_cast<std::uint32_t>(expected_indices.size()));blob(expected_indices.data(),expected_indices.size()*2);
}
static void packing_contract() {
    for(int cycle=0;cycle<10;++cycle) {
        std::vector<Vertex> a(3),b(3);
        for(int i=0;i<3;++i) {a[i]={{float(i),0,.5f},{0,0,1},{0,0}};b[i]={{float(i),1,.5f},{0,0,1},{1,0}};}
        std::array<std::uint16_t,3> ai{0,1,2},bi{0,1,2};
        SBigVBSource<Vertex> va,vb;va.verts=a.data();va.size=96;vb.verts=b.data();vb.size=96;
        SBigIBSource ia,ib;ia.inds=ai.data();ia.size=6;ib.inds=bi.data();ib.size=6;
        auto* vertices=CBigVB<Vertex>::NewBigVB(nullptr);auto* indices=CBigIB::NewBigIB(nullptr);
        vertices->AddSource(&va);indices->AddSource(&ia);vertices->BeforeDraw();indices->BeforeDraw();
        assert(va.IsDirty(vertices)&&ia.IsDirty(indices));assert(va.Select(vertices)==0&&ia.Select(indices)==0);
        assert(!va.IsDirty(vertices)&&!ia.IsMarkedNoNeed(indices));
        const Range one{0,0,3,0,3};record(std::span(&one,1),a,{0,1,2});
        vertices->AddSource(&vb);indices->AddSource(&ib);vertices->BeforeDraw();indices->BeforeDraw();
        assert(va.IsDirty(vertices)&&ia.IsDirty(indices));
        assert(va.Select(vertices)==0&&ia.Select(indices)==0&&vb.Select(vertices)==3&&ib.Select(indices)==3);
        const std::array ranges{Range{0,0,3,0,3},Range{3,3,3,0,3}};
        auto expected=a;expected.insert(expected.end(),b.begin(),b.end());record(ranges,expected,{0,1,2,0,1,2});
        a[0].position[0]+=.125f;va.Invalidate(vertices);vertices->BeforeDraw();assert(va.Select(vertices)==0);
        expected=a;expected.insert(expected.end(),b.begin(),b.end());record(ranges,expected,{0,1,2,0,1,2});
        va.MarkNoNeed(vertices);vb.MarkNoNeed(vertices);ia.MarkNoNeed(indices);ib.MarkNoNeed(indices);
        // Last source owner was released; the bound data remains readable.
        record(ranges,expected,{0,1,2,0,1,2});
        vertices->BeforeDraw();indices->BeforeDraw();assert(va.Select(vertices)==0&&vb.Select(vertices)==3&&ia.Select(indices)==0&&ib.Select(indices)==3);
        record(ranges,expected,{0,1,2,0,1,2});
        assert(!vertices->DelSource(&va)&&!indices->DelSource(&ia));vertices->BeforeDraw();indices->BeforeDraw();
        assert(vb.Select(vertices)==0&&ib.Select(indices)==0);record(std::span(&one,1),b,{0,1,2});
        vertices->ReleaseBuffers();indices->ReleaseBuffers();vertices->BeforeDraw();indices->BeforeDraw();
        assert(vb.Select(vertices)==0&&ib.Select(indices)==0);record(std::span(&one,1),b,{0,1,2});
        assert(vertices->DelSource(&vb)&&indices->DelSource(&ib));
        CBigVB<Vertex>::DestroyBigVB(vertices);CBigIB::DestroyBigIB(indices);
#ifdef MATRIXGAME_ORIGINAL_BIG_BUFFER_REFERENCE
        ASSERT_DX(g_D3DD->SetStreamSource(0,nullptr,0,0));ASSERT_DX(g_D3DD->SetIndices(nullptr));
#else
        buffers::Current().ClearBindings();assert(buffers::Current().AllocatedBytes()==0);
#endif
    }
}
static void large_base_contract() {
    std::vector<Vertex> prefix(70000),triangle(3);
    for(int i=0;i<3;++i) triangle[i]={{float(i),2,.5f},{0,0,1},{0,0}};
    std::array<std::uint16_t,3> data{0,1,2};
    SBigVBSource<Vertex> first,last;first.size=int(prefix.size()*32);first.verts=prefix.data();last.size=96;last.verts=triangle.data();
    SBigIBSource source;source.size=6;source.inds=data.data();
    auto* vertices=CBigVB<Vertex>::NewBigVB(nullptr);auto* indices=CBigIB::NewBigIB(nullptr);
    vertices->AddSource(&first);vertices->AddSource(&last);indices->AddSource(&source);
    vertices->BeforeDraw();indices->BeforeDraw();assert(last.Select(vertices)==70000&&source.Select(indices)==0);
    const Range range{70000,0,3,0,3};const auto actual=capture(std::span(&range,1));
    assert(actual.vertices.size()==70003&&!std::memcmp(actual.vertices.data()+70000,triangle.data(),96));
    assert(actual.indices.size()==6&&!std::memcmp(actual.indices.data(),data.data(),6));
    write_u32(0x53505253);write_u32(70000);write_u32(static_cast<std::uint32_t>(actual.vertices.size()));write_u32(static_cast<std::uint32_t>(actual.indices.size()));
    blob(triangle.data(),96);blob(data.data(),6);
    assert(!vertices->DelSource(&first));assert(vertices->DelSource(&last)&&indices->DelSource(&source));
    CBigVB<Vertex>::DestroyBigVB(vertices);CBigIB::DestroyBigIB(indices);
#ifdef MATRIXGAME_ORIGINAL_BIG_BUFFER_REFERENCE
    ASSERT_DX(g_D3DD->SetStreamSource(0,nullptr,0,0));ASSERT_DX(g_D3DD->SetIndices(nullptr));
#else
    buffers::Current().ClearBindings();assert(buffers::Current().AllocatedBytes()==0);
#endif
}
static void partial_contract() {
    std::vector<Vertex> data(3);
    for(int i=0;i<3;++i) data[i]={{float(i),3,.5f},{0,0,1},{0,0}};
    std::array<std::uint16_t,3> triangles{0,1,1};
    SBigVBSource<Vertex> source;source.verts=data.data();source.size=96;
    SBigIBSource is;is.inds=triangles.data();is.size=6;
    auto* vertices=CBigVB<Vertex>::NewBigVB(nullptr);auto* indices=CBigIB::NewBigIB(nullptr);
    vertices->AddSource(&source);indices->AddSource(&is);vertices->BeforeDraw();indices->BeforeDraw();
    assert(!source.Prepare(vertices,64));assert(source.Select(vertices)==0&&is.Select(indices)==0);
    const Range partial{0,0,3,0,2};record(std::span(&partial,1),std::vector<Vertex>(data.begin(),data.begin()+2),{0,1,1});
    source.Invalidate(vertices);vertices->BeforeDraw();assert(source.Select(vertices)==0);
    const Range full{0,0,3,0,3};record(std::span(&full,1),data,{0,1,1});
    assert(vertices->DelSource(&source)&&indices->DelSource(&is));CBigVB<Vertex>::DestroyBigVB(vertices);CBigIB::DestroyBigIB(indices);
#ifdef MATRIXGAME_ORIGINAL_BIG_BUFFER_REFERENCE
    ASSERT_DX(g_D3DD->SetStreamSource(0,nullptr,0,0));ASSERT_DX(g_D3DD->SetIndices(nullptr));
#else
    buffers::Current().ClearBindings();assert(buffers::Current().AllocatedBytes()==0);
#endif
}
struct MeshInput {std::vector<Vertex> vertices;std::vector<std::uint16_t> indices;std::vector<Range> ranges;std::uint32_t frames{};};
static MeshInput read_mesh(const char* path) {
    std::ifstream in(path,std::ios::binary);assert(in.good());char magic[4];in.read(magic,4);assert(!std::memcmp(magic,"BGM1",4));
    auto read_u32=[&] {std::uint8_t data[4]{};in.read(reinterpret_cast<char*>(data),4);assert(in.good());return std::uint32_t(data[0])|(std::uint32_t(data[1])<<8)|(std::uint32_t(data[2])<<16)|(std::uint32_t(data[3])<<24);};
    MeshInput result;const auto vb=read_u32();assert(vb&&vb<=BVB_MAX_SIZE&&vb%32==0);result.vertices.resize(vb/32);
    in.read(reinterpret_cast<char*>(result.vertices.data()),vb);assert(in.good());
    const auto ib=read_u32();assert(ib&&ib<=BIB_MAX_SIZE&&ib%2==0);result.indices.resize(ib/2);
    in.read(reinterpret_cast<char*>(result.indices.data()),ib);assert(in.good());result.frames=read_u32();assert(result.frames&&result.frames<=100000);
    for(std::uint32_t frame=0;frame<result.frames;++frame) {
        const auto count=read_u32();assert(count<=100000&&result.ranges.size()+count<=4000000);
        for(std::uint32_t i=0;i<count;++i) {
            const auto surface=std::bit_cast<std::int32_t>(read_u32()),base=std::bit_cast<std::int32_t>(read_u32());
            const auto first=read_u32(),n=read_u32(),minimum=read_u32(),vertices=read_u32();
            result.ranges.push_back({base,first,n,minimum,vertices,surface});
        }
    }
    assert(in.peek()==std::ifstream::traits_type::eof());return result;
}
static void mesh_contract(MeshInput& mesh) {
    SBigVBSource<Vertex> vs;vs.verts=mesh.vertices.data();vs.size=int(mesh.vertices.size()*32);
    SBigIBSource is;is.inds=mesh.indices.data();is.size=int(mesh.indices.size()*2);
    auto* vertices=CBigVB<Vertex>::NewBigVB(nullptr);auto* indices=CBigIB::NewBigIB(nullptr);
    vertices->AddSource(&vs);indices->AddSource(&is);vertices->BeforeDraw();indices->BeforeDraw();assert(vs.Select(vertices)==0&&is.Select(indices)==0);
    write_u32(mesh.frames);record(mesh.ranges,mesh.vertices,mesh.indices);
    vertices->ReleaseBuffers();indices->ReleaseBuffers();vertices->BeforeDraw();indices->BeforeDraw();assert(vs.Select(vertices)==0&&is.Select(indices)==0);
    record(mesh.ranges,mesh.vertices,mesh.indices);
    assert(vertices->DelSource(&vs)&&indices->DelSource(&is));CBigVB<Vertex>::DestroyBigVB(vertices);CBigIB::DestroyBigIB(indices);
#ifdef MATRIXGAME_ORIGINAL_BIG_BUFFER_REFERENCE
    ASSERT_DX(g_D3DD->SetStreamSource(0,nullptr,0,0));ASSERT_DX(g_D3DD->SetIndices(nullptr));
#else
    buffers::Current().ClearBindings();assert(buffers::Current().AllocatedBytes()==0);
#endif
}
int main(int argc,char** argv) {
    const bool mesh_mode=argc==4&&std::strcmp(argv[1],"--mesh")==0;
    const bool vo_mode=argc==4&&std::strcmp(argv[1],"--vo")==0;
    assert(argc<=2||mesh_mode||vo_mode);
#ifdef MATRIXGAME_ORIGINAL_BIG_BUFFER_REFERENCE
    Device device;
#else
    resource_contract();buffers::Context context;buffers::Scope scope(context);
#endif
    write_u32(0x31425642);
    if(mesh_mode) {auto mesh=read_mesh(argv[2]);mesh_contract(mesh);}
    else if(vo_mode) {
#ifdef MATRIXGAME_ORIGINAL_BIG_BUFFER_REFERENCE
        throw std::invalid_argument("Original D3D9 reference uses independent --mesh oracle input");
#else
        const auto model=srhd_awa::matrixgame::VectorGeometry::Load(::matrixgame::native::ConfigWide(argv[2]).c_str());
        MeshInput mesh;mesh.vertices.resize(model.Vertices().size());std::memcpy(mesh.vertices.data(),model.Vertices().data(),model.Vertices().size_bytes());
        mesh.indices.assign(model.Indices().begin(),model.Indices().end());mesh.frames=std::uint32_t(model.Frames().size());
        for(std::uint32_t frame=0;frame<mesh.frames;++frame) for(auto r:model.FrameRanges(frame)) mesh.ranges.push_back({r.base_vertex,r.first_index,r.index_count,r.minimum_vertex,r.vertex_count,r.surface});
        mesh_contract(mesh);
        // Actual resource owner used by the SDK adapter, including active-source
        // destructor cleanup and a second bind after buffer release/recreation.
        {srhd_awa::matrixgame::PackedVectorBuffers packed(model);packed.Select(context);const auto current=capture(mesh.ranges);
         assert(!std::memcmp(current.vertices.data(),mesh.vertices.data(),mesh.vertices.size()*32));
         packed.ReleaseBuffers();packed.Select(context);(void)capture(mesh.ranges);}
        context.ClearBindings();assert(context.AllocatedBytes()==0);
#endif
    } else {packing_contract();large_base_contract();partial_contract();}
    const char* output=argc==2?argv[1]:(argc==4?argv[3]:nullptr);
    if(output) {std::ofstream out(output,std::ios::binary);out.write(reinterpret_cast<const char*>(transcript.data()),transcript.size());out.close();assert(out.good());}
    std::printf("PASS original BigVB/BigIB packing mode=%s transcript_bytes=%zu; gpu_draw_executed=false battle_executed=false\n",argc==4?"model":"controlled",transcript.size());
}
