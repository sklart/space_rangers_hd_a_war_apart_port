#include "ui_object.hpp"
#include "ui_tree_fingerprint.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

using srhd_awa::platform::ui::Point;
using srhd_awa::platform::ui::Rect;
using srhd_awa::platform::ui::Size;
using srhd_awa::platform::ui::UiObject;
using srhd_awa::platform::ui::UiPanel;
using srhd_awa::platform::ui::UiTree;

namespace {
void Check(bool value, const char* message) {
  if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}

void TestOwnershipAndDepth() {
  UiTree tree;
  auto* root = tree.Root();
  auto* low = root->AddObject(); low->SetName("low"); low->SetDepth(-10);
  auto* equal_a = root->AddObject(); equal_a->SetName("equal-a"); equal_a->SetDepth(5);
  auto* equal_b = root->AddObject(); equal_b->SetName("equal-b"); equal_b->SetDepth(5);
  auto* high = root->AddObject(); high->SetName("high"); high->SetDepth(100);
  Check(root->Children().at(0)->Name() == "high", "depth descending first");
  Check(root->Children().at(1)->Name() == "equal-b", "equal depth newest first");
  Check(root->Children().at(2)->Name() == "equal-a", "equal depth prior second");
  Check(root->Children().at(3)->Name() == "low", "depth descending last");
  low->SetDepth(200);
  Check(root->Children().front().get() == low, "SetDepth reinserts child");

  auto* panel = root->AddPanel();
  auto* child = panel->AddObject(); child->SetName("child");
  std::string error;
  Check(child->Reparent(root, &error), "reparent preserves ownership");
  Check(child->Parent() == root, "reparent parent");
  Check(!root->Reparent(child, &error), "root cannot reparent");
}

void TestGeometryAndState() {
  UiTree tree;
  auto* root = tree.Root(); root->SetPosition({10, 20}); root->SetSize({100, 80});
  auto* panel = root->AddPanel(); panel->SetPosition({30, 40}); panel->SetOrigin({5, 6}); panel->SetSize({50, 40});
  auto* child = panel->AddObject(); child->SetPosition({7, 8}); child->SetOrigin({1, 2}); child->SetSize({20, 10});
  Check(panel->AbsolutePosition() == Point{40, 60}, "panel absolute");
  Check(child->AbsolutePosition() == Point{47, 68}, "child absolute");
  Check(child->HitTestBounds() == Rect{46, 66, 66, 76}, "child hit bounds");
  Check(child->ContainsPoint({46, 66}) && child->ContainsPoint({65, 75}), "inclusive left/top exclusive right/bottom");
  Check(!child->ContainsPoint({66, 75}) && !child->ContainsPoint({65, 76}), "exclusive edges");
  Check(child->ToLocalPoint({50, 71}) == Point{3, 3}, "base ToLocal");
  Check(child->ToAbsolutePoint({3, 3}) == Point{50, 71}, "base ToAbsolute");
  child->SetHitTestDisabled(true);
  Check(!child->ContainsPoint({46, 66}), "hit test disabled");
  child->SetHitTestDisabled(false); child->SetActive(false);
  Check(!child->ContainsPoint({46, 66}), "inactive no hit test");
  child->SetActive(true);
  Check(child->ContainsPoint({46, 66}), "reactivated hit test geometry");
}

void TestPanelScrollAndInvalidation() {
  UiTree tree;
  auto* root = tree.Root(); root->SetSize({200, 200});
  auto* panel = root->AddPanel(); panel->SetPosition({20, 30}); panel->SetOrigin({2, 3}); panel->SetSize({50, 40});
  auto* fixed = panel->AddObject(); fixed->SetPosition({10, 11}); fixed->SetSize({4, 4});
  auto* scrolling = panel->AddObject(); scrolling->SetPosition({10, 11}); scrolling->SetSize({4, 4}); scrolling->SetPositionModeW(true);
  panel->SetScrollOffset({15, 20});
  Check(fixed->AbsolutePosition() == Point{30, 41}, "non ModeW child stays fixed");
  Check(scrolling->AbsolutePosition() == Point{15, 21}, "ModeW child scrolls once");
  Check(panel->ToLocalPoint({20, 30}) == Point{15, 20}, "panel ToLocal includes scroll");
  Check(panel->ToAbsolutePoint({15, 20}) == Point{20, 30}, "panel ToAbsolute includes scroll");
  Check(panel->GetVisibleContentRect() == Rect{13, 17, 63, 57}, "visible content rect");
  panel->ScrollRectIntoView({0, 0, 10, 10});
  Check(panel->ScrollOffset() == Point{2, 3}, "scroll rect bottom top right left order");
  tree.ClearDirtyRects(); fixed->SetPosition({12, 13});
  Check(!tree.DirtyRects().empty(), "moving active object invalidates geometry");
}

void TestNamesAndCycleGuard() {
  UiTree tree;
  auto* root = tree.Root(); root->SetName("same");
  auto* first = root->AddObject(); first->SetName("same");
  auto* nested = first->AddObject(); nested->SetName("target");
  auto* second = root->AddObject(); second->SetName("target");
  Check(root->FindByNameRecursive("same") == root, "name self first");
  Check(root->FindByNameRecursive("target") == second, "name follows depth child order");
  std::string error;
  Check(!first->Reparent(nested, &error), "descendant reparent rejected");
  srhd_awa::platform::ui_fingerprint::Value fingerprint{};
  Check(srhd_awa::platform::ui_fingerprint::ComputeTree(*root, &fingerprint, &error) && fingerprint.bytes > 0 && fingerprint.crc32 != 0 && fingerprint.fnv64 != 0, "tree fingerprint");
}
}  // namespace

int main() {
  TestOwnershipAndDepth();
  TestGeometryAndState();
  TestPanelScrollAndInvalidation();
  TestNamesAndCycleGuard();
  std::cout << "UI OBJECT TEST PASS\n";
}
