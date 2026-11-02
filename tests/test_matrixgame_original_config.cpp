// SPDX-License-Identifier: GPL-2.0-or-later
// Full original config TU, real input/parser/heaps. Gamma-only CPU routing;
// there is no graphics provider, map, native Init, or synthesized game config.
#include "MatrixConfig.hpp"
#include "MatrixGame.h"
#include "StringConstants.hpp"
#include "native_gamma.hpp"
#include <cassert>
#include <codecvt>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <locale>
#include <vector>

Base::CHeap* g_MatrixHeap;
Base::CHeap* g_CacheHeap;
Base::CBlockPar* g_MatrixData;
int g_MaxFPS;
namespace p=srhd_awa::matrixgame::platform3g;
struct GammaOnly final:p::Device {
    p::NativeGammaStore gamma;unsigned calls{};
    p::DeviceCaps ReadCaps()override{throw p::Unsupported("config fixture has no GPU caps");}
    p::GammaRamp ReadGamma(unsigned index)override{return gamma.Read(index);}
    void RestoreGamma(unsigned index,const p::GammaRamp& value)noexcept override{gamma.Restore(index,value);}
    void ApplyCalibratedGamma(unsigned index,const p::GammaRamp& value)override{gamma.Apply(index,value);++calls;}
    std::uint64_t AvailableTextureBytes()override{throw p::Unsupported("config fixture has no GPU allocator");}
};
struct RefusingGamma final:p::Device {
    p::DeviceCaps ReadCaps()override{throw p::Unsupported("baseline fixture has no GPU caps");}
    p::GammaRamp ReadGamma(unsigned)override{throw p::Unsupported("baseline fixture has no gamma");}
    void RestoreGamma(unsigned,const p::GammaRamp&)noexcept override{std::terminate();}
    std::uint64_t AvailableTextureBytes()override{throw p::Unsupported("baseline fixture has no GPU allocator");}
};
template<class T>void Append(std::vector<char>& output,const T& value){
    static_assert(std::is_trivially_copyable_v<T>);const auto* bytes=reinterpret_cast<const char*>(&value);output.insert(output.end(),bytes,bytes+sizeof(value));
}
void Append(std::vector<char>& output,const std::wstring& value){
    std::string utf8;
    if constexpr(sizeof(wchar_t)==2)utf8=std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>{}.to_bytes(value);
    else utf8=std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.to_bytes(value);
    Append(output,std::uint32_t(utf8.size()));output.insert(output.end(),utf8.begin(),utf8.end());
}
std::vector<char> Observe(){
    std::vector<char> output;
    Append(output,g_Config.m_CursorsCnt);
    for(int i=0;i<g_Config.m_CursorsCnt;++i){Append(output,g_Config.m_Cursors[i].key);Append(output,g_Config.m_Cursors[i].val);}
    for(const auto* table:{g_Config.m_CannonDamages,g_Config.m_BuildingDamages,g_Config.m_RobotDamages,g_Config.m_ObjectDamages,g_Config.m_FlyerDamages})
        for(int i=0;i<WEAPON_COUNT;++i){Append(output,table[i].damage);Append(output,table[i].mindamage);Append(output,table[i].friend_damage);}
    for(const auto& table:{g_Config.m_WeaponStrengthAI,g_Config.m_WeaponRadius})for(int i=0;i<WEAPON_COUNT;++i)Append(output,table[i]);
    Append(output,g_Config.m_WeaponCooldown);Append(output,g_Config.m_BuildingHitPoints);
    Append(output,g_Config.m_KeyActions);Append(output,g_Config.m_Timings);Append(output,g_Config.m_Price);Append(output,g_Config.m_Overheat);Append(output,g_Config.m_ItemChars);
    for(int i=0;i<LABELS_LAST;++i)Append(output,g_Config.m_Labels[i]);
    for(int i=0;i<DESCRIPTIONS_LAST;++i)Append(output,g_Config.m_Descriptions[i]);
    Append(output,g_Config.m_CamBaseAngleZ);Append(output,g_Config.m_CamMoveSpeed);Append(output,g_Config.m_CamInRobotForward0);Append(output,g_Config.m_CamInRobotForward1);
    return output;
}
int main(int argc,char** argv){
    assert(argc==3);std::vector<char> expected;unsigned completed=0;
    for(unsigned session=0;session<2;++session){
        Base::CHeap game_heap,cache_heap;g_MatrixHeap=&game_heap;g_CacheHeap=&cache_heap;
        Base::CBlockPar data;data.LoadFromTextFile(std::filesystem::path(argv[1]).wstring().c_str());g_MatrixData=&data;
        g_Config.SetDefaults();RefusingGamma baseline;auto& binding=p::Current();binding.device=&baseline;binding.ready=true;
        bool refused=false;try{g_Config.ReadParams();}catch(const p::Unsupported& error){refused=std::string(error.what())=="backend calibrated gamma submission unsupported";}
        assert(refused && g_Config.m_CursorsCnt==5 && !g_Config.m_Labels && !g_Config.m_Descriptions);
        GammaOnly device;binding.device=&device;
        for(unsigned retry=0;retry<2;++retry){
            g_Config.ReadParams();assert(device.calls==retry+1);
            assert(g_Config.m_CursorsCnt==data.BlockGet(PAR_SOURCE_CURSORS)->ParCount());
            assert(g_Config.m_Labels && g_Config.m_Descriptions);
            auto* labels=data.BlockGet(IF_LABELS_BLOCKPAR);
            assert(g_Config.m_Labels[W1_CHAR]==labels->BlockGet(PAR_SOURCE_ITEMS_LABELS)->ParGet(PAR_SOURCE_W1_CHAR));
            assert(g_Config.m_Descriptions[CH5_DESCR]==labels->BlockGet(PAR_SOURCE_ITEMS_DESCRIPTIONS)->ParGet(PAR_SOURCE_CH5_DESCR));
            assert(g_Config.m_Price[WEAPON1_TITAN]==data.BlockGet(PAR_SOURCE_PRICE)->BlockGet(PAR_SOURCE_PRICE_WEAPON)->ParGet(PAR_SOURCE_PRICE_WEAPON1_TITAN).GetInt());
            const auto snapshot=Observe();if(expected.empty())expected=snapshot;else assert(snapshot==expected);
            ++completed;
        }
        g_Config.Clear();g_Config.Clear();assert(!g_Config.m_Cursors && !g_Config.m_CursorsCnt && !g_Config.m_Labels && !g_Config.m_Descriptions);
        binding.device=nullptr;binding.ready=false;g_MatrixData=nullptr;g_MatrixHeap=g_CacheHeap=nullptr;
    }
    std::ofstream output(argv[2],std::ios::binary);output.write(expected.data(),expected.size());assert(output.good());
    std::printf("ORIGINAL FULL ReadParams returned=%u; two gamma-refusal baseline/retry sessions; numeric tables + %u labels + %u descriptions; repeated Clear PASS; CPU_GAMMA_ONLY NO_NATIVE_INIT NO_MAP NO_GPU\n",completed,unsigned(LABELS_LAST),unsigned(DESCRIPTIONS_LAST));
}
