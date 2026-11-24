// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstddef>
#include <vector>
namespace srhd_awa::matrixgame::platform3g {
// Device-local CPU scratch. Nested draws get distinct buffers; oversized
// allocations are released at scope exit rather than retained indefinitely.
template<class T>class NativeVertexScratch {
    static constexpr std::size_t Slots=2,RetainElements=4096;
    struct Slot{std::vector<T> values;bool in_use{};};
    std::array<Slot,Slots> slots_{};
public:
    class Lease {
        Slot* slot_{};std::vector<T> fallback_;
    public:
        Lease(NativeVertexScratch& owner,std::size_t count){
            for(auto& slot:owner.slots_)if(!slot.in_use){slot_=&slot;slot.in_use=true;break;}
            try{Values().assign(count,T{});}catch(...){if(slot_)slot_->in_use=false;throw;}
        }
        Lease(const Lease&)=delete;Lease& operator=(const Lease&)=delete;
        Lease(Lease&&)=delete;Lease& operator=(Lease&&)=delete;
        ~Lease(){if(slot_){if(slot_->values.capacity()>RetainElements)std::vector<T>{}.swap(slot_->values);slot_->in_use=false;}}
        std::vector<T>& Values()noexcept{return slot_?slot_->values:fallback_;}
    };
    Lease Acquire(std::size_t count){return Lease(*this,count);}
};
}
