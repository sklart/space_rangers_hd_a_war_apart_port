// SPDX-License-Identifier: GPL-2.0-or-later
#include "rangers_exception_boundary.hpp"
#include <cassert>
#include <cstdio>
#include <stdexcept>

int main(){
 namespace m=srhd_awa::matrixgame;
 bool unwound=false,caught=false;
 struct Guard {bool& value;~Guard(){value=true;}};
 try {m::TranslateRangersExceptions([&]() -> int {Guard guard{unwound};throw std::runtime_error("native-init-failed");});}
 catch(...) {
  auto* e=pas::class_cast_if<pas::Exception*>(pas::caught_object());
  assert(e && e->message==pas::AnsiString("native-init-failed"));caught=true;
 }
 assert(unwound && caught);
 auto* original=pas::make_exception<pas::Exception>("original-pascal-error"_a);
 try {m::TranslateRangersExceptions([&]() -> int {pas::raise(original);});}
 catch(...) {assert(pas::caught_object()==original);}
 try {m::TranslateRangersExceptions([]() -> int {throw std::bad_alloc();});}
 catch(...) {assert(pas::class_cast_if<pas::Exception*>(pas::caught_object()));}
 assert(m::TranslateRangersExceptions([]{return 42;})==42);
 std::puts("RANGERS_EXCEPTION_BOUNDARY native_to_pascal=PASS unwind_before_conversion=PASS pascal_and_oom_preserved=PASS");
}
