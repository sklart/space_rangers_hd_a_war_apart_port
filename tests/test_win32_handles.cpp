#include "win32_handles.hpp"

#include <cassert>
#include <memory>
#include <thread>

using namespace srhd_awa::platform::win32_compat;

int main() {
  auto& handles = Handles();
  const auto event = handles.Allocate(HandleType::Event, std::make_shared<int>(42));
  const auto file = handles.Allocate(HandleType::File, std::make_shared<int>(7));
  assert(event != 0 && event != 0xffffffffu && event != file);
  assert(*static_cast<int*>(handles.Lookup(event, HandleType::Event).get()) == 42);
  SetLastError(kErrorAlreadyExists);
  assert(!handles.TryLookup(event, HandleType::File));
  assert(GetLastError() == kErrorAlreadyExists);
  assert(handles.TryLookup(event, HandleType::Event));
  assert(GetLastError() == kErrorAlreadyExists);
  assert(!handles.Lookup(event, HandleType::File));
  assert(GetLastError() == kErrorInvalidHandle);
  assert(!handles.Close(event, HandleType::File));
  assert(handles.Close(event, HandleType::Event));
  assert(!handles.Close(event, HandleType::Event));
  assert(!handles.Lookup(event, HandleType::Event));
  const auto newer = handles.Allocate(HandleType::Event, std::make_shared<int>(8));
  assert(newer != event && !handles.Lookup(event, HandleType::Event));
  assert(handles.Close(newer, HandleType::Event));
  assert(handles.Close(file, HandleType::File));
  std::thread worker([] {
    assert(GetLastError() == kErrorSuccess);
    assert(!Handles().Lookup(0xffffffffu, HandleType::Thread));
    assert(GetLastError() == kErrorInvalidHandle);
  });
  worker.join();
  assert(GetLastError() == kErrorSuccess);
}
