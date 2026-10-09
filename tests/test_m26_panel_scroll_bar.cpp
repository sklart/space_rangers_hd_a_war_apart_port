#include "ui_panel_scroll_bar.hpp"
#include "ui_tree_renderer.hpp"

#include <cstdio>
#include <cstdlib>
#include <memory>

namespace {
using namespace srhd_awa::platform::ui;
void Check(bool value, const char* reason) {
  if (!value) { std::fprintf(stderr, "M26 PANELSCROLLBAR FAIL: %s\n", reason); std::exit(1); }
}
void Arrow(UiScrollBar* bar, Size size) {
  auto image = std::make_unique<UiImageLeaf>();
  image->SetSize(size);
  Check(bar->AddImage(ScrollBarPart::Up, ScrollBarState::Normal,
                      std::move(image)), "arrow attachment");
  Check(bar->UpdateLayout(), "arrow layout");
}
}

int main() {
  UiPanel root;
  root.SetSize({1280, 720});
  auto* panel = root.AddPanelScrollBar();
  panel->SetPosition({20, 30});
  panel->SetSize({100, 80});
  Arrow(panel->HorizontalBar(), {10, 10});
  Arrow(panel->VerticalBar(), {10, 10});
  panel->SetHorizontalActive(true);
  panel->SetVerticalActive(true);
  Check(panel->UpdateScrollbarPlacement() &&
        panel->HorizontalBar()->LocalPosition() == Point{0, 70} &&
        panel->HorizontalBar()->ClientSize() == Size{90, 10} &&
        panel->VerticalBar()->LocalPosition() == Point{90, 0} &&
        panel->VerticalBar()->ClientSize() == Size{10, 70},
        "internal auto placement with both bars");
  auto* world = panel->AddObject();
  world->SetPositionModeW(true);
  world->SetPosition({-20, -10});
  world->SetSize({200, 160});
  Check(panel->UpdateScrollRanges() &&
        panel->HorizontalBar()->Minimum() == -20 &&
        panel->HorizontalBar()->Maximum() == 179 &&
        panel->HorizontalBar()->PageSize() == 100 &&
        panel->VerticalBar()->Minimum() == -10 &&
        panel->VerticalBar()->Maximum() == 149 &&
        panel->VerticalBar()->PageSize() == 80,
        "world union excludes internal bars");
  panel->SetUnlimitedWorld(false);
  panel->SetScrollOffset({100, 100});
  Check(panel->ScrollOffset() == Point{80, 70} &&
        panel->HorizontalBar()->Position() == 80 &&
        panel->VerticalBar()->Position() == 70,
        "finite world clamps both axes");
  Check(world->AbsolutePosition() == Point{-80, -50},
        "world child absolute position follows scroll");
  panel->HorizontalBar()->SetPosition(20);
  Check(panel->ScrollOffset().x == 20, "bar position updates panel");
  panel->SetUnlimitedWorld(true);
  panel->SetScrollOffset({100, 100});
  Check(panel->ScrollOffset() == Point{100, 100} &&
        panel->HorizontalBar()->Position() == 80,
        "unlimited world keeps offset while bar clamps");
  panel->SetExternal(true);
  Check(panel->HorizontalBar()->Parent() == &root &&
        panel->VerticalBar()->Parent() == &root &&
        panel->HorizontalBar()->LocalPosition() == Point{20, 110} &&
        panel->HorizontalBar()->ClientSize() == Size{100, 10} &&
        panel->VerticalBar()->LocalPosition() == Point{120, 30} &&
        panel->VerticalBar()->ClientSize() == Size{10, 80},
        "external auto placement in parent coordinates");
  panel->SetAutoHorizontal(false);
  panel->SetHorizontalRect({2, 3, 42, 13});
  Check(panel->HorizontalBar()->LocalPosition() == Point{2, 3} &&
        panel->HorizontalBar()->ClientSize() == Size{40, 10},
        "manual horizontal rectangle");
  panel->SetExternal(false);
  Check(panel->HorizontalBar()->Parent() == panel &&
        panel->VerticalBar()->Parent() == panel,
        "bars reparent internally");
  world->SetActive(false);
  Check(panel->UpdateScrollRanges() && panel->HorizontalBar()->Minimum() == 0 &&
        panel->HorizontalBar()->Maximum() == 99,
        "empty world safe range");
  std::puts("M26 PANELSCROLLBAR CPU PASS");
}
