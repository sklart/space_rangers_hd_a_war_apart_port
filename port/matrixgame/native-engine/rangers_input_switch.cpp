// SPDX-License-Identifier: GPL-2.0-or-later
#include "../native/rangers_native_input.hpp"
#include "platform_switch.hpp"
namespace srhd_awa::matrixgame {
RangersNativeInputBackend MakeSwitchRangersInputBackend(platform3g::SwitchWindow& window){
 return {[&window]{window.Update();},[&window]{window.WaitForDrawable();},[&window]{window.CancelHeldInput();},[&window]{return window.Drawable();},
  [&window]{const auto r=window.ReadClientRect();return std::pair{r.right-r.left,r.bottom-r.top};},
  [&window](int x,int y){window.WarpInputCursor(x,y);}};
}
}
