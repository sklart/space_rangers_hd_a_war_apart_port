// Real effective EC_Cache, class metadata, EC_Buf, locks and event handles.
// Only the resource producer is controlled; no copied cache model is used.
#include "units/EC_Cache.hpp"
#include "units/EC_Buf.hpp"
#include "units/EC_CacheBuf.hpp"
#include "units/GR_Main.hpp"
#include "win32_handles.hpp"
#include "e2e5_memory_recovery.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <thread>

namespace {
std::atomic<unsigned> phase{}, loads{}, live_data{}, live_buffers{};
std::atomic<bool> entered{}, release_loader{};
std::atomic<std::uint32_t> pending_event{}, successful_event{};
struct Data : EC_Cache::TCacheDataEC {
  PAS_CLASS_META(Data, EC_Cache::TCacheDataEC, "NativeCacheProbeData", 36)
  EC_Buf::TBufEC* buffer{};
  void p_destroy() override {
    if (buffer) { pas::free(buffer); buffer = nullptr; --live_buffers; }
    --live_data;
    EC_Cache::TCacheDataEC::p_destroy();
  }
  void LoadFromKey(const pas::WideString&) override {
    const auto ordinal = ++loads;
    if (ordinal == 2) successful_event = LoadCompleteEvent;
    if (ordinal == 1) {
      pending_event = LoadCompleteEvent;
      entered = true;
      while (!release_loader) std::this_thread::yield();
      if (phase == 1) throw std::bad_alloc();
    }
    buffer = pas::construct_call<EC_Buf::TBufEC>(EC_Buf::TBufEC_Create);
    ++live_buffers;
    buffer->SetSize(65536);
    std::memset(buffer->Data, 0x6d, buffer->DataSize);
    if (ordinal == 1 && phase == 2) throw std::bad_alloc();
    ResidentBytes = buffer->DataSize;
  }
};
struct Control : EC_Cache::TCacheControlEC {
  PAS_CLASS_META(Control, EC_Cache::TCacheControlEC, "NativeCacheProbeControl", 24)
  EC_Cache::TCacheDataEC* CreateData() override {
    auto* data = pas::construct_call<Data>(EC_Cache::TCacheDataEC_Create);
    ++live_data;
    return data;
  }
};
void await(const std::atomic<bool>& flag) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (!flag && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
  assert(flag);
}
}
int main() {
#if defined(E2E56_RECLAIM_STRESS)
  phase = 0; loads = live_data = live_buffers = 0; release_loader = true;
  (void)pas::class_ref<Data>();
  auto* stress_cache = pas::construct_call<EC_Cache::TCacheEC>(EC_Cache::TCacheEC_Create);
  GR_Main::GlobalCache = stress_cache;
  auto* stress_control = pas::construct_call<Control>(EC_Cache::TCacheControlEC_Create);
  stress_control->SetCacheKey(u"Controlled.Native.Reclaim"_w);
  std::atomic<unsigned> ready{}, calls{};
  std::atomic<bool> start{}, done{};
  std::thread writer([&] {
    ++ready; while (!start) std::this_thread::yield();
    for (unsigned i = 0; i < 200; ++i) {
      auto* data = stress_control->AcquireDataFromDirectKey(pas::class_ref<Data>());
      assert(data && data->ResidentBytes == 65536);
      std::this_thread::yield();
      stress_control->Release();
      stress_cache->TrimToBudget(0);
    }
    done = true;
  });
  std::thread reclaimer([&] {
    ++ready; while (!start) std::this_thread::yield();
    do {
      srhd_awa::platform::e2e5_memory_recovery::Reclaim("concurrent-native", 65536);
      ++calls;
      std::this_thread::yield();
    } while (!done);
  });
  while (ready != 2) std::this_thread::yield();
  start = true;
  writer.join(); reclaimer.join();
  assert(calls && !live_data && !live_buffers && !stress_cache->ResidentBytes);
  pas::free(stress_control); pas::free(stress_cache); GR_Main::GlobalCache = nullptr;
  std::printf("real native concurrent reclaim: loads=%u reclaim_calls=%u cleanup PASS\n",
      loads.load(), calls.load());
  return 0;
#endif
  namespace handles = srhd_awa::platform::win32_compat;
  for (unsigned fault : {1u, 2u}) {
    phase = fault; loads = live_data = live_buffers = 0;
    entered = release_loader = false; pending_event = successful_event = 0;
    auto* cache = pas::construct_call<EC_Cache::TCacheEC>(EC_Cache::TCacheEC_Create);
    GR_Main::GlobalCache = cache;
    auto* first = pas::construct_call<Control>(EC_Cache::TCacheControlEC_Create);
    auto* second = pas::construct_call<Control>(EC_Cache::TCacheControlEC_Create);
    first->SetCacheKey(u"Controlled.Native.Cache"_w);
    second->SetCacheKey(u"Controlled.Native.Cache"_w);
    std::atomic<bool> failed{}, contender_started{}, contender_done{};
    std::thread producer([&] {
      try { first->AcquireDataFromDirectKey(pas::class_ref<Data>()); }
      catch (const std::bad_alloc&) { failed = true; }
    });
    await(entered);
    assert(pending_event && handles::Handles().IsValid(pending_event, handles::HandleType::Event));
    std::thread contender([&] {
      contender_started = true;
      auto* data = second->AcquireDataFromDirectKey(pas::class_ref<Data>());
      assert(data && data->LoadCompleteEvent == 0);
      contender_done = true;
    });
    await(contender_started);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    assert(!contender_done && loads == 1);
    release_loader = true;
    producer.join(); contender.join();
    assert(failed && contender_done && loads == 2);
    assert(!first->BoundData && first->RetainCount == 0);
    assert(second->BoundData && second->RetainCount == 1);
    assert(!handles::Handles().IsValid(pending_event, handles::HandleType::Event));
    assert(successful_event && !handles::Handles().IsValid(successful_event, handles::HandleType::Event));
    assert(live_data == 1 && live_buffers == 1 && cache->ResidentBytes == 65536);
    auto* data = pas::checked_cast<Data*>(first->AcquireDataFromDirectKey(pas::class_ref<Data>()));
    assert(data == second->BoundData && first->RetainCount == 1 && loads == 2);
    for (int i = 0; i < data->buffer->DataSize; ++i) assert(static_cast<unsigned char*>(data->buffer->Data)[i] == 0x6d);
    first->Release(); second->Release();
    cache->TrimToBudget(0);
    assert(!first->BoundData && !second->BoundData);
    assert(!cache->MostRecentData && !cache->LeastRecentData && cache->ResidentBytes == 0);
    assert(!live_data && !live_buffers);
    pas::free(first); pas::free(second); pas::free(cache);
    GR_Main::GlobalCache = nullptr;
    std::printf("real native cache: fault_phase=%u concurrent retry, retained shared data, trim, buffer/event cleanup PASS\n", fault);
  }
  // Native raw-buffer resources must contribute their owned byte capacity to
  // the cache budget; otherwise quest/script data can remain invisible to it.
  auto* cache = pas::construct_call<EC_Cache::TCacheEC>(EC_Cache::TCacheEC_Create);
  GR_Main::GlobalCache = cache;
  auto* source = pas::construct_call<EC_Buf::TBufEC>(EC_Buf::TBufEC_Create);
  source->SetSize(131079);
  std::memset(source->Data, 0x27, source->DataSize);
  auto* raw = pas::construct_call<EC_CacheBuf::TCBufEC>(EC_CacheBuf::TCBufEC_Create);
  raw->LoadFromConfigBuffer(source, pas::WideString());
  std::printf("native raw cache bytes=%d capacity=%d resident=%d\n", raw->Buffer->DataSize,
      raw->Buffer->Capacity, raw->ResidentBytes);
  std::fflush(stdout);
  assert(raw->ResidentBytes == raw->Buffer->Capacity);
  assert(raw->Buffer->DataSize == source->DataSize);
  assert(!std::memcmp(raw->Buffer->Data, source->Data, source->DataSize));
  auto* control = pas::construct_call<EC_CacheBuf::TCBufControlEC>(EC_Cache::TCacheControlEC_Create);
  control->BoundData = raw; control->RetainCount = 1; raw->AppendControl(control);
  cache->AddDataToLruHead(raw); cache->ResidentBytes += raw->ResidentBytes;
  assert(!srhd_awa::platform::e2e5_memory_recovery::Reclaim("native-raw-retained", 131079));
  assert(control->BoundData == raw && cache->ResidentBytes == raw->Buffer->Capacity);
  control->Release();
  assert(srhd_awa::platform::e2e5_memory_recovery::Reclaim("native-raw-released", 131079));
  assert(!control->BoundData && !cache->ResidentBytes && !cache->MostRecentData);
  pas::free(control); pas::free(source); pas::free(cache); GR_Main::GlobalCache = nullptr;
  std::puts("real native raw cache: capacity accounting, retained protection and real recovery reclaim PASS");
}
