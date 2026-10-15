#include "e2e_threads.hpp"
#include "win32_compat_sync.hpp"
#include <cassert>
#include <cstdint>
namespace api = srhd_awa::platform::win32_compat;
namespace backend = srhd_awa::platform::e2e_threads;
// This focused harness uses thread/event handles only.
namespace srhd_awa::platform::win32_compat { bool CloseFileHandle(std::uint32_t) { return false; } }
std::int32_t Fail(void*) { backend::MarkCurrentFailed(); return 0; }
int main() {
  auto create = reinterpret_cast<std::uint32_t(*)(void*,std::uint32_t,backend::Entry,void*,std::uint32_t,std::uint32_t*)>(api::ResolveSyncImport("kernel32.dll","CreateThread"));
  auto resume = reinterpret_cast<std::uint32_t(*)(std::uint32_t)>(api::ResolveSyncImport("kernel32.dll","ResumeThread"));
  auto wait = reinterpret_cast<std::uint32_t(*)(std::uint32_t,std::uint32_t)>(api::ResolveSyncImport("kernel32.dll","WaitForSingleObject"));
  auto close = reinterpret_cast<std::int32_t(*)(std::uint32_t)>(api::ResolveSyncImport("kernel32.dll","CloseHandle"));
  std::uint32_t id=0;
  auto token=create(nullptr,0,Fail,nullptr,4,&id);
  assert(token && api::IsThreadUsable(token));
  assert(!backend::IsUsable(token)); // A frontend token is not a backend handle.
  assert(api::PreferCalculationCore(token));
  assert(resume(token)==1);
  assert(wait(token,2000)==0xffffffffu);
  assert(!api::IsThreadUsable(token));
  assert(close(token));
  assert(!api::PreferCalculationCore(token));
}
