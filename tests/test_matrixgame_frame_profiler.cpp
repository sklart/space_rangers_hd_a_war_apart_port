#include "../port/matrixgame/native-engine/native_frame_profiler.hpp"
#include <cassert>
#include <stdexcept>
namespace fp=srhd_awa::matrixgame::frame_profiler;
int main(){
 fp::entries={};fp::last_report=fp::Clock::now();
 for(int i=0;i<1000;++i){fp::Scope parent(fp::Component::Draw);fp::Scope child(fp::Component::DrawWater);}
 assert(fp::entries[unsigned(fp::Component::Draw)].calls==1000);
 assert(fp::entries[unsigned(fp::Component::DrawWater)].calls==1000);
 fp::ReportIfDue();assert(fp::entries[unsigned(fp::Component::Draw)].calls==1000);
 try{fp::Scope scope(fp::Component::DrawObjects);throw std::runtime_error("test");}catch(const std::runtime_error&){}
 assert(fp::entries[unsigned(fp::Component::DrawObjects)].calls==1);
 fp::last_report=fp::Clock::now()-std::chrono::seconds(6);fp::ReportIfDue();
 for(const auto& entry:fp::entries)assert(entry.calls==0);
}
