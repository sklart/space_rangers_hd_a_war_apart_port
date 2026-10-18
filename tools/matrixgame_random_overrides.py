"""Preserve the original Windows CRT stream without libc/namespace dependence."""
INPUTS = {
 'random.cpp':'9c5ef914ad896f214be468babd49304548613743b7c533c200fb64cbf0b1cdfd',
 'random.hpp':'b169672fd41650a3d8d090e34a31a64aa5fd7607bb22406f496808fb704901a8',
}


def patch_random(files, body):
    files['random.hpp']=files['random.hpp'].replace(b'namespace random',b'namespace matrixgame_random')
    c=files['random.cpp'].decode().replace('#include <cmath>','''#include "random.hpp"
#include "CException.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>''')
    c=c.replace('random::','matrixgame_random::')
    c=c.replace('namespace random\n{','''namespace matrixgame_random
{
static_assert(sizeof(unsigned int)==4);
// Match the observed UCRT per-thread contract; no host libc state is modified.
static thread_local uint32_t rng_state=1;
''')
    c=body(c,'void seed(unsigned int val)', '''void seed(unsigned int val) { rng_state=val; }''')
    c=body(c,'int Rnd()', '''int Rnd() {
    rng_state=rng_state*214013u+2531011u;
    return int((rng_state>>16)&32767u);
}''')
    c=body(c,'int Rnd(int zmin, int zmax)', '''int Rnd(int zmin, int zmax) {
    const int64_t low=std::min(zmin,zmax),high=std::max(zmin,zmax);
    const uint64_t span=uint64_t(high-low)+1;
    return int(low+int64_t(uint64_t(Rnd())%span));
}''')
    c=c.replace('(1.0 / (RAND_MAX))','(1.0 / 32767.0)')
    c=c.replace('fabs(double((to) - (from)))','std::fabs(double(to)-double(from))')
    c=body(c,'float FSRND(int x)', '''float FSRND(int x) {
    const float scaled=2.0f*float(x);
    if (double(scaled)<double(std::numeric_limits<int>::min()) ||
        double(scaled)>double(std::numeric_limits<int>::max())) ERROR_S(L"FSRND argument exceeds int conversion");
    return FRND(int(scaled))-float(x);
}''')
    c=body(c,'int IRND(int n)', '''int IRND(int n) {
    const double value=std::round(RND(0,int(double(n)-0.55)));
    if (value<double(std::numeric_limits<int>::min()) || value>double(std::numeric_limits<int>::max()))
        ERROR_S(L"IRND result exceeds int conversion");
    return int(value);
}''')
    files['random.cpp']=c.encode()
