// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
namespace srhd_awa::matrixgame::platform3g {
// Cache immutable CPU metadata only; weak ownership never extends GPU lifetime.
template<class Resource,class Value,std::size_t Slots>class NativeWeakResourceCache {
    struct Entry{std::weak_ptr<Resource> resource;const Resource* identity;Value value;};
    std::array<std::optional<Entry>,Slots> entries_{};
public:
    template<class Factory>const Value& Get(std::size_t slot,const std::shared_ptr<Resource>& resource,Factory&& factory,bool& reused){
        if(slot>=Slots || !resource)throw std::invalid_argument("resource cache slot/value");
        auto& entry=entries_[slot];
        // The supplied strong pointer keeps its owner alive. Compare both its
        // control block and subobject address without acquiring another owner.
        if(entry && entry->identity==resource.get() && !entry->resource.owner_before(resource) && !resource.owner_before(entry->resource)){
            reused=true;return entry->value;
        }
        auto value=std::forward<Factory>(factory)(resource);
        entry=Entry{resource,resource.get(),std::move(value)};reused=false;return entry->value;
    }
};
}
