// SPDX-License-Identifier: GPL-2.0-or-later
#include "vector_geometry.hpp"
#include "CStorage.hpp"
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace srhd_awa::matrixgame {
namespace {
static_assert(std::endian::native==std::endian::little);
static_assert(sizeof(MeshVertex)==sizeof(vo::SVOVertex) && sizeof(MeshVertex)==32);
static_assert(offsetof(vo::SVOVertex,v)==offsetof(MeshVertex,position) &&
    offsetof(vo::SVOVertex,n)==offsetof(MeshVertex,normal) &&
    offsetof(vo::SVOVertex,tu)==offsetof(MeshVertex,uv));
static_assert(sizeof(vo::SVOUnion)==24 && sizeof(vo::SVOKadr)==64);
static_assert(sizeof(vo::SVOKadrEdge)==16 && sizeof(vo::SVOFrameIndex)==8);
static_assert(sizeof(VectorSimpleGeometry::bounds)==48);
[[noreturn]] void Bad(const char* why) { throw std::invalid_argument(why); }
void Slice(std::int64_t first,std::int64_t count,std::size_t available) {
  if(first<0 || count<0 || std::uint64_t(first)>available ||
      std::uint64_t(count)>available-std::uint64_t(first)) Bad("VO payload range");
}
struct Reader {
  CStorage& storage;
  std::size_t bytes{};
  void Budget(std::size_t count,std::size_t stride) {
    constexpr std::size_t limit=128*1024*1024;
    if(stride && count>(limit-bytes)/stride) throw std::length_error("VO owned data budget");
    bytes+=count*stride;
  }
  CDataBuf* Column(const wchar_t* table,const wchar_t* name,EStorageType type,bool required=true) {
    auto* p=storage.GetBuf(table,name,type);
    if(!p && required) Bad("missing VO column");
    return p;
  }
  template<class T> std::vector<T> Array(const wchar_t* table,const wchar_t* name,EStorageType type=ST_BYTE,bool required=true) {
    static_assert(std::is_trivially_copyable_v<T>);
    auto* p=Column(table,name,type,required);
    if(!p) return {};
    if(p->GetArraysCount()!=1) Bad("VO single payload array required");
    const auto bytes=std::size_t(p->GetArrayLength(0))*StorageTypeSize(type);
    if(bytes%sizeof(T)) Bad("VO payload stride");
    const auto count=bytes/sizeof(T);Budget(count,sizeof(T));
    std::vector<T> out(count);
    if(bytes) std::memcpy(out.data(),p->GetFirst<std::uint8_t>(0),bytes);
    return out;
  }
  std::vector<std::u16string> Names(const wchar_t* table,const wchar_t* name,bool required=false) {
    auto* p=Column(table,name,ST_WCHAR,required);
    if(!p) return {};
    const auto count=p->GetArraysCount();Budget(count,sizeof(std::u16string));
    std::vector<std::u16string> out;out.reserve(count);
    for(std::uint32_t i=0;i<count;++i) {
      const auto units=p->GetArrayLength(i);
      // Length-delimited UTF-16LE code units, with no wchar_t conversion or
      // terminator. Budget before allocating/copying; preserve lone surrogates.
      Budget(std::size_t(units)+1,sizeof(char16_t));
      std::u16string text(units,u'\0');
      if(units) std::memcpy(text.data(),p->GetFirst<std::uint8_t>(i),std::size_t(units)*2);
      if(text.find(u'\0')!=text.npos) Bad("VO embedded NUL/name extent");
      out.push_back(std::move(text));
    }
    return out;
  }
};
void Finite(const D3DXVECTOR3& p) {
  if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)) Bad("nonfinite VO position");
}
}
VectorGeometry VectorGeometry::Load(const wchar_t* path) {
  if(!path) Bad("null VO path");
  CStorage source;
  if(!source.Load(path)) Bad("cannot load VO storage");
  return Load(source);
}
VectorGeometry VectorGeometry::Load(CStorage& source) {
  Reader r{source};VectorGeometry out;
  out.vertices_=r.Array<MeshVertex>(L"verts",L"data");
  out.indices_=r.Array<std::uint16_t>(L"idxs",L"data");
  out.frames_=r.Array<vo::SVOKadr>(L"frames",L"data");
  out.frame_vertices_=r.Array<std::uint32_t>(L"frames",L"verts");
  out.frame_groups_=r.Array<std::uint32_t>(L"frames",L"unions");
  out.edges_=r.Array<vo::SVOKadrEdge>(L"frames",L"edges");
  out.groups_=r.Array<vo::SVOUnion>(L"unions",L"data");
  out.surfaces_=r.Names(L"surfs",L"texs",true);
  if(out.vertices_.empty()||out.frames_.empty()||out.indices_.size()%3) Bad("empty/invalid VO geometry");
  for(const auto& vertex:out.vertices_) {
    for(auto v:vertex.position) if(!std::isfinite(v)) Bad("nonfinite VO vertex");
    for(auto v:vertex.normal) if(!std::isfinite(v)) Bad("nonfinite VO normal");
    for(auto v:vertex.uv) if(!std::isfinite(v)) Bad("nonfinite VO UV");
  }
  for(auto index:out.frame_vertices_) if(index>=out.vertices_.size()) Bad("VO frame vertex ID");
  for(auto index:out.frame_groups_) if(index>=out.groups_.size()) Bad("VO frame group ID");
  for(const auto& edge:out.edges_) for(auto offset:{edge.v00,edge.v01})
    if(offset<0 || offset%sizeof(MeshVertex) || std::size_t(offset)/sizeof(MeshVertex)>=out.vertices_.size()) Bad("VO edge vertex byte offset");
  for(const auto& group:out.groups_) {
    if(group.m_Surface<0 || std::size_t(group.m_Surface)>=out.surfaces_.size()) Bad("VO surface ID");
    const auto range=VectorObjectRange(group,0,0);
    ValidateMeshRange(range,out.indices_,static_cast<std::uint32_t>(out.vertices_.size()));
  }
  for(const auto& frame:out.frames_) {
    Slice(frame.m_UnionStart,frame.m_UnionCnt,out.frame_groups_.size());
    Slice(frame.m_VerStart,frame.m_VerCnt,out.frame_vertices_.size());
    // Original loader reads these fields only when an edge payload exists.
    // Release files without edges can retain heap-fill values (0xbaadf00d).
    if(!out.edges_.empty()) Slice(frame.m_EdgeStart,frame.m_EdgeCnt,out.edges_.size());
    Finite(frame.m_Min);Finite(frame.m_Max);Finite(frame.m_GeoCenter);
    if(!std::isfinite(frame.m_Radius)||frame.m_Radius<0) Bad("VO frame radius");
  }
  auto names=r.Names(L"anims",L"name");
  if(r.Column(L"anims",L"name",ST_WCHAR,false)) {
    auto ids=r.Array<std::uint32_t>(L"anims",L"id",ST_DWORD);
    auto starts=r.Array<std::uint32_t>(L"anims",L"disp",ST_DWORD);
    auto counts=r.Array<std::uint32_t>(L"anims",L"cnt",ST_DWORD);
    out.animation_frames_=r.Array<vo::SVOFrameIndex>(L"anims",L"frames");
    if(ids.size()!=names.size()||starts.size()!=names.size()||counts.size()!=names.size()) Bad("VO animation columns");
    r.Budget(names.size(),sizeof(VectorAnimation));out.animations_.reserve(names.size());
    for(std::size_t i=0;i<names.size();++i) {
      Slice(starts[i],counts[i],out.animation_frames_.size());
      if(!counts[i]) Bad("empty VO animation");
      out.animations_.push_back({std::move(names[i]),ids[i],starts[i],counts[i]});
    }
    for(auto frame:out.animation_frames_)
      if(frame.frame<0||std::size_t(frame.frame)>=out.frames_.size()||!frame.time||frame.time==INT32_MIN) Bad("VO animation frame/time");
  }
  names=r.Names(L"matrices",L"name");
  if(r.Column(L"matrices",L"name",ST_WCHAR,false)) {
    auto ids=r.Array<std::uint32_t>(L"matrices",L"id",ST_DWORD);
    auto starts=r.Array<std::uint32_t>(L"matrices",L"disp",ST_DWORD);
    out.matrices_=r.Array<D3DXMATRIX>(L"matrices",L"data");
    for(const auto& matrix:out.matrices_) for(const auto& row:matrix.m)
      for(float v:row) if(!std::isfinite(v)) Bad("nonfinite VO matrix");
    if(ids.size()!=names.size()||starts.size()!=names.size()) Bad("VO matrix columns");
    r.Budget(names.size(),sizeof(VectorMatrix));out.matrix_names_.reserve(names.size());
    for(std::size_t i=0;i<names.size();++i) {
      Slice(starts[i],out.frames_.size(),out.matrices_.size());
      out.matrix_names_.push_back({std::move(names[i]),ids[i],starts[i]});
    }
  }
  names=r.Names(L"properties",L"name");
  if(r.Column(L"properties",L"name",ST_WCHAR,false)) {
    auto values=r.Names(L"properties",L"value",true);
    if(names.size()!=values.size()) Bad("VO property columns");
    r.Budget(names.size(),sizeof(out.properties_[0]));out.properties_.reserve(names.size());
    for(std::size_t i=0;i<names.size();++i) out.properties_.emplace_back(std::move(names[i]),std::move(values[i]));
  }
  if(auto* simple=r.Column(L"simple",L"data",ST_BYTE,false)) {
    if(simple->GetArraysCount()!=1) Bad("VO simple array count");
    const auto bytes=simple->GetArrayLength(0);const auto* p=simple->GetFirst<std::uint8_t>(0);
    if(bytes<48) Bad("short VO simple geometry");
    VectorSimpleGeometry shape{};std::memcpy(&shape.bounds,p,48);
    const auto nt=shape.bounds.m_TriCnt,nv=shape.bounds.m_VertsCnt;
    if(nt<0||nv<0||48+std::uint64_t(nt)*12+std::uint64_t(nv)*12!=bytes) Bad("VO simple geometry extent");
    r.Budget(nt,sizeof(vo::SVOTriangle));r.Budget(nv,sizeof(D3DXVECTOR3));
    shape.triangles.resize(nt);shape.vertices.resize(nv);
    if(nt) std::memcpy(shape.triangles.data(),p+48,std::size_t(nt)*12);
    if(nv) std::memcpy(shape.vertices.data(),p+48+std::size_t(nt)*12,std::size_t(nv)*12);
    for(const auto& tri:shape.triangles) for(auto i:tri.i) if(i<0||i>=nv) Bad("VO simple triangle ID");
    for(const auto& point:shape.vertices) Finite(point);
    out.simple_=std::move(shape);
  }
  return out;
}
std::vector<MeshRange> VectorGeometry::FrameRanges(std::uint32_t index) const {
  if(index>=frames_.size()) throw std::out_of_range("VO frame");
  const auto& frame=frames_[index];std::vector<MeshRange> out;out.reserve(frame.m_UnionCnt);
  // Stable surface order is equivalent to original Draw's nested loops,
  // preserving duplicate IDs and relative union order within each surface.
  // Sorting avoids an adversarial surfaces * frame-groups traversal.
  for(std::size_t i=0;i<std::size_t(frame.m_UnionCnt);++i)
    out.push_back(VectorObjectRange(groups_[frame_groups_[frame.m_UnionStart+i]],0,0));
  std::stable_sort(out.begin(),out.end(),[](const auto& a,const auto& b){return a.surface<b.surface;});
  return out;
}
}
