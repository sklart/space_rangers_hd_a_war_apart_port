#pragma once

#include "e2e4_script_value.hpp"
#include "units/EC_Expression.hpp"

namespace srhd_awa::platform::e2e5_script_pointer_int {

// The original ShipDestination("Date") returns the target address through
// SetInt, not FlyDate. Preserve its int tag and ordinary numeric conversions,
// while retaining a native address in the existing wide int/dword storage.
inline void Set(EC_Expression::TVarEC* cell,std::uintptr_t address) {
    cell->SetInt(std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(address)));
    auto* target=EC_Expression::TVarEC_Resolve(cell);
    if(!target)return;
    if(target->Kind==EC_Expression::vkInt)
        target->IntValue=e2e4_script_value::IntStorageFromDword(address);
    else if(target->Kind==EC_Expression::vkDword)
        target->DwordValue=address;
}

} // namespace srhd_awa::platform::e2e5_script_pointer_int
