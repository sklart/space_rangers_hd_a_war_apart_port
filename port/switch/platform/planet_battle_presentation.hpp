// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <memory>
#include <stdexcept>
namespace srhd_awa::platform::planet_battle {
class Lease {
public:
 virtual ~Lease()=default;
 virtual void EnterBattle()=0;
 virtual void Restore()=0;
 virtual bool UsesNativeWindow()const noexcept=0;
};
class Provider {
public:virtual ~Provider()=default;virtual std::unique_ptr<Lease> Capture()=0;
};
inline thread_local Provider* active{};
class Scope {
 Provider* provider_;
public:
 explicit Scope(Provider& value):provider_(&value){if(active)throw std::logic_error("battle caller presentation already bound");active=provider_;}
 ~Scope(){if(active!=provider_)std::terminate();active=nullptr;}
 Scope(const Scope&)=delete;Scope& operator=(const Scope&)=delete;
};
inline std::unique_ptr<Lease> Capture(){
 if(!active)return nullptr;
 auto value=active->Capture();if(!value)throw std::logic_error("battle presentation returned no lease");return value;
}
}
