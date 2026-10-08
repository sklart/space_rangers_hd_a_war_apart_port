#include "ui_zone.hpp"

#include <cassert>
#include <cstdio>

using namespace srhd_awa::platform::ui;

int main() {
  UiPanel root;
  root.SetSize({40, 30});
  auto* zone = root.AddZone();
  zone->SetPosition({10, 10});
  zone->SetSize({10, 8});
  assert(zone->GetZoneKind() == ZoneKind::Rect);
  assert(zone->HitTest({10, 10}) && zone->HitTest({19, 17}));
  assert(!zone->HitTest({20, 17}) && !zone->HitTest({19, 18}));
  zone->SetHitTestDisabled(true);
  assert(!zone->HitTest({10, 10}));
  zone->SetHitTestDisabled(false);
  zone->SetActive(false);
  assert(!zone->HitTest({10, 10}));

  zone->SetZoneKind(ZoneKind::Circle);
  // A 10x8 rectangle has diameter 8 and integer centre (15,14).
  assert(zone->HitTest({15, 14}) && zone->HitTest({19, 14}));
  assert(!zone->HitTest({20, 14}) && !zone->HitTest({19, 18}));
  zone->SetHitTestDisabled(true);
  assert(zone->HitTest({15, 14})); // upstream Circle quirk
  zone->SetActive(true);
  zone->SetSize({5, 5});
  assert(zone->HitTest({12, 12}) && zone->HitTest({14, 12}));
  assert(!zone->HitTest({15, 12}));
  std::puts("M24 ZONE HOST TEST PASS");
}
