// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <utility>
namespace srhd_awa::matrixgame::platform3g {
// Caches CPU descriptor values only, never references to GPU upload memory.
template<class Key,class Value,std::size_t Slots>class NativeDescriptorCache {
    struct Entry {Key key;Value value;};
    std::array<std::optional<Entry>,Slots> entries_{};
public:
    template<class Factory>const Value& Get(std::size_t slot,const Key& key,Factory&& factory,bool& reused){
        if(slot>=Slots)throw std::out_of_range("descriptor cache slot");
        auto& entry=entries_[slot];
        if(entry && entry->key==key){reused=true;return entry->value;}
        // Descriptors depend only on the complete key, not the texture slot.
        for(const auto& other:entries_)if(other && other->key==key){
            entry=Entry{key,other->value};reused=true;return entry->value;
        }
        auto next=std::forward<Factory>(factory)(); // Failure leaves old entry intact.
        entry=Entry{key,std::move(next)};reused=false;return entry->value;
    }
};
}
