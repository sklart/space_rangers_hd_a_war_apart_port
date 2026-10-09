#include "ui_input.hpp"
#include "ui_graph_button.hpp"
#include "ui_edit.hpp"
#include "ui_scroll_bar.hpp"
#include "ui_tree_renderer.hpp"
#include "ui_zone.hpp"
#include "m23_aft_fixture.hpp"

#include <cassert>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace srhd_awa::platform::ui;

namespace {
struct Probe : UiObject {
  explicit Probe(std::vector<std::string>& log, std::string id) : log(log), id(std::move(id)) {}
  void OnMouseEnter() override { log.push_back(id + ".enter"); }
  void OnMouseLeave() override { log.push_back(id + ".leave"); }
  void ProcessPointerMove(Point) override { log.push_back(id + ".move"); }
  void ProcessDoubleClick(Point) override { log.push_back(id + ".double"); }
  void OnFocusLost() override { log.push_back(id + ".focus_lost"); }
  std::vector<std::string>& log;
  std::string id;
};
std::vector<std::uint8_t> GraphHitGi() {
  std::vector<std::uint8_t> bytes(104);
  std::memcpy(bytes.data(), "gi\0", 3);
  const auto put = [&bytes](std::size_t at, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) bytes[at + i] = value >> (i * 8);
  };
  put(4, 1); put(16, 2); put(20, 2);
  put(24, 0xf800); put(28, 0x07e0); put(32, 0x001f);
  put(44, 1); put(64, 96); put(68, 8); put(80, 2); put(84, 2);
  for (std::size_t at = 98; at < 104; at += 2) {
    bytes[at] = 0x00; bytes[at + 1] = 0xf8;
  }
  return bytes;
}
}

int main() {
  std::vector<std::string> log;
  UiObject root;
  root.SetName("root"); root.SetSize({100, 100});
  auto a = std::make_unique<Probe>(log, "a");
  a->SetName("a"); a->SetPosition({5, 5}); a->SetSize({50, 50});
  auto* ap = a.get(); assert(root.Attach(std::move(a)));
  auto b = std::make_unique<Probe>(log, "b");
  b->SetName("b"); b->SetPosition({10, 10}); b->SetSize({50, 50});
  auto* bp = b.get(); assert(root.Attach(std::move(b)));
  UiInputRouter router(root);
  router.PointerMove({20, 20});
  assert((log == std::vector<std::string>{"b.enter", "b.move", "a.enter", "a.move"}));
  router.SetFocusedControl(ap);
  log.clear(); router.PointerMove({90, 90});
  assert((log == std::vector<std::string>{"a.move", "b.leave", "a.leave"}));
  router.SetFocusedControl(bp);
  auto detached = root.Detach(bp);
  assert(router.FocusedControl() == nullptr);
  assert(log.back() == "b.focus_lost");
  assert(router.QueryPointOcclusionState({20, 20}, ap) == -1);
  auto blocker = std::make_unique<Probe>(log, "blocker");
  blocker->SetPosition({0, 0}); blocker->SetSize({80, 80}); blocker->SetMouseBlocking(true);
  blocker->SetDepth(-1);
  root.Attach(std::move(blocker));
  assert(router.QueryPointOcclusionState({20, 20}, ap) == 1);
  router.SetFocusedControl(ap);
  ap->SetActive(false);
  assert(router.FocusedControl() == nullptr);
  {
    UiObject double_root;
    double_root.SetSize({20, 20});
    auto probe = std::make_unique<Probe>(log, "double");
    probe->SetSize({10, 10});
    double_root.Attach(std::move(probe));
    UiInputRouter input(double_root);
    log.clear();
    input.PointerDoubleClick(UiPointerButton::Left, {5, 5});
    input.PointerDoubleClick(UiPointerButton::Right, {5, 5});
    assert((log == std::vector<std::string>{"double.double", "double.double"}));
  }

  {
    UiObject control_root;
    control_root.SetSize({200, 100});
    auto* graph = control_root.AddGraphButton();
    graph->SetName("button"); graph->SetPosition({10, 10}); graph->SetSize({30, 20});
    graph->SetHitKind(GraphButtonHitKind::Rect);
    graph->SetHasOnPressCode(true);
    graph->SetHelp("button-help", "press");
    auto* zone = control_root.AddZone();
    zone->SetName("zone"); zone->SetPosition({60, 10}); zone->SetSize({20, 20});
    zone->SetHasMouseEnterCode(true);
    zone->SetHasMouseLeaveCode(true);
    zone->SetHasOnKeyCode(true);
    UiInputRouter input(control_root);
    input.PointerMove({15, 15});
    assert(input.HoveredControl() == graph && graph->Hovered() && input.CurrentHelp() == "press");
    input.PointerDown(UiPointerButton::Left, {15, 15});
    assert(graph->Down());
    input.PointerUp(UiPointerButton::Left, {15, 15});
    assert(!graph->Down());
    input.PointerMove({65, 15});
    assert(!graph->Hovered() && input.CurrentHelp().empty() && zone->CursorInside());
    input.PointerDown(UiPointerButton::Left, {65, 15});
    input.PointerUp(UiPointerButton::Left, {65, 15});
    input.PointerLeave();
    assert(!zone->CursorInside());
    graph->SetButtonKind(GraphButtonKind::Fix);
    input.PointerMove({15, 15});
    input.PointerDown(UiPointerButton::Left, {15, 15});
    input.PointerUp(UiPointerButton::Left, {15, 15});
    assert(graph->Down());
    input.PointerDown(UiPointerButton::Left, {15, 15});
    input.PointerUp(UiPointerButton::Left, {15, 15});
    assert(!graph->Down());
    graph->SetButtonKind(GraphButtonKind::Disable);
    graph->SetDisabled(true);
    input.PointerDown(UiPointerButton::Left, {15, 15});
    input.PointerUp(UiPointerButton::Left, {15, 15});
    assert(!graph->Down());
    graph->SetButtonKind(GraphButtonKind::FixDisable);
    input.PointerDown(UiPointerButton::Left, {15, 15});
    input.PointerUp(UiPointerButton::Left, {15, 15});
    assert(!graph->Down());
    graph->SetActive(false);
    assert(input.HoveredControl() == nullptr);
    zone->SetHasRightClickCode(true);
    input.PointerDown(UiPointerButton::Right, {65, 15});
    input.PointerUp(UiPointerButton::Right, {65, 15});
    input.KeyDown(UiKey::Enter);
    zone->SetZoneKind(ZoneKind::Circle);
    input.PointerMove({60, 10});
    assert(zone->MouseInside() && !zone->CursorInside());
    input.PointerMove({70, 20});
    assert(zone->CursorInside());
    bool down{}, up{}, activate{}, zone_down{}, zone_up{}, right_script{}, press_script{},
         enter_script{}, leave_script{}, key_script{};
    for (const auto& action : input.Actions()) {
      down |= action.kind == UiActionKind::ButtonDown;
      up |= action.kind == UiActionKind::ButtonUp;
      activate |= action.kind == UiActionKind::ButtonActivate;
      zone_down |= action.kind == UiActionKind::ZoneDown;
      zone_up |= action.kind == UiActionKind::ZoneUp;
      right_script |= action.kind == UiActionKind::DeferredScriptRequested &&
                      action.control.ends_with("/zone") && action.payload == "OnMouseRightClick";
      enter_script |= action.kind == UiActionKind::DeferredScriptRequested &&
                      action.control.ends_with("/zone") && action.payload == "OnMouseEnterCode";
      leave_script |= action.kind == UiActionKind::DeferredScriptRequested &&
                      action.control.ends_with("/zone") && action.payload == "OnMouseLeaveCode";
      key_script |= action.kind == UiActionKind::DeferredScriptRequested &&
                    action.control.ends_with("/zone") && action.payload == "OnKey";
      press_script |= action.kind == UiActionKind::DeferredScriptRequested &&
                      action.control.ends_with("/button") && action.payload == "OnPressCode";
    }
    assert(down && up && activate && zone_down && zone_up && right_script &&
           enter_script && leave_script && key_script && press_script);
    const auto hash = FingerprintActions(input.Actions());
    assert(hash.bytes > 0 && hash.crc32 && hash.fnv64);
  }

  {
    UiObject pixel_root;
    pixel_root.SetSize({30, 30});
    auto* graph = pixel_root.AddGraphButton();
    graph->SetPosition({10, 10}); graph->SetSize({2, 2});
    graph->SetHitKind(GraphButtonHitKind::Graph);
    auto image = std::make_unique<UiImageLeaf>();
    const auto bytes = GraphHitGi();
    std::string error;
    assert(image->LoadBytes(srhd_awa::platform::image_object::Kind::GI,
                            bytes.data(), bytes.size(), "synthetic-graph-hit", "", &error));
    assert(graph->AddStateImage(GraphButtonSlot::Normal, std::move(image), &error));
    UiInputRouter input(pixel_root);
    input.PointerMove({10, 10});
    assert(input.HoveredControl() != graph);
    input.PointerMove({11, 10});
    assert(input.HoveredControl() == graph);
  }
  {
    UiObject edit_root;
    edit_root.SetSize({100, 50});
    const auto bytes = m23_test::AftFixture();
    auto font = std::make_shared<srhd_awa::platform::aft_font::AftFont>();
    assert(font->Load(bytes.data(), bytes.size()));
    auto* edit = edit_root.AddEdit();
    edit->SetPosition({5, 5}); edit->SetSize({30, 8});
    edit->SetFont("fixture", font); edit->SetText(u"ABC");
    auto* next = edit_root.AddEdit();
    next->SetPosition({40, 5}); next->SetSize({30, 8});
    next->SetFont("fixture", font);
    auto* third = edit_root.AddEdit();
    third->SetPosition({75, 5}); third->SetSize({20, 8});
    third->SetFont("fixture", font);
    UiInputRouter input(edit_root);
    input.PointerDown(UiPointerButton::Left, {10, 7});
    assert(input.FocusedControl() == edit && edit->Focused() && edit->CaretPosition() == 3);
    input.PointerDown(UiPointerButton::Left, {45, 7});
    assert(input.FocusedControl() == next && !edit->Focused() && next->Focused());
    next->SetActive(false);
    assert(input.FocusedControl() == nullptr);
    next->SetActive(true);
    input.PointerDown(UiPointerButton::Left, {10, 7});
    assert(input.FocusedControl() == edit && edit->Focused() && !next->Focused());
    input.Update(200); assert(edit->CaretBlinkOn());
    input.Update(1); assert(!edit->CaretBlinkOn());
    input.KeyDown(UiKey::Left); input.TextInput(u'A');
    assert(edit->Text() == u"ABAC");
    edit->SetAcceptCharacter([](char16_t c) { return c != u'B'; });
    input.TextInput(u'B'); assert(edit->Text() == u"ABAC");
    input.TextInput(u'\u2603'); assert(edit->Text() == u"ABAC");
    edit->SetMaxLength(4);
    input.TextInput(u'A'); assert(edit->Text() == u"ABAC");
    edit->SetText(u"ABC"); edit->SetCaretPosition(2);
    input.KeyDown(UiKey::Backspace, {.shift = true});
    assert(edit->Text().empty() && edit->CaretPosition() == 0);
    input.KeyDown(UiKey::Tab);
    assert(input.FocusedControl() == next);
    input.KeyDown(UiKey::Tab, {.shift = true});
    assert(input.FocusedControl() == edit);
    input.PointerDown(UiPointerButton::Left, {45, 7});
    assert(input.FocusedControl() == next);
    input.KeyDown(UiKey::Tab);
    assert(input.FocusedControl() == third);
    input.KeyDown(UiKey::Tab, {.shift = true});
    assert(input.FocusedControl() == next);
    input.PointerDown(UiPointerButton::Left, {10, 7});
    edit->SetActive(false);
    assert(input.FocusedControl() == nullptr);
  }
  {
    UiObject first, second;
    first.SetSize({100, 100}); second.SetSize({100, 100});
    auto* graph = first.AddGraphButton();
    graph->SetPosition({10, 10}); graph->SetSize({20, 20});
    graph->SetHitKind(GraphButtonHitKind::Rect);
    UiInputRouter input(first);
    input.PointerMove({15, 15});
    input.SetFocusedControl(graph);
    assert(input.HoveredControl() == graph && input.FocusedControl() == graph);
    auto owned = first.Detach(graph);
    assert(input.HoveredControl() == nullptr && input.FocusedControl() == nullptr);
    graph->SetPosition({70, 10});
    assert(second.Attach(std::move(owned)));
    UiInputRouter moved(second);
    moved.PointerMove({15, 15});
    assert(moved.HoveredControl() == nullptr);
    moved.PointerMove({75, 15});
    assert(moved.HoveredControl() == graph);
  }
  {
    UiObject bar_root;
    bar_root.SetSize({100, 140});
    auto* bar = bar_root.AddScrollBar();
    bar->UiObject::SetPosition({10, 10}); bar->SetSize({20, 100});
    bar->SetRange(0, 100); bar->SetPosition(10);
    UiInputRouter input(bar_root);
    const Point point{15, 20};
    input.PointerMove(point);
    input.PointerDown(UiPointerButton::Left, point);
    assert(bar->Position() == 11);
    input.Update(499); assert(bar->Position() == 11);
    input.Update(1); assert(bar->Position() == 12);
    input.Update(50); assert(bar->Position() == 13);
    input.Update(100); assert(bar->Position() == 15);
    input.PointerUp(UiPointerButton::Left, point);
    input.Update(1000); assert(bar->Position() == 15);
    input.PointerDown(UiPointerButton::Left, point);
    assert(bar->Position() == 16);
    bar->SetActive(false);
    input.Update(1000);
    assert(bar->Position() == 16 && input.FocusedControl() == nullptr);
  }
  {
    auto owned_root = std::make_unique<UiObject>();
    owned_root->SetSize({40, 40});
    auto* child = owned_root->AddObject();
    child->SetSize({20, 20});
    auto input = std::make_unique<UiInputRouter>(*owned_root);
    input->SetFocusedControl(child);
    input->PointerMove({5, 5});
    owned_root.reset();
    assert(input->FocusedControl() == nullptr && input->HoveredControl() == nullptr);
    input.reset();
  }
}
