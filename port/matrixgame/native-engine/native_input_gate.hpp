// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
namespace srhd_awa::matrixgame::native_input {
// A new window/focus owner must see a release before adopting an already held
// physical key/button/contact. Other newly pressed bits pass immediately.
template<class Mask>class HeldUntilRelease {
 Mask blocked_{};
public:
 void Prime(Mask held)noexcept{blocked_=held;}
 Mask Filter(Mask held)noexcept{blocked_&=held;return Mask(held & ~blocked_);}
};
}
