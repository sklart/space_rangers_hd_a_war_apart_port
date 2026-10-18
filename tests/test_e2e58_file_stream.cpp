// Linked to the real effective game, adapter, package reader and thread/events.
#include "units/EC_File.hpp"
#include "units/EC_FileStream.hpp"
#include "units/EC_HsFile.hpp"
#include "units/WindowsSdk.hpp"
#include "ec_file_adapter.hpp"
#include "win32_compat_sync.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <new>
#include <thread>
#include <vector>

namespace {
std::atomic<std::size_t> fail_size{};
std::atomic<bool> injected{}, release_failure{};
}
void* operator new(std::size_t size) {
  auto wanted = size;
  if (fail_size.compare_exchange_strong(wanted, 0)) {
    injected = true;
    // A second reader must actually wait while PackageFileLock is owned.
    while (!release_failure.load()) std::this_thread::yield();
    throw std::bad_alloc();
  }
  if (auto* p = std::malloc(size ? size : 1)) return p;
  throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

int main(int argc, char** argv) {
  assert(argc == 3);
  namespace file = srhd_awa::platform::ec_file;
  std::string error;
  assert(file::OpenPackage(argv[1], &error));
  const auto descriptors = [&] {
    int count = 0;
    for (const auto& fd : std::filesystem::directory_iterator("/proc/self/fd")) {
      std::error_code ec;
      const auto path = std::filesystem::read_symlink(fd.path(), ec);
      if (!ec && (path == argv[1] || path == argv[2])) ++count;
    }
    return count;
  };
  const auto before = descriptors();
  // Constructor/open failure cleans up the partially created real thread.
  bool failed = false;
  try {
    auto* s = pas::construct_call<EC_FileStream::TFileStreamEC>(
        EC_FileStream::TFileStreamEC_Create, 0x400ff, u"MISSING"_w);
    pas::free(s);
  } catch (...) { failed = true; }
  assert(failed);

  // The bounded package reader allocates packed and decoded blocks only.
  for (auto size : {65536u, 72112u}) {
    injected = false; release_failure = false; fail_size = size;
    auto* stream = pas::construct_call<EC_FileStream::TFileStreamEC>(
        EC_FileStream::TFileStreamEC_Create, 0x400ff, u"MUSIC"_w);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!injected && std::chrono::steady_clock::now() < deadline)
      std::this_thread::yield();
    assert(injected);
    std::atomic<bool> waiter_started{}, waiter_done{};
    std::thread waiter([&] {
      waiter_started = true;
      auto* other = pas::construct_call<EC_File::TFileEC>(EC_File::TFileEC_Create);
      other->SetFileName(u"MUSIC"_w);
      other->AcquireReadHandle(false);
      char byte{}; other->ReadBuffer(&byte, 1);
      pas::free(other);
      waiter_done = true;
    });
    while (!waiter_started) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    assert(!waiter_done);
    release_failure = true;
    assert(stream->WaitForIdle(3000));
    assert(!stream->IsRunning());
    waiter.join(); assert(waiter_done);
    // Failed OS worker cannot silently be restarted, and destruction must join.
    assert(!srhd_awa::platform::win32_compat::IsThreadUsable(stream->ThreadHandle));
    pas::free(stream);
    assert(descriptors() == before);
  }
  // A healthy replacement reader can finish after all failed readers.
  auto* stream = pas::construct_call<EC_FileStream::TFileStreamEC>(
      EC_FileStream::TFileStreamEC_Create, 0x400ff, u"MUSIC"_w);
  std::vector<char> bytes(1100003);
  assert(stream->Read(bytes.data(), bytes.size()) == static_cast<int>(bytes.size()));
  pas::free(stream);
  assert(descriptors() == before);
  file::ClosePackage();
  assert(file::OpenPackage(argv[2], &error));
  stream = pas::construct_call<EC_FileStream::TFileStreamEC>(
      EC_FileStream::TFileStreamEC_Create, 0x400ff, u"MUSIC"_w);
  // Streaming now reaches corruption only when the final block is requested.
  // Drain the real reader instead of assuming the entire resource is prefetched.
  bool read_failed = false;
  int read_count = 0;
  try { read_count = stream->Read(bytes.data(), bytes.size()); }
  catch (...) { read_failed = true; }
  assert(read_failed || read_count < static_cast<int>(bytes.size()));
  assert(stream->WaitForIdle(3000));
  pas::free(stream);
  // The actual file lock remains available after a decoder/read failure.
  std::thread after_error([] {
    pas::critical_enter(EC_HsFile::PackageFileLock);
    pas::critical_leave(EC_HsFile::PackageFileLock);
  });
  after_error.join();
  assert(descriptors() == before);
  file::ClosePackage();
  std::puts("real TFileStreamEC: open/block OOM, late decode/read failure, contending reader, IdleEvent, join, fd cleanup, replacement PASS");
}
