// SPDX-License-Identifier: GPL-2.0-or-later
#include "rangers_input_queue.hpp"
#include "native_input_engine.hpp"
// Retained SDK link root only. Not called by a diagnostic NRO or by tests.
extern "C" int RangersInputQueueSdkLinkRoot(){
 namespace m=srhd_awa::matrixgame;namespace n=m::native_input;
 m::RangersInputQueue queue(1024,768,1280,720);n::UiTargetScope scope(queue);n::EngineSink sink;
 sink.Key(true,17);sink.Move(640,360);sink.Mouse(n::Button::Down,1,640,360);
 sink.Mouse(n::Button::Up,1,640,360);sink.Key(false,17);sink.ReconcileMouseKeys({},0);
 int messages=0;while(queue.Take())++messages;return messages;
}
