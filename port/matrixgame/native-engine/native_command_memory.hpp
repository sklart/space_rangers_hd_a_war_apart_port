// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <new>
#include <stdexcept>
#include <utility>
#include <vector>
namespace srhd_awa::matrixgame::platform3g {
enum class CommandMemoryFailure {None,Allocation,Quota};
class CommandMemoryAllocationError final:public std::bad_alloc {
public:const char* what()const noexcept override{return "native command page allocation failed";}
};
// Owns additional backing pages until the caller has established GPU completion
// (or discarded an unsubmitted list). The persistent base/discard pages belong
// to Driver. Discard is real reserved memory, but must NEVER be submitted.
template<class Driver>class NativeCommandMemory {
public:
 static constexpr std::uint32_t BaseBytes=2*1024*1024,QuotaBytes=16*1024*1024,Alignment=4096;
 using Command=typename Driver::Command;using Block=typename Driver::Block;
private:
 Driver driver_;std::vector<Block> pages_;std::uint32_t bytes_{BaseBytes};CommandMemoryFailure failure_{};
 void Fail(CommandMemoryFailure error)noexcept{if(failure_==CommandMemoryFailure::None)failure_=error;}
public:
 explicit NativeCommandMemory(Driver driver):driver_(std::move(driver)){
  // Metadata allocation is outside the C callback. Every spill is >= BaseBytes.
  pages_.reserve(QuotaBytes/BaseBytes);
 }
 NativeCommandMemory(const NativeCommandMemory&)=delete;
 NativeCommandMemory& operator=(const NativeCommandMemory&)=delete;
 static bool SupportedRequest(std::uint64_t minimum)noexcept{return minimum && minimum<=QuotaBytes;}
 static std::uint32_t PageSize(std::uint64_t minimum){
  if(!SupportedRequest(minimum))throw std::length_error("native command request outside quota");
  const auto aligned=(std::uint32_t(minimum)+Alignment-1)&~(Alignment-1);
  return aligned<BaseBytes?BaseBytes:aligned;
 }
 void Refill(Command command,std::uint64_t minimum)noexcept{
  static_assert(noexcept(driver_.Attach(command,std::declval<const Block&>(),0)));
  static_assert(noexcept(driver_.AttachDiscard(command)));
  // The private recorder uses bounded SDK state/draw calls. A single request
  // beyond the entire quota cannot be served even by the reserved discard page;
  // Driver must terminate rather than return insufficient storage to deko3d.
  if(!SupportedRequest(minimum))driver_.RejectRequest(minimum);
  if(failure_==CommandMemoryFailure::None){
   const auto bytes=PageSize(minimum);
   if(bytes_>QuotaBytes-bytes)Fail(CommandMemoryFailure::Quota);
   else try{
    auto page=driver_.Allocate(bytes);
    if(page){
     pages_.push_back(std::move(page));bytes_+=bytes;
     driver_.Attach(command,pages_.back(),bytes);return;
    }
    Fail(CommandMemoryFailure::Allocation);
   }catch(...){Fail(CommandMemoryFailure::Allocation);}
  }
  // Let only the current bounded SDK call return normally. Failed recording is
  // latched and rejected by FinishForSubmission; repeated writes may overwrite
  // this page because NONE of the failed list may reach the GPU.
  driver_.AttachDiscard(command);
 }
 void Check()const{
  if(failure_==CommandMemoryFailure::Allocation)throw CommandMemoryAllocationError();
  if(failure_==CommandMemoryFailure::Quota)throw std::length_error("native command memory quota exhausted");
 }
 template<class Finish>auto FinishForSubmission(Finish finish){
  Check();auto list=finish();Check();return list;
 }
 template<class ClearAndRebind>void Retire(ClearAndRebind clear_and_rebind)noexcept{
  static_assert(noexcept(clear_and_rebind()));
  // dkCmdBufClear rolls back to its LAST backing page. Rebind persistent base
  // before freeing spills, so the SDK never retains a dangling current slice.
  clear_and_rebind();pages_.clear();bytes_=BaseBytes;failure_=CommandMemoryFailure::None;
 }
 std::uint32_t Bytes()const noexcept{return bytes_;}
 // Drain between bounded original draw operations while at least one complete
 // page remains available for the current operation and its final barrier.
 bool NeedsDrain()const noexcept{return bytes_>=QuotaBytes-BaseBytes;}
 std::size_t Pages()const noexcept{return pages_.size();}
 CommandMemoryFailure Failure()const noexcept{return failure_;}
};
}
