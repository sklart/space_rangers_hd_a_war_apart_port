#include "units/aScriptFun.hpp"
#include "units/aRuins.hpp"
#include "units/aGalaxy.hpp"
#include "units/EC_Expression.hpp"
#include "units/EC_Buf.hpp"
#include "e2e4_script_value.hpp"
#include <array>
#include <bit>
#include <cassert>
#include <cstdio>
#include <memory>

using namespace EC_Expression;
using Cell = std::unique_ptr<TVarEC, void(*)(TVarEC*)>;
Cell MakeCell(TVarKind kind=vkEmpty) {
  return {pas::construct_call<TVarEC>(TVarEC_Create,kind),[](TVarEC* p){pas::free(p);}};
}
int main() {
  aRuins::TRuins station{};
  aGalaxy::TStar first{},second{};
  first.Name=u"First native star"_wref.get();second.Name=u"Second native star"_wref.get();
  const auto address=reinterpret_cast<std::uintptr_t>(&first);
  assert(address>UINT32_MAX);
  station.FlyToStar=&first;station.FlyDate=1234;
  auto result=MakeCell(),ship=MakeCell(),date=MakeCell(),turn=MakeCell();
  ship->SetDword(reinterpret_cast<std::uintptr_t>(&station));date->SetString(u"Date"_wref.get());turn->SetInt(-123);
  std::array<TVarEC*,4> args{result.get(),ship.get(),date.get(),turn.get()};
  aScriptFun::SF_ShipDestination({args.data(),3},nullptr);
  assert(result->Kind==vkInt && result->GetDword()==address);
  assert(result->GetInt()==std::bit_cast<std::int32_t>(std::uint32_t(address)));
  assert(station.FlyDate==1234);
  aScriptFun::SF_ShipDestination({args.data(),4},nullptr);
  assert(result->Kind==vkInt && result->GetDword()==address && station.FlyDate==-123);

  auto copy=MakeCell();copy->AssignFrom(result.get(),false);
  assert(copy->Kind==vkInt && copy->GetDword()==address);
  copy->ResetKind(vkEmpty);copy->Assume(result.get(),false);
  assert(copy->Kind==vkInt && copy->GetDword()==address);
  auto reference=MakeCell(vkRef);reference->SetRef(copy.get());
  args[0]=reference.get();aScriptFun::SF_ShipDestination({args.data(),3},nullptr);
  assert(reference->Kind==vkRef && copy->Kind==vkInt && reference->GetDword()==address);
  auto name=MakeCell();
  for(auto* carrier:{result.get(),copy.get(),reference.get()}) {
    name->ResetKind(vkEmpty);std::array<TVarEC*,2> read{name.get(),carrier};
    aScriptFun::SF_StarName({read.data(),2},nullptr);
    assert(name->Kind==vkString && name->GetString()==first.Name);
  }

  auto target=MakeCell();target->SetDword(reinterpret_cast<std::uintptr_t>(&second));
  args[0]=result.get();args[2]=target.get();result->ResetKind(vkEmpty);
  aScriptFun::SF_ShipDestination({args.data(),3},nullptr);
  assert(result->Kind==vkDword && result->GetDword()==address && station.FlyToStar==&second);
  args[2]=date.get();result->ResetKind(vkEmpty);
  aScriptFun::SF_ShipDestination({args.data(),3},nullptr);
  assert(result->Kind==vkInt && result->GetDword()==reinterpret_cast<std::uintptr_t>(&second));
  station.FlyToStar=nullptr;result->ResetKind(vkEmpty);
  aScriptFun::SF_ShipDestination({args.data(),3},nullptr);
  assert(result->Kind==vkInt && result->GetInt()==0 && result->GetDword()==0);

  station.FlyToStar=&first;
  result->ResetKind(vkDword);aScriptFun::SF_ShipDestination({args.data(),3},nullptr);
  assert(result->Kind==vkDword && result->GetDword()==address);
  const auto legacy=std::bit_cast<std::int32_t>(std::uint32_t(address));
  result->ResetKind(vkFloat);aScriptFun::SF_ShipDestination({args.data(),3},nullptr);
  assert(result->Kind==vkFloat && result->GetFloat()==double(legacy));
  result->ResetKind(vkString);aScriptFun::SF_ShipDestination({args.data(),3},nullptr);
  assert(result->Kind==vkString && result->GetString()==pas::wide_int_to_str(legacy));
  for(const auto numeric:{INT32_MIN,-123,0,INT32_MAX}) {
    result->ResetKind(vkInt);result->SetInt(numeric);copy->ResetKind(vkInt);
    copy->Assume(result.get(),false);
    assert(copy->Kind==vkInt && copy->IntValue==numeric && copy->GetInt()==numeric);
  }
  for(const auto bits:{0u,0x7fffffffu,0x80000000u,0xffffffffu}) {
    result->ResetKind(vkDword);result->SetDword(bits);copy->ResetKind(vkInt);
    copy->Assume(result.get(),false);
    assert(copy->Kind==vkInt && copy->GetInt()==std::bit_cast<std::int32_t>(bits));
  }

  auto buffer=std::unique_ptr<EC_Buf::TBufEC,void(*)(EC_Buf::TBufEC*)>(
      pas::construct_call<EC_Buf::TBufEC>(EC_Buf::TBufEC_Create),[](auto* p){pas::free(p);});
  result->ResetKind(vkInt);result->SetInt(-123);result->SaveToBuffer(buffer.get());buffer->SetPosition(0);
  copy->LoadFromBuffer(buffer.get());assert(copy->Kind==vkInt && copy->GetInt()==-123);
  station.FlyToStar=&first;result->ResetKind(vkEmpty);
  aScriptFun::SF_ShipDestination({args.data(),3},nullptr);
  buffer->Clear();bool rejected=false;
  try{result->SaveToBuffer(buffer.get());}catch(const pas::Raised&){rejected=true;}
  assert(rejected && result->Kind==vkInt && result->GetDword()==address);
  buffer->Clear();result->SetInt(42);result->SaveToBuffer(buffer.get());buffer->SetPosition(0);
  copy->LoadFromBuffer(buffer.get());assert(copy->GetInt()==42);
  std::puts("real ShipDestination: int tag, high target addresses, setter, copy/reference/assignment, numeric save/load and pointer save rejection PASS");
}
