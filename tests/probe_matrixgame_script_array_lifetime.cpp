// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef NDEBUG
#error Array lifetime checks require assertions
#endif
#include "script_array_lifetime.hpp"
#include "units/EC_Expression.hpp"
#include <cassert>
#include <cstdio>
#include <thread>
namespace arrays=srhd_awa::platform::script_arrays;
namespace {
int destroyed{};
struct ObservedArray:EC_Expression::TVarArrayEC {
 void p_destroy()override{++destroyed;EC_Expression::TVarArrayEC_Destroy(this);}
};
ObservedArray* New(){return pas::construct_call<ObservedArray>(EC_Expression::TVarArrayEC_Create);}
}
int main(){
 auto* foreign=New();foreign->Add(u"keep"_wref.get(),EC_Expression::vkInt)->SetInt(17);
 for(int cycle=0;cycle<2;++cycle){
  const auto before=destroyed;
  {
   arrays::Scope scope;
   bool rejected=false;try{arrays::Scope duplicate;}catch(const std::logic_error&){rejected=true;}assert(rejected);
   auto* value=New();value->Add(u"value"_wref.get(),EC_Expression::vkInt)->SetInt(42);
   auto* first=pas::construct_call<EC_Expression::TVarEC>(EC_Expression::TVarEC_Create,EC_Expression::vkArray);first->SetArray(value);
   auto* alias=pas::construct_call<EC_Expression::TVarEC>(EC_Expression::TVarEC_Create,EC_Expression::vkArray);alias->SetArray(value);
   assert(arrays::Pending()==1);pas::free(first);
   assert(alias->GetArray()==value && alias->GetArray()->GetVar(u"value"_wref.get())->GetInt()==42 && destroyed==before);
   pas::free(alias);assert(destroyed==before);pas::free(value);assert(destroyed==before+1 && arrays::Pending()==0);
   std::thread worker([]{auto* owned=New();owned->Add(u"worker"_wref.get(),EC_Expression::vkInt)->SetInt(123);});worker.join();
   assert(arrays::Pending()==1 && destroyed==before+1);
   scope.End();assert(destroyed==before+2 && arrays::Pending()==0);
   assert(foreign->GetVar(u"keep"_wref.get())->GetInt()==17);
  }
  assert(destroyed==before+2);
 }
 assert(destroyed==4);pas::free(foreign);assert(destroyed==5);
 std::puts("SCRIPT_ARRAY_LIFETIME original_ctor_dtor=PASS borrowed_alias_survives_cell=PASS explicit_free_unregister=PASS scope_drain_once=PASS foreign_array_survives=PASS worker_thread=PASS duplicate_owner=REJECTED cycles=2");
}
