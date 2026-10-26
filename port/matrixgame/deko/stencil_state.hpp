// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cstdint>
#include <stdexcept>
namespace srhd_awa::matrixgame {
// Original serialized D3D9 comparison/operation IDs. No SDK object aliases.
inline constexpr std::uint32_t NativeStencilCaps=0x1ff; // Eight ops + two-sided.
struct StencilFace {
    std::uint32_t fail{1},depth_fail{1},pass{1},compare{8};
    bool operator==(const StencilFace&)const=default;
};
struct MeshStencilState {
    bool enabled{},two_sided{};
    StencilFace clockwise{},counterclockwise{};
    std::uint32_t reference{},read_mask{~0u},write_mask{~0u};
    const StencilFace& Face(bool cw)const noexcept{return cw || !two_sided?clockwise:counterclockwise;}
    std::uint8_t Reference8()const noexcept{return std::uint8_t(reference);}
    void Validate()const{
        for(auto f:{clockwise,counterclockwise})for(auto v:{f.fail,f.depth_fail,f.pass,f.compare})
            if(v<1 || v>8)throw std::invalid_argument("invalid native stencil operation/comparison");
    }
    bool operator==(const MeshStencilState&)const=default;
};
template<class Read>MeshStencilState ReadStencilState(Read&& read){
    MeshStencilState s;s.enabled=read(52)!=0;s.two_sided=read(185)!=0;
    s.clockwise={read(53),read(54),read(55),read(56)};
    s.counterclockwise={read(186),read(187),read(188),read(189)};
    s.reference=read(57);s.read_mask=read(58);s.write_mask=read(59);s.Validate();return s;
}
inline void RequireStencilAttachment(const MeshStencilState& state,bool depth_available,std::uint32_t format){
    if(state.enabled && (!depth_available || format!=75))throw std::invalid_argument("native stencil requires a D24S8 depth attachment");
}
// CPU diagnostic oracle only. Production rendering uses the real depth/stencil
// pipeline. Values are later compared against actual HAL stencil readback.
inline bool StencilCompare(std::uint32_t op,std::uint8_t ref,std::uint8_t value){
    switch(op){case 1:return false;case 2:return ref<value;case 3:return ref==value;case 4:return ref<=value;case 5:return ref>value;case 6:return ref!=value;case 7:return ref>=value;case 8:return true;default:throw std::invalid_argument("native stencil comparison");}
}
inline std::uint8_t StencilOperation(std::uint32_t op,std::uint8_t value,std::uint8_t ref){
    switch(op){case 1:return value;case 2:return 0;case 3:return ref;case 4:return value==255?255:value+1;case 5:return value?value-1:0;case 6:return std::uint8_t(~value);case 7:return std::uint8_t(value+1);case 8:return std::uint8_t(value-1);default:throw std::invalid_argument("native stencil operation");}
}
struct StencilResult {bool color_pass;std::uint8_t value;};
inline StencilResult EvaluateStencil(const MeshStencilState& s,bool clockwise,bool depth_pass,std::uint8_t old){
    s.Validate();if(!s.enabled)return {depth_pass,old};const auto& face=s.Face(clockwise);
    const bool pass=StencilCompare(face.compare,s.Reference8()&s.read_mask,old&s.read_mask);
    const auto v=StencilOperation(!pass?face.fail:!depth_pass?face.depth_fail:face.pass,old,s.Reference8());
    return {pass && depth_pass,std::uint8_t((old&~s.write_mask)|(v&s.write_mask))};
}
}
