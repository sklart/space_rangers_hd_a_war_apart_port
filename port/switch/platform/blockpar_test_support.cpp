// Test-only bridge.  The production ARM64 build sees an empty translation unit;
// host harnesses compile the declarations in tests/ without GR_Main.cpp.
#if !defined(__SWITCH__)
#include "../../tests/blockpar_test_support.cpp"
#endif
