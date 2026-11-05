// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "native_frame_scope.hpp"
#include <cstdint>
#include <stdexcept>
namespace srhd_awa::matrixgame::platform3g {
// Bound outstanding GPU uploads, not the amount of original work in a frame.
class NativeDrawBatch {
 std::uint64_t draws_{},bytes_{},draw_limit_,byte_limit_;
public:
 enum Limit : unsigned {DrawCount=1, UploadBytes=2};
 // Count is an independent SDK-object bound: small uniform uploads can exhaust
 // mapping resources long before the byte budget is reached. Draws are drained,
 // never discarded; the logical frame/scene remains live.
 static constexpr std::uint64_t DrawLimit=64,ByteLimit=256ull*1024*1024;
 explicit NativeDrawBatch(std::uint64_t draws=DrawLimit,std::uint64_t bytes=ByteLimit):draw_limit_(draws),byte_limit_(bytes){
  if(!draws || !bytes)throw std::invalid_argument("native draw batch limits");
 }
 unsigned LimitsReached(std::uint64_t bytes)const{
  if(!bytes || bytes>byte_limit_)throw std::length_error("native single draw upload budget");
  return (draws_>=draw_limit_?DrawCount:0u) | (bytes_>byte_limit_-bytes?UploadBytes:0u);
 }
 bool NeedsDrain(std::uint64_t bytes)const{return LimitsReached(bytes)!=0;}
 void Add(std::uint64_t bytes){
  if(NeedsDrain(bytes))throw std::logic_error("native draw batch requires completion before append");
  ++draws_;bytes_+=bytes;
 }
 void Retired()noexcept{draws_=bytes_=0;}
 std::uint64_t Draws()const noexcept{return draws_;}
 std::uint64_t Bytes()const noexcept{return bytes_;}
};
// Driver must retain packet/target owners if submit or completion fails. A
// successful wait permits release, while the logical scene/image stays live.
template<class Driver,class... Args>void DrainNativeDrawBatch(NativeFrameScope& frame,NativeDrawBatch& batch,Driver& driver,Args... args){
 static_assert(noexcept(driver.ReleaseCompleted()),"native batch retirement must not throw");
 frame.RequireBatch();
 driver.SubmitAndWait(args...);
 driver.ReleaseCompleted();
 batch.Retired();
}
}
