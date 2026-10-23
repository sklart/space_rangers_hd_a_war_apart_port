// Actual blocked host threads, independently observed through bounded snapshots.
#include "e2e515_perf.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
using namespace srhd_awa::platform::perf515;
int main(int argc,char**argv){
 assert(argc==2);const auto root=std::filesystem::path(argv[1]);assert(!std::filesystem::exists(root));
 std::filesystem::create_directories(root);{std::ofstream f(root/"perf-mode.txt");f<<"basic\n";}
 Initialize(argv[1]);assert(Mode()==1);
 std::atomic<bool> waiting{};bool release{};std::mutex mutex;std::condition_variable resume;
 std::thread worker([&]{ThreadScope phase(ThreadRole::Turn,"turn-fixture-real-wait");waiting=true;
   std::unique_lock lock(mutex);resume.wait(lock,[&]{return release;});});
 while(!waiting.load())std::this_thread::yield();
 {
  ThreadScope keyboard(ThreadRole::Main,"osk-active");
  const auto main=InspectThread(ThreadRole::Main),turn=InspectThread(ThreadRole::Turn);
  assert(main.consistent && turn.consistent && main.thread_token && turn.thread_token && main.thread_token!=turn.thread_token);
  assert(std::string(main.phase)=="osk-active" && std::string(turn.phase)=="turn-fixture-real-wait");
  const auto progress=turn.progress;
  std::this_thread::sleep_for(std::chrono::milliseconds(1100)); // One real heartbeat, no repeated long run.
  const auto still_waiting=InspectThread(ThreadRole::Turn);
  assert(still_waiting.consistent && still_waiting.progress==progress && still_waiting.since_us==turn.since_us);
 }
 assert(std::string(InspectThread(ThreadRole::Main).phase)=="idle");
 {std::lock_guard lock(mutex);release=true;}resume.notify_one();worker.join();
 assert(std::string(InspectThread(ThreadRole::Turn).phase)=="idle");
 const auto began=std::chrono::steady_clock::now();Shutdown();
 assert(std::chrono::steady_clock::now()-began<std::chrono::seconds(3));
 std::ifstream f(root/"logs/perf515-stage.log");const std::string log((std::istreambuf_iterator<char>(f)),{});
 assert(log.find("watchdog role=main")!=log.npos && log.find("phase=osk-active")!=log.npos);
 assert(log.find("watchdog role=turn")!=log.npos && log.find("phase=turn-fixture-real-wait")!=log.npos);
 assert(log.find("shutdown drain/join begin")!=log.npos && log.find("writer queue drained")!=log.npos);
 Initialize(argv[1]); // A new session must not inherit a previous worker identity/phase.
 assert(InspectThread(ThreadRole::Turn).thread_token==0);
 {ThreadScope reentry(ThreadRole::Main,"second-session");assert(std::string(InspectThread(ThreadRole::Main).phase)=="second-session");}
 Shutdown();
 std::puts("PASS real blocked thread distinction, native identity, OSK scope restore, heartbeat and bounded shutdown; no Switch freeze reproduced");
}
