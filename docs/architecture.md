# Architecture

`port/switch/` is exclusively **Space Rangers HD: A War Apart**. Its application identifier, NRO target, save/log root and variables use `SpaceRangersHDAWarApart` / `space-rangers-hd-a-war-apart`; a future Space Rangers 1 port must use a separate sibling, never `rangers` as a shared writable root.

Planned boundaries: game code → platform common interfaces → Switch SDL2/libnx implementations. Interfaces cover GameRoot/resource paths, user data, time, threads/events, input pointer emulation, audio, window/presentation, logging and dynamic-library replacement. Fixed-width serialized values remain `uint32_t`/`int32_t`; runtime addresses use `uintptr_t`/`intptr_t`.
