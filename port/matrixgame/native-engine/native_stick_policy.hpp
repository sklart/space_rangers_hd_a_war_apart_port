// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
namespace srhd_awa::matrixgame::native_input {
struct StickPoint {int x{},y{};};
struct ScrollDirections {bool left{},right{},up{},down{};};
inline constexpr int CameraDeadzone=12000;
inline ScrollDirections CameraScroll(StickPoint right)noexcept {
 return {right.x < -CameraDeadzone,right.x > CameraDeadzone,
         right.y > CameraDeadzone,right.y < -CameraDeadzone};
}
struct BattleSticks {StickPoint pointer;ScrollDirections camera;};
inline BattleSticks MapBattleSticks(StickPoint left,StickPoint right)noexcept {
 return {left,CameraScroll(right)};
}
}
