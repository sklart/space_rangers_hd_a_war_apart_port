// SPDX-License-Identifier: GPL-2.0-or-later
#include "material_combiner.hpp"
#include <algorithm>
#include <cmath>
namespace srhd_awa::matrixgame::materials {
namespace {
void Operation(std::uint32_t v){
    switch(v){case 1:case 2:case 3:case 4:case 5:case 6:case 7:case 8:case 9:case 10:case 11:
    case 12:case 13:case 14:case 15:case 16:return;
    default:throw Incomplete("texture operation "+std::to_string(v)+" has no native shader equation");}
}
void Argument(std::uint32_t v){if((v&~0x3fu) || (v&15u)>5 || (v&15u)==4)throw Incomplete("unsupported texture argument "+std::to_string(v));}
bool Uses(std::uint32_t op,std::uint32_t a,std::uint32_t b){
    if(op==1)return false;
    return (op!=3 && (a&15u)==2) || (op!=2 && (b&15u)==2) || op==13 || op==15;
}
bool TextureArgument(std::uint32_t op,std::uint32_t a,std::uint32_t b){
    return op!=1 && ((op!=3 && (a&15u)==2) || (op!=2 && (b&15u)==2));
}
bool Reads(std::uint32_t op,std::uint32_t a,std::uint32_t b,std::uint32_t source){
    return op!=1 && ((op!=3 && (a&15u)==source) || (op!=2 && (b&15u)==source));
}
Color Arg(std::uint32_t arg,Color diffuse,Color current,Color texture,Color factor,Color temp){
    Argument(arg);Color out{};
    switch(arg&15u){case 0:out=diffuse;break;case 1:out=current;break;case 2:out=texture;break;case 3:out=factor;break;case 5:out=temp;break;}
    if(arg&32u)out.fill(out[3]);
    if(arg&16u)for(auto& x:out)x=1-x;
    return out;
}
float Equation(std::uint32_t op,float a,float b,float da,float ta,float fa,float ca){
    switch(op){
    case 2:return a;case 3:return b;case 4:return a*b;case 5:return 2*a*b;case 6:return 4*a*b;
    case 7:return a+b;case 8:return a+b-.5f;case 9:return 2*(a+b-.5f);case 10:return a-b;
    case 11:return a+b-a*b;case 12:return a*da+b*(1-da);case 13:return a*ta+b*(1-ta);
    case 14:return a*fa+b*(1-fa);case 15:return a+b*(1-ta);case 16:return a*ca+b*(1-ca);
    default:throw Incomplete("undefined/unsupported shader operation");}
}
}
Combiner::Combiner(){state_.stages[0].color[0]=4;state_.stages[0].alpha[0]=2;}
void Combiner::Set(std::uint32_t stage,std::uint32_t reg,std::uint32_t value){
    auto& s=state_.stages.at(stage);
    switch(reg){
    case 1:Operation(value);s.color[0]=value;break;
    case 2:Argument(value);s.color[1]=value;break;
    case 3:Argument(value);s.color[2]=value;break;
    case 4:Operation(value);s.alpha[0]=value;break;
    case 5:Argument(value);s.alpha[1]=value;break;
    case 6:Argument(value);s.alpha[2]=value;break;
    case 28:if(value!=1 && value!=5)throw Incomplete("unsupported stage result register");s.color[3]=value;break;
    default:throw Incomplete("register is not a texture combiner register");}
}
std::uint32_t Combiner::Get(std::uint32_t stage,std::uint32_t reg)const{
    const auto& s=state_.stages.at(stage);
    switch(reg){case 1:return s.color[0];case 2:return s.color[1];case 3:return s.color[2];case 4:return s.alpha[0];case 5:return s.alpha[1];case 6:return s.alpha[2];case 28:return s.color[3];default:throw Incomplete("register is not a texture combiner register");}
}
void Combiner::Bind(std::uint32_t stage,bool present){bound_.at(stage)=present;}
void Combiner::Factor(std::uint32_t value)noexcept{state_.factor={float((value>>16)&255)/255,float((value>>8)&255)/255,float(value&255)/255,float(value>>24)/255};}
Uniforms Combiner::Prepare()const{
    auto prepared=state_;
    std::size_t count=0;
    bool diffuse_color=false;
    for(std::size_t i=0;i<8;++i){const auto& s=state_.stages[i];
        if(s.color[0]==1 || (!bound_[i] && TextureArgument(s.color[0],s.color[1],s.color[2])))break;
        const bool sample=Uses(s.color[0],s.color[1],s.color[2]) || Uses(s.alpha[0],s.alpha[1],s.alpha[2]);
        prepared.stages[i].alpha[3]=sample?(bound_[i]?1:2):0;
        diffuse_color|=Reads(s.color[0],s.color[1],s.color[2],0) ||
            (i==0 && Reads(s.color[0],s.color[1],s.color[2],1));
        ++count;
    }
    for(std::size_t i=count;i<8;++i)prepared.stages[i].color[0]=1;
    if(count)prepared.stages[count-1].color[3]=1;
    // Original D3D9 gloss passes omit vertex diffuse in RGB. With alpha
    // disabled at the first stage their initial alpha is opaque, unlike a
    // cascade that actually consumes vertex diffuse. Reference-tested below.
    const auto& first=state_.stages[0];
    if(count && first.alpha[0]==1 && !diffuse_color)
        prepared.stages[0].alpha[3]|=4;
    return prepared;
}
Color Evaluate(const Uniforms& u,Color diffuse,const std::array<Color,8>& textures){
    Color current=diffuse,temp{},output=diffuse;
    if(u.stages[0].alpha[3]&4)current[3]=1;
    for(std::size_t i=0;i<8;++i){const auto& s=u.stages[i];if(s.color[0]==1)break;
        Operation(s.color[0]);Operation(s.alpha[0]);
        const Color texel=(s.alpha[3]&2)?Color{1,1,1,1}:textures[i];
        const auto ca=Arg(s.color[1],diffuse,current,texel,u.factor,temp),cb=Arg(s.color[2],diffuse,current,texel,u.factor,temp);
        const auto aa=Arg(s.alpha[1],diffuse,current,texel,u.factor,temp),ab=Arg(s.alpha[2],diffuse,current,texel,u.factor,temp);
        Color next{};
        for(unsigned c=0;c<4;++c){
            next[c]=(c==3 && s.alpha[0]==1)?(s.color[3]==5?temp[3]:current[3]):Equation(c==3?s.alpha[0]:s.color[0],c==3?aa[c]:ca[c],c==3?ab[c]:cb[c],diffuse[3],texel[3],u.factor[3],current[3]);
            if(!std::isfinite(next[c]))throw std::invalid_argument("nonfinite material input");
            next[c]=std::clamp(next[c],0.f,1.f);
        }
        if(s.color[3]==5)temp=next;
        else if(s.color[3]==1)current=next;
        else throw Incomplete("unsupported stage result");
        output=next; // Final stage output, including a final TEMP destination.
    }
    return output;
}
}
