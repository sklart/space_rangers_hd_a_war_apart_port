// Separate TU keeps Windows SDK declarations independent of Delphi declarations.
#include <windows.h>
#include <cstddef>
extern "C" std::size_t WindowsCriticalSize(){return sizeof(CRITICAL_SECTION);}
extern "C" std::size_t WindowsCriticalAlignment(){return alignof(CRITICAL_SECTION);}
