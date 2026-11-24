// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <exception>
namespace srhd_awa::matrixgame::frame_profiler {
using Clock=std::chrono::steady_clock;
enum class Component:unsigned {BeforeDraw,DrawObjects,DrawLandscape,DrawLandscapeSurfaces,DrawWater,DrawShadows,DrawEffects,Draw,Count};
inline constexpr std::array<const char*,unsigned(Component::Count)> names{
 "BeforeDraw","DrawObjects","DrawLandscape","DrawLandscapeSurfaces","DrawWater","DrawShadows","DrawEffects","Draw"};
struct Entry {std::uint64_t calls{},total_us{},max_us{};};
inline thread_local std::array<Entry,unsigned(Component::Count)> entries{};
inline thread_local Clock::time_point last_report{};
class Scope {
 Component component_;Clock::time_point start_;int exceptions_;
public:
 explicit Scope(Component c)noexcept:component_(c),start_(Clock::now()),exceptions_(std::uncaught_exceptions()){}
 ~Scope(){
  const auto elapsed=std::uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-start_).count());
  auto& value=entries[unsigned(component_)];++value.calls;value.total_us+=elapsed;value.max_us=std::max(value.max_us,elapsed);
  if(std::uncaught_exceptions()>exceptions_)std::fprintf(stderr,"[MATRIX_FRAME] exception component=%s elapsed_us=%llu\n",names[unsigned(component_)],(unsigned long long)elapsed);
 }
};
inline void ReportIfDue(){
 const auto now=Clock::now();if(last_report==Clock::time_point{}){last_report=now;return;}
 const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(now-last_report).count();
 if(elapsed<5000)return;
 for(unsigned i=0;i<entries.size();++i){auto& value=entries[i];if(!value.calls)continue;
  std::fprintf(stderr,"[MATRIX_FRAME] window_ms=%lld component=%s calls=%llu avg_us=%llu max_us=%llu\n",
   (long long)elapsed,names[i],(unsigned long long)value.calls,(unsigned long long)(value.total_us/value.calls),(unsigned long long)value.max_us);
  value={};
 }
 last_report=now;
}
}
