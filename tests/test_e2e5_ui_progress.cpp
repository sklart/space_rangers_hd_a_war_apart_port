#include "e2e_stage.hpp"
#include <cassert>
#include <chrono>
#include <future>
#include <thread>
using namespace srhd_awa::platform::e2e_stage;
int main() {
  UiProgressScope outer(16, 2); outer.Mark(14);
  auto before = g_ui_progress.load();
  {
    UiProgressScope nested(15, 1); nested.Mark(4);
    auto reader = std::async(std::launch::async, [] {
      // Simulate the UI retaining its log lock inside a blocked operation.
      // The independent music worker must still obtain progress immediately.
      auto text = UiProgressText();
      assert(text.find("ui_screen=15 ui_operation=1 ui_stage=4") != std::string::npos);
    });
    std::lock_guard lock(g_stage_mutex);
    assert(reader.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    reader.get();
  }
  assert(g_ui_progress.load() == before);
  try { UiProgressScope input(16,3); input.Mark(513); throw 1; } catch (int) {}
  assert(g_ui_progress.load() == before);
}
