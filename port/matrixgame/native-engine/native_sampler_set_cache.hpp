// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <optional>
namespace srhd_awa::matrixgame::platform3g {
// Addresses are borrowed from the current upload arena and must be forgotten
// before that arena is retired or reused.
class NativeSamplerSetCache {
public:
 using Key=std::array<std::array<std::uint32_t,8>,8>;
private:
 std::optional<Key> key_;std::uint64_t address_{};
public:
 bool Find(const Key& key,std::uint64_t& address)const noexcept{
  if(!key_ || *key_!=key)return false;address=address_;return true;
 }
 void Publish(const Key& key,std::uint64_t address)noexcept{key_=key;address_=address;}
 void Invalidate()noexcept{key_.reset();address_=0;}
};
}
