// Original MSVC/UCRT and native RNG execute the same mixed stream contract.
#include "random.hpp"
#include "CException.hpp"
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <vector>
#ifndef MATRIXGAME_RANDOM_NO_THREADS
#include <thread>
#endif
#ifdef _MSC_VER
#include <crtdbg.h>
#endif
#ifdef MATRIXGAME_ORIGINAL_RANDOM_REFERENCE
namespace game_rng=random;
#else
namespace game_rng=matrixgame_random;
#endif
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK %s:%d: %s\n",__FILE__,__LINE__,#e);std::exit(3);}}while(0)
FILE* output;
size_t values=0;
template<class T>void emit(T value){CHECK(std::fwrite(&value,sizeof(value),1,output)==1);++values;}
void integer(int value){emit(std::bit_cast<uint32_t>(int32_t(value)));}
void real64(double value){emit(std::bit_cast<uint64_t>(value));}
void real32(float value){emit(std::bit_cast<uint32_t>(value));}
int run(int argc,char**argv){
    static_assert(std::endian::native==std::endian::little && sizeof(int)==4 && sizeof(float)==4 && sizeof(double)==8);
    CHECK(argc==2);auto folder=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(folder);
    output=std::fopen((folder/"random-trace.bin").string().c_str(),"wb");CHECK(output);
    for(unsigned i=0;i<32;++i)integer(game_rng::Rnd());
    std::vector<uint32_t> seeds;for(unsigned i=0;i<256;++i)seeds.push_back(i);
    for(auto seed:std::array<uint32_t,4>{0x7fffffff,0x80000000,0xffffffff,0x12345678})seeds.push_back(seed);
    for(auto seed:seeds){
        game_rng::seed(seed);for(unsigned i=0;i<1024;++i)integer(game_rng::Rnd());
        game_rng::seed(seed^0xa5a5a5a5u);
        for(int i=0;i<128;++i){
            integer(game_rng::Rnd(-1000+i,777-i));integer(game_rng::Rnd(77,77));integer(game_rng::Rnd(900,-321));
            real64(game_rng::RndFloat());real64(game_rng::RndFloat(-5.25,7.75));real64(RND(-1234,5678));
            real32(FRND(1000-i));real32(FSRND(i-64));integer(IRND(i*19+1));integer(IRND(-37));
        }
    }
    unsigned threads=0;
#ifndef MATRIXGAME_RANDOM_NO_THREADS
    threads=4;std::array<std::vector<int>,4> streams;std::array<std::thread,4> workers;
    for(unsigned index=0;index<workers.size();++index)workers[index]=std::thread([&,index]{
        streams[index].push_back(game_rng::Rnd());game_rng::seed(71+index);
        for(unsigned i=0;i<1024;++i)streams[index].push_back(game_rng::Rnd());
    });
    for(auto& worker:workers)worker.join();
    for(const auto& stream:streams)for(auto value:stream)integer(value);
#endif
    integer(game_rng::Rnd()); // Creating other threads must not change this stream.
#ifndef MATRIXGAME_ORIGINAL_RANDOM_REFERENCE
    game_rng::seed(1);CHECK(game_rng::Rnd(std::numeric_limits<int>::min(),std::numeric_limits<int>::max())==std::numeric_limits<int>::min()+41);
    game_rng::seed(1);CHECK(game_rng::Rnd(std::numeric_limits<int>::max(),std::numeric_limits<int>::min())==std::numeric_limits<int>::min()+41);
    game_rng::seed(1);std::srand(77);(void)std::rand();CHECK(game_rng::Rnd()==41);
    bool rejected=false;
    try{(void)FSRND(std::numeric_limits<int>::max());}catch(const Base::CException&){rejected=true;}CHECK(rejected);
#endif
    CHECK(std::fclose(output)==0);
    std::printf("{\"seeds\":%zu,\"values\":%zu,\"threads\":%u}\n",seeds.size(),values,threads);
    return 0;
}
int main(int argc,char**argv){
#ifdef _MSC_VER
    _set_error_mode(_OUT_TO_STDERR);_set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
    try{return run(argc,argv);}catch(const std::exception& error){std::fprintf(stderr,"RNG: %s\n",error.what());return 2;}
    catch(const Base::CException& error){auto message=error.Info();std::fprintf(stderr,"RNG: %s\n",std::string(message.begin(),message.end()).c_str());return 2;}
}
