// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <functional>
#include <utility>
#include <stdexcept>
#include <thread>
namespace srhd_awa::platform::rangers_ui_input {
using Dispatch=std::function<void(std::uint32_t,std::uint32_t,std::int32_t)>;
class Source {
public:
 virtual ~Source()=default;
 virtual bool Enabled()const noexcept=0;
 virtual bool Pump(const Dispatch&)=0;
 virtual bool KeyHeld(std::uint8_t)const noexcept=0;
 virtual std::pair<int,int> Cursor()const noexcept=0;
 virtual bool OwnsWindow(std::uint32_t)const noexcept=0;
 virtual void Warp(int,int)=0;
 virtual void Suspend()=0;
 virtual void Resume()=0;
};
inline thread_local Source* installed{};
inline Source* Current()noexcept{return installed && installed->Enabled()?installed:nullptr;}
class Scope {
 Source* source_;std::thread::id thread_=std::this_thread::get_id();
public:
 explicit Scope(Source& source):source_(&source){if(installed)throw std::logic_error("native Rangers input source already owned");installed=source_;}
 ~Scope(){if(thread_!=std::this_thread::get_id() || installed!=source_)std::terminate();installed=nullptr;}
 Scope(const Scope&)=delete;Scope& operator=(const Scope&)=delete;
};
inline void Suspend(){if(installed)installed->Suspend();}
inline void Resume(){if(installed)installed->Resume();}
}
