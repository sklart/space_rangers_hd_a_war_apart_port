#include "gai_playback_cpu.hpp"

#include <cstdio>
#include <string>
#include <vector>

using namespace srhd_awa::platform;
int main() {
  gai_cpu::GaiSequence sequence{0, {{2,10},{0,20},{1,30}}};
  gai_playback_cpu::State state{}; std::string error; std::vector<gai_playback_cpu::Step> steps;
  if (!gai_playback_cpu::Initialize(&state,sequence,&error) || state.sequence_frame != 0) return 1;
  if (!gai_playback_cpu::AdvanceBy(&state,sequence,9,&steps,&error) || !steps.empty() || state.sequence_frame != 0) return 1;
  if (!gai_playback_cpu::AdvanceBy(&state,sequence,1,&steps,&error) || steps.size()!=1 || steps[0].sequence_frame!=1 || steps[0].source_frame_index!=0) return 1;
  if (!gai_playback_cpu::AdvanceBy(&state,sequence,19,&steps,&error) || !steps.empty()) return 1;
  if (!gai_playback_cpu::AdvanceBy(&state,sequence,1,&steps,&error) || state.sequence_frame!=2) return 1;
  if (!gai_playback_cpu::AdvanceBy(&state,sequence,30,&steps,&error) || steps.size()!=1 || !steps[0].wrapped || state.sequence_frame!=0 || state.cycles_completed!=1) return 1;
  if (!gai_playback_cpu::AdvanceBy(&state,sequence,65,&steps,&error) || state.sequence_frame!=0 || state.elapsed_in_frame_ms!=5 || state.cycles_completed!=2 || steps.size()!=3) return 1;
  if (!gai_playback_cpu::Stop(&state,&error) || !gai_playback_cpu::AdvanceBy(&state,sequence,100,&steps,&error) || state.sequence_frame!=0) return 1;
  if (!gai_playback_cpu::Restart(&state,sequence,&error) || !gai_playback_cpu::SetFramePosition(&state,sequence,2,false,&error) || state.sequence_frame!=2) return 1;
  state.stop_after_one_cycle=true; if (!gai_playback_cpu::AdvanceBy(&state,sequence,30,&steps,&error) || state.running || state.sequence_frame!=0 || state.cycles_completed!=3) return 1;
  gai_cpu::GaiSequence one{0,{{7,1}}}; if (!gai_playback_cpu::Initialize(&state,one,&error) || !gai_playback_cpu::AdvanceBy(&state,one,1000,&steps,&error) || !steps.empty()) return 1;
  gai_cpu::GaiSequence bad{0,{{0,0}}}; if (gai_playback_cpu::Initialize(&state,bad,&error)) return 1;
  std::puts("GAI PLAYBACK CPU PASS"); return 0;
}
