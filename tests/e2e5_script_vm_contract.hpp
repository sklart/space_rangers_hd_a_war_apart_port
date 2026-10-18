// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
// Actual source-language compiler and VM, with original callbacks and live
// game objects. No fake external-function body or renderer is used.
#include "units/EC_Expression.hpp"
#include "units/aScriptFun.hpp"
#include "units/Globals.hpp"
#include "units/aGalaxy.hpp"
#include "units/aRuins.hpp"
#include "types/aItem.hpp"
#include "classes.hpp"
#include <cassert>
#include <bit>
#include <cstdio>
#include <memory>
namespace srhd_awa::test::script_vm {
using namespace EC_Expression;
template<class T> using Owned=std::unique_ptr<T,void(*)(T*)>;
template<class T> Owned<T> own(T* p){return {p,[](auto* v){pas::free(v);}};}
inline void Check(){
 auto scope=own(pas::construct_call<TVarArrayEC>(TVarArrayEC_Create));
 using Callback=pas::Proc<void(pas::OpenArray<TVarEC*>,TCodeEC*)>;
 auto add=[&](const pas::WideString& name,auto fun){scope->Add(name,vkExternFun)->SetExternFun(reinterpret_cast<void*>(pas::callback_address(Callback(fun))));};
 add(u"CurItem"_wref.get(),aScriptFun::SF_CurItem);add(u"GetTalkShip"_wref.get(),aScriptFun::SF_GetTalkShip);
 add(u"ShipDestination"_wref.get(),aScriptFun::SF_ShipDestination);add(u"StarName"_wref.get(),aScriptFun::SF_StarName);
 aGalaxy::TStar star{};star.Name=u"Native VM star"_wref.get();aRuins::TRuins ship{};ship.FlyToStar=&star;
 aItem::TGoods item{},nested{};pas::List stack{};
 auto* old_stack=Globals::ScriptItemContextStack;auto* old_item=Globals::ScriptUseItem;auto* old_talk=Globals::TalkShip;
 Globals::ScriptItemContextStack=&stack;Globals::ScriptUseItem=&item;Globals::TalkShip=&ship;
 scope->Add(u"Ship"_wref.get(),vkDword)->SetDword(reinterpret_cast<std::uintptr_t>(&ship));
 scope->Add(u"DateQuery"_wref.get(),vkString)->SetString(u"Date"_wref.get());
 const auto address=reinterpret_cast<std::uintptr_t>(&star);assert(address>UINT32_MAX);
 unsigned checks=0;
 auto run=[&](const pas::WideString& text,auto check){
  auto analyzer=own(pas::construct_call<TCodeAnalyzerEC>(TCodeAnalyzerEC_Create));
  analyzer->Tokenize(text,0);analyzer->RemoveComments();analyzer->RemoveNewlines();analyzer->RemoveWhitespace();
  assert(analyzer->ValidateDelimiters()==u""_wref.get());
  auto expression=own(pas::construct_call<TExpressionEC>(TExpressionEC_Create));pas::WideString error;
  expression->Compile(analyzer.get(),nullptr,nullptr,nullptr,error);assert(error==u""_wref.get());
  expression->Link(scope.get(),false);TExpressionEC_Evaluate(expression.get(),nullptr,nullptr,nullptr);
  auto* value=expression->GetResult();assert(value);check(value);++checks;
 };
 for(unsigned repeat=0;repeat<3;++repeat){
  run(u"CurItem()"_wref.get(),[&](auto* v){assert(v->GetDword()==reinterpret_cast<std::uintptr_t>(&item));});
  pas::list_add(&stack,&nested);
  run(u"CurItem()"_wref.get(),[&](auto* v){assert(v->GetDword()==reinterpret_cast<std::uintptr_t>(&nested));});
  pas::list_delete(&stack,0);
  run(u"GetTalkShip()"_wref.get(),[&](auto* v){assert(v->GetDword()==reinterpret_cast<std::uintptr_t>(&ship));});
  run(u"ShipDestination(Ship,DateQuery)"_wref.get(),[&](auto* v){assert(v->Kind==vkInt&&v->GetDword()==address);});
  run(u"StarName(ShipDestination(Ship,DateQuery))"_wref.get(),[&](auto* v){assert(v->GetString()==star.Name);});
  run(u"ShipDestination(Ship,DateQuery)+0.0"_wref.get(),[&](auto* v){assert(v->GetFloat()==double(std::bit_cast<std::int32_t>(std::uint32_t(address))));});
 }
 stack.p_clear();Globals::ScriptItemContextStack=old_stack;Globals::ScriptUseItem=old_item;Globals::TalkShip=old_talk;
 ScriptCallTraceCount=0;ScriptCallTracePosition=0;
 std::printf("actual lexer/compiler/linker/VM nested callbacks PASS expressions=%u high_native_addresses=true\n",checks);
}

} // namespace srhd_awa::test::script_vm
