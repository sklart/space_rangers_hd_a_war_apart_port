// Ownership contract only; full original manager/robot loading is tested by Init.
#include "skin_callbacks.hpp"
#include <cassert>
#include <cstdio>
#include <stdexcept>
using namespace srhd_awa::matrixgame;
static const SSkin* First(const wchar_t*,std::uint32_t){throw std::logic_error("fixture callback must not run");}
static const SSkin* Second(const wchar_t*,std::uint32_t){throw std::logic_error("second fixture callback must not run");}
template<class F> static void Refused(F f){bool failed=false;try{f();}catch(const std::logic_error&){failed=true;}assert(failed);}
int main(){
    int a{},b{},c{};
    assert(!BorrowedSkinLeaseCount());
    Refused([&]{TrackBorrowedSkinOwner(&a,First);});
    Refused([&]{BorrowedSkinScope invalid(nullptr);});
    for(int cycle=0;cycle<2;++cycle){
        {
            BorrowedSkinScope outer(First);
            TrackBorrowedSkinOwner(&a,First);TrackBorrowedSkinOwner(&b,First);
            CheckBorrowedSkinOwner(&a);CheckBorrowedSkinOwner(&b);
            {
                BorrowedSkinScope inner(Second);TrackBorrowedSkinOwner(&c,Second);
                assert(BorrowedSkinProviderBound(First) && BorrowedSkinProviderBound(Second));
                InvalidateBorrowedSkins(First);InvalidateBorrowedSkins(First);
                Refused([&]{CheckBorrowedSkinOwner(&a);});Refused([&]{CheckBorrowedSkinOwner(&b);});
                CheckBorrowedSkinOwner(&c);
                ForgetBorrowedSkinOwner(&a);TrackBorrowedSkinOwner(&a,First);CheckBorrowedSkinOwner(&a);
                Refused([&]{CheckBorrowedSkinOwner(&b);}); // New epoch cannot revive old pointers.
            }
            Refused([&]{CheckBorrowedSkinOwner(&c);});CheckBorrowedSkinOwner(&a);
        }
        Refused([&]{CheckBorrowedSkinOwner(&a);});
        for(auto* key:{&a,&b,&c})ForgetBorrowedSkinOwner(key);
        ForgetBorrowedSkinOwner(&a);
        assert(!BorrowedSkinLeaseCount() && !BorrowedSkinProviderBound(First));
    }
    std::puts("borrowed manager scope, two peers, invalidation before free, nested callbacks, expired epoch rejection, retry and cleanup/reentry PASS; GI-cache and GPU NOT RUN");
}
