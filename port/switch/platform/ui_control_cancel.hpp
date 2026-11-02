// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "units/GI_MessageLoop.hpp"
#include "types/GI_GraphButton.hpp"
namespace srhd_awa::platform::controller_screen_context {
inline void CancelMomentaryButtons(GI_MessageLoop::TObjectGI* object) {
  if (!object) return;
  if (auto* button = pas::class_cast_if<GI_GraphButton::TGraphButtonGI*>(object)) {
    if (button->Down && (button->Kind == GI_GraphButton::gbkNormal ||
                         button->Kind == GI_GraphButton::gbkDisable)) {
      button->Down = false;
      if (button->MessageLoop) button->UpdateStateVisuals();
    }
  }
  for (auto* child = object->FirstChild; child; child = child->NextSibling)
    CancelMomentaryButtons(child);
}

}
