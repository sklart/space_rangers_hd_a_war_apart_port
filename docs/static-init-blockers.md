# Static initialization blockers after the EC_File slice

Milestone 4 deliberately does not link `GR_Main.cpp`. The first real resource
operation is now `TFileEC` over `DATA/common.pkg`; no game global or
`GR_Main` static initializer is required for that path.

The next attempted game-owned startup route is `GR_Main::InitializePlatformRuntime`.
Its reached dependencies are Win32 window registration/creation, DirectSound,
registry-backed configuration and later the configured package collection.
Those are not individual missing imports that can safely be stubbed: linking
the unit would trigger the static/UI initialization avalanche described by the
dependency map. They remain out of scope for Milestone 4.

The next narrow investigation should isolate the configuration and
`aPacket::LoadConfiguredPackages` resource branch before any window, audio or
registry initialization. No `GR_Main` code was added to the NRO for this
milestone.
