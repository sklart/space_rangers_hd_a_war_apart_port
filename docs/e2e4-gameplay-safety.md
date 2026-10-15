# E2E-4 protected-state safety

The E2E-3 audit identified live pointer truncation and a fixed four-byte VMT
skip in `TGalaxy::XorProtectedState`. The E2E-4 disposable override keeps the
original XOR mask/order but walks bytes with native pointers, skips the C++
vtable pointer using `sizeof(void*)`, and excludes each live list-pointer field
using native pointer width. The excluded fields are `TStar`'s five lists,
`TPlanet::EquipmentShop`, `TShip::Inventory`/`Artefacts` and the optional
`TRuins`, `TRanger` and `TPlayer` lists. The original 32-bit serialized IDs
are untouched. Invalid or unsorted exclusion ranges are rejected before any
bytes in that object are modified. The integrity marker range is measured
without narrowing addresses.

`host-e2e4-protected-state-test` applies the same segment transform twice to
a polymorphic object above 4 GiB, checks full-byte restoration, live excluded
pointer dereference and rejection of invalid ranges. Native-layout compile
assertions verify the ordering and bounds of the original game fields on
ARM64. The generated original `aGalaxy.cpp` compiles for ARM64.

This does **not** prove full `TGalaxy::ObfuscateProtectedState` and
`RestoreProtectedState` with a real generated galaxy: that method also reaches
screen state and nested objects absent from the isolated host fixture.
Neither save/load nor script pointer safety is cleared by these tests.
