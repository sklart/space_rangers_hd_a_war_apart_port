// Original CHeap uses real calloc/realloc/free in its release implementation.
#include "CHeap.hpp"
#include "CException.hpp"
#include <string>
namespace Base {
void CHeap::AllocationError(size_t bytes) {
    throw CExceptionStr(__FILE__, __LINE__, L"MatrixLib allocation failed: " + std::to_wstring(bytes));
}
}
