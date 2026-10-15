#pragma once

#include <cstdint>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>

namespace srhd_awa::platform::e2e5_pointer_payload {

// The translated dialog callback ABI carries int32 values. Keep ordinary
// numeric choices intact and give pointer choices opaque, owner-scoped tokens.
class Registry {
 public:
  std::int32_t Register(const void* owner, const void* pointer) {
    if (owner == nullptr || pointer == nullptr)
      throw std::invalid_argument("dialog pointer owner/value is null");
    std::lock_guard lock(mutex_);
    auto& owned = by_owner_[owner];
    if (const auto found = owned.find(pointer); found != owned.end())
      return found->second;
    if (by_token_.size() >= 65536 ||
        next_ > std::numeric_limits<std::uint32_t>::max())
      throw std::overflow_error("dialog pointer token limit reached");
    const auto token = static_cast<std::int32_t>(static_cast<std::uint32_t>(next_++));
    by_token_.emplace(token, Entry{owner, pointer});
    owned.emplace(pointer, token);
    return token;
  }

  template <typename Pointer>
  Pointer Resolve(const void* owner, std::int32_t token) const {
    static_assert(std::is_pointer_v<Pointer>);
    std::lock_guard lock(mutex_);
    const auto found = by_token_.find(token);
    if (found == by_token_.end() || found->second.owner != owner)
      throw std::runtime_error("invalid dialog pointer token");
    return reinterpret_cast<Pointer>(const_cast<void*>(found->second.pointer));
  }

  void Release(const void* owner, std::int32_t token) {
    std::lock_guard lock(mutex_);
    const auto found = by_token_.find(token);
    if (found == by_token_.end() || found->second.owner != owner)
      throw std::runtime_error("invalid dialog pointer token release");
    by_owner_.at(owner).erase(found->second.pointer);
    by_token_.erase(found);
  }

  void ReleaseOwner(const void* owner) {
    std::lock_guard lock(mutex_);
    const auto found = by_owner_.find(owner);
    if (found == by_owner_.end()) return;
    for (const auto& [pointer, token] : found->second) by_token_.erase(token);
    by_owner_.erase(found);
  }

 private:
  struct Entry { const void* owner; const void* pointer; };
  mutable std::mutex mutex_;
  std::uint64_t next_ = 0x80000000ull;
  std::unordered_map<std::int32_t, Entry> by_token_;
  std::unordered_map<const void*, std::unordered_map<const void*, std::int32_t>> by_owner_;
};

inline Registry& Global() {
  static Registry registry;
  return registry;
}

inline std::int32_t Register(const void* owner, const void* pointer) {
  return Global().Register(owner, pointer);
}

template <typename Pointer>
Pointer Resolve(const void* owner, std::int32_t token) {
  return Global().Resolve<Pointer>(owner, token);
}

inline void Release(const void* owner, std::int32_t token) {
  Global().Release(owner, token);
}

inline void ReleaseOwner(const void* owner) {
  Global().ReleaseOwner(owner);
}

}  // namespace srhd_awa::platform::e2e5_pointer_payload
