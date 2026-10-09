#include "ui_scroll_bar.hpp"
#include "ui_tree_renderer.hpp"

#include <cstdio>
#include <cstdlib>
#include <memory>

namespace {
using namespace srhd_awa::platform::ui;
void Check(bool value, const char* reason) {
  if (!value) { std::fprintf(stderr, "M26 SCROLLBAR FAIL: %s\n", reason); std::exit(1); }
}
void Add(UiScrollBar* bar, ScrollBarPart part, ScrollBarState state,
         std::int32_t width, std::int32_t height) {
  auto image = std::make_unique<UiImageLeaf>();
  image->SetSize({width, height});
  Check(bar->AddImage(part, state, std::move(image)), "image triple attachment");
}
}  // namespace

int main() {
  UiScrollBar bar;
  Check(bar.Minimum() == 0 && bar.Maximum() == 99 && bar.Position() == 0 &&
        bar.LargeChange() == 1 && bar.SmallChange() == 1 && bar.PageSize() == 1 &&
        bar.Orientation() == 2 && bar.CalculationMode() == 0,
        "upstream defaults");
  bar.SetOrientation(1);
  bar.SetSize({300, 20});
  for (int state = 0; state < 3; ++state)
    Add(&bar, ScrollBarPart::Up, static_cast<ScrollBarState>(state), 10, 20);
  Add(&bar, ScrollBarPart::Down, ScrollBarState::Normal, 10, 20);
  Add(&bar, ScrollBarPart::Before, ScrollBarState::Normal, 2, 20);
  Add(&bar, ScrollBarPart::After, ScrollBarState::Normal, 2, 20);
  Add(&bar, ScrollBarPart::ThumbTop, ScrollBarState::Normal, 5, 20);
  Add(&bar, ScrollBarPart::ThumbCenter, ScrollBarState::Normal, 2, 20);
  Add(&bar, ScrollBarPart::ThumbBottom, ScrollBarState::Normal, 5, 20);
  bar.SetPageSize(10);
  bar.SetPosition(40);
  Check(bar.UpdateLayout() && !bar.Narrow() && bar.TrackLength() == 280 &&
        bar.ThumbLength() == 28 && bar.BeforeLength() == 112 &&
        bar.AfterLength() == 140, "horizontal mode 0 layout");
  Check(bar.GetHitRegion({-1, 1000}) == 0 && bar.GetHitRegion({0, -1000}) == 1 &&
        bar.GetHitRegion({10, 0}) == 1 && bar.GetHitRegion({11, 0}) == 3 &&
        bar.GetHitRegion({122, 0}) == 3 && bar.GetHitRegion({123, 0}) == 5 &&
        bar.GetHitRegion({300, 0}) == 2 && bar.GetHitRegion({301, 0}) == 0,
        "axis-only upstream hit boundaries");
  bar.SetHoveredRegion(1);
  Check(!bar.Image(ScrollBarPart::Up, ScrollBarState::Normal)->Active() &&
        bar.Image(ScrollBarPart::Up, ScrollBarState::Active)->Active(),
        "hover state selected");
  bar.PressRegion(1);
  Check(bar.Position() == 39 &&
        bar.Image(ScrollBarPart::Up, ScrollBarState::Down)->Active(),
        "press steps and down state");
  bar.ReleaseRegion();
  bar.SetLargeChange(7);
  Check(bar.LargeChange() == 7, "large change assigned");
  bar.SetLargeChange(1);
  Check(bar.LargeChange() == 7, "upstream LargeChange comparison quirk");
  bar.SetPageSize(1);
  bar.SetPosition(40);
  Check(bar.ThumbLength() == 10 && bar.BeforeLength() == 109 &&
        bar.AfterLength() == 161, "minimum thumb branch");
  bar.SetCalculationMode(1);
  bar.SetRange(0, 9);
  bar.SetPageSize(5);
  bar.SetPosition(9);
  Check(bar.Position() == 5 && bar.BeforeLength() == 140,
        "alternative mode clamps position at max-page+1");
  bar.SetSize({25, 20});
  Check(bar.Narrow() && !bar.Image(ScrollBarPart::ThumbCenter,
                                    ScrollBarState::Normal)->Active(),
        "narrow layout hides center");
  std::puts("M26 SCROLLBAR CPU PASS");
}
