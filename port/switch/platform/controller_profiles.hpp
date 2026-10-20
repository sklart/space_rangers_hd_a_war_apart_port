#pragma once
#include <atomic>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>

namespace srhd_awa::platform::controller_profiles {
enum class Profile : std::uint8_t { Hybrid, Cursor };
struct Settings {
  Profile profile{Profile::Hybrid};
  int deadzone{5500};
  double pointer_speed{600.0};
  bool invert_x{}, invert_y{}, acceleration{}, swap_confirm_cancel{};
};
inline bool Valid(const Settings& value) {
  return (value.profile == Profile::Hybrid || value.profile == Profile::Cursor) &&
      value.deadzone >= 0 && value.deadzone <= 30000 && std::isfinite(value.pointer_speed) &&
      value.pointer_speed >= 100 && value.pointer_speed <= 2000;
}
inline std::mutex settings_mutex;
inline Settings settings;
inline std::atomic<std::uint64_t> revision{1};
inline Settings Get() { std::lock_guard lock(settings_mutex); return settings; }
inline bool Set(const Settings& value) {
  if (!Valid(value)) return false;
  std::lock_guard lock(settings_mutex);
  settings = value;
  ++revision;
  return true;
}
inline void ResetDefaults() { Set(Settings{}); }
inline std::string Serialize(const Settings& value) {
  std::ostringstream out;
  out << std::setprecision(17) << "1 " << static_cast<int>(value.profile) << ' ' << value.deadzone << ' '
      << value.pointer_speed << ' ' << value.invert_x << ' ' << value.invert_y << ' '
      << value.acceleration << ' ' << value.swap_confirm_cancel << '\n';
  return out.str();
}
inline bool Parse(const std::string& text, Settings& value) {
  std::istringstream in(text);
  Settings candidate;
  int version{}, profile{}, invert_x{}, invert_y{}, acceleration{}, swap{};
  if (!(in >> version >> profile >> candidate.deadzone >> candidate.pointer_speed >>
        invert_x >> invert_y >> acceleration >> swap) || version != 1 ||
      profile < 0 || profile > 1 || invert_x < 0 || invert_x > 1 || invert_y < 0 || invert_y > 1 ||
      acceleration < 0 || acceleration > 1 || swap < 0 || swap > 1) return false;
  in >> std::ws;
  if (!in.eof()) return false;
  candidate.profile = static_cast<Profile>(profile);
  candidate.invert_x = invert_x; candidate.invert_y = invert_y;
  candidate.acceleration = acceleration; candidate.swap_confirm_cancel = swap;
  if (!Valid(candidate)) return false;
  value = candidate;
  return true;
}
} // namespace srhd_awa::platform::controller_profiles
