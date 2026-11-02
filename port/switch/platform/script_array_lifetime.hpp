// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "runtime_support.hpp"
#include <exception>
#include <mutex>
#include <stdexcept>
#include <unordered_set>
namespace srhd_awa::platform::script_arrays {
class Scope;
inline std::mutex mutex;
inline Scope* active{};
void Created(pas::Object*);
void Destroyed(pas::Object*)noexcept;
std::size_t Pending();
// A VM owns all arrays constructed within its lifetime. SetArray/AssignFrom
// keep their original borrowed aliases; explicit destruction unregisters the
// object. Remaining arrays are reclaimed only after every VM consumer stops.
class Scope {
 std::unordered_set<pas::Object*> arrays;
 bool owns=true;
 friend void Created(pas::Object*);
 friend void Destroyed(pas::Object*)noexcept;
 friend std::size_t Pending();
public:
 Scope(){std::lock_guard lock(mutex);if(active)throw std::logic_error("script array scope already owned");active=this;}
 Scope(const Scope&)=delete;Scope& operator=(const Scope&)=delete;
 void End()noexcept{
  for(;;){
   pas::Object* value{};
   {
    std::lock_guard lock(mutex);if(!owns)return;if(active!=this)std::terminate();
    if(arrays.empty()){active=nullptr;owns=false;return;}
    const auto entry=arrays.begin();value=*entry;arrays.erase(entry);
   }
   try{pas::free(value);}catch(...){std::terminate();}
  }
 }
 ~Scope(){End();}
};
inline void Created(pas::Object* value){std::lock_guard lock(mutex);if(active)active->arrays.insert(value);}
inline void Destroyed(pas::Object* value)noexcept{std::lock_guard lock(mutex);if(active)active->arrays.erase(value);}
inline std::size_t Pending(){std::lock_guard lock(mutex);return active?active->arrays.size():0;}
}
