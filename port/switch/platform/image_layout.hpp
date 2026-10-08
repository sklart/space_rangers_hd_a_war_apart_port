#pragma once
#include "okgf_rle_bridge.hpp"
#include <cstdint>
#include <vector>
namespace srhd_awa::platform::image_layout {
enum class XMode { LeftFill, CenterFill, RightFill, Left, Center, Right };
enum class YMode { TopFill, CenterFill, BottomFill, Top, Center, Bottom };
struct Point { std::int32_t x{}, y{}; };
struct Plan { bool supported{}; std::int32_t first_x{}, first_y{}, end_x{}, end_y{}, step_x{}, step_y{}; std::vector<Point> tiles; };
Plan MakePlan(const okgf_rle_bridge::Rect& bounds,const okgf_rle_bridge::Rect& clip,std::int32_t tile_width,std::int32_t tile_height,XMode x_mode,YMode y_mode);
}  // namespace srhd_awa::platform::image_layout
