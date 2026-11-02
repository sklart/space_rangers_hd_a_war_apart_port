// Real platform CPU accounting and wall-clock diagnostic writer.
#include "e2e515_perf.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
int main(int argc,char** argv) {
 assert(argc==2);
 assert(std::filesystem::create_directory(argv[1]));
 namespace p=srhd_awa::platform::perf515;
 const auto before=p::CpuMicros();assert(before>=0);
 p::Initialize(argv[1]);assert(p::Mode()>0);
 const auto end=std::chrono::steady_clock::now()+std::chrono::milliseconds(200);
 std::atomic<unsigned long long> work{};
 while(std::chrono::steady_clock::now()<end)work.fetch_add(1,std::memory_order_relaxed);
 const auto after=p::CpuMicros();assert(after>before && work.load()>0);
 p::Record("clock-probe",200000,after-before,0,0);
 p::Shutdown();
 std::printf("PERF_CLOCK cpu_before=%lld cpu_after=%lld work=%llu writer_join=PASS\n",
  static_cast<long long>(before),static_cast<long long>(after),work.load());
}
