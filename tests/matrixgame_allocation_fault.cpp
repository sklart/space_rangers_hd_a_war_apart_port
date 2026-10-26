// Test-only ordinary C++ allocation failure injection. Not linked into production.
#include <cstdlib>
#include <new>
namespace matrixgame_fault {long countdown=-1;}
void* operator new(std::size_t n){
    if(matrixgame_fault::countdown==0){matrixgame_fault::countdown=-1;throw std::bad_alloc();}
    if(matrixgame_fault::countdown>0)--matrixgame_fault::countdown;
    if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();
}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
