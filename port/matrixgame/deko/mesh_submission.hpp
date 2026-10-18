// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <type_traits>
namespace srhd_awa::matrixgame {
struct MeshRange {
  std::int32_t surface{}, base_vertex{};
  std::uint32_t first_index{}, index_count{}, minimum_vertex{}, vertex_count{};
};
// Direct translation of the two VectorObject.cpp DrawIndexedPrimitive branches.
// Group is the native SVOUnion (or a controlled contract input in portable tests).
// No index rebasing, reordering, truncation or geometry conversion is performed.
template<class Group>
MeshRange VectorObjectRange(const Group& group,std::int32_t vertex_base,std::int32_t index_base) {
  static_assert(std::is_same_v<decltype(group.m_Base),std::int32_t> &&
      std::is_same_v<decltype(group.m_IBase),std::int32_t> &&
      std::is_same_v<decltype(group.m_TriCnt),std::int32_t> &&
      std::is_same_v<decltype(group.m_TriStart),std::int32_t> &&
      std::is_same_v<decltype(group.m_VerCnt),std::int32_t> &&
      std::is_same_v<decltype(group.m_Surface),std::int32_t>);
  if(vertex_base<0 || index_base<0 || group.m_VerCnt<0 || group.m_TriCnt<0)
    throw std::invalid_argument("invalid VectorObject draw extent");
  const std::int64_t base=std::int64_t(group.m_Base)+vertex_base;
  const std::int64_t first=group.m_IBase<0 ? -std::int64_t(group.m_IBase)+index_base :
      std::int64_t(group.m_TriStart)*3+index_base;
  const std::int64_t count=std::int64_t(group.m_TriCnt)*3;
  if(base<std::numeric_limits<std::int32_t>::min() || base>std::numeric_limits<std::int32_t>::max() ||
      first<0 || first>std::numeric_limits<std::uint32_t>::max() ||
      count>std::numeric_limits<std::uint32_t>::max())
    throw std::overflow_error("VectorObject draw field overflow");
  return {group.m_Surface,static_cast<std::int32_t>(base),static_cast<std::uint32_t>(first),
      static_cast<std::uint32_t>(count),group.m_IBase<0 ? 0u : static_cast<std::uint32_t>(group.m_VerMinIndex),
      static_cast<std::uint32_t>(group.m_VerCnt)};
}
inline void ValidateMeshRange(const MeshRange& range,std::span<const std::uint16_t> indices,
    std::uint32_t available_vertices) {
  if(range.index_count%3 || std::uint64_t(range.first_index)+range.index_count>indices.size())
    throw std::out_of_range("mesh index range");
  const auto end=std::uint64_t(range.minimum_vertex)+range.vertex_count;
  for(const auto index:indices.subspan(range.first_index,range.index_count)) {
    const auto vertex=std::int64_t(range.base_vertex)+index;
    if(index<range.minimum_vertex || index>=end || vertex<0 || vertex>=available_vertices)
      throw std::out_of_range("mesh effective vertex range");
  }
}
} // namespace srhd_awa::matrixgame
