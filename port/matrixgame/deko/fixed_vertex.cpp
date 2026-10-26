// SPDX-License-Identifier: GPL-2.0-or-later
#include "fixed_vertex.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace srhd_awa::matrixgame::materials {
namespace {
Color TransformVector(Color v,const Matrix& m){Color out{};for(unsigned c=0;c<4;++c)for(unsigned r=0;r<4;++r)out[c]+=v[r]*m[r*4+c];return out;}
void Finite(float v){if(!std::isfinite(v))throw std::invalid_argument("nonfinite fixed vertex value");}
float Dot(Color a,Color b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
Color Unit(Color v){const auto n=std::sqrt(Dot(v,v));if(n>0)for(unsigned i=0;i<3;++i)v[i]/=n;return v;}
Color ColorValue(platform3g::LightColor c){return {c.r,c.g,c.b,c.a};}
Color Argb(std::uint32_t c){return {float((c>>16)&255)/255,float((c>>8)&255)/255,float(c&255)/255,float(c>>24)/255};}
Matrix NormalMatrix(const Matrix& m){
    const float a=m[0],b=m[1],c=m[2],d=m[4],e=m[5],f=m[6],g=m[8],h=m[9],i=m[10];
    const float det=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);
    if(!std::isfinite(det) || std::abs(det)<1e-12f)throw std::invalid_argument("singular fixed-function normal transform");
    Matrix n{};n[0]=(e*i-f*h)/det;n[1]=(f*g-d*i)/det;n[2]=(d*h-e*g)/det;
    n[4]=(c*h-b*i)/det;n[5]=(a*i-c*g)/det;n[6]=(b*g-a*h)/det;
    n[8]=(b*f-c*e)/det;n[9]=(c*d-a*f)/det;n[10]=(a*e-b*d)/det;n[15]=1;return n;
}
}
Matrix Identity()noexcept{return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
Matrix Multiply(const Matrix& a,const Matrix& b){Matrix out{};for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)for(unsigned k=0;k<4;++k)out[r*4+c]+=a[r*4+k]*b[k*4+c];return out;}
FixedVertexState::FixedVertexState():world_(Identity()),view_(Identity()),projection_(Identity()){
    for(unsigned i=0;i<8;++i){values_.texture[i]=Identity();values_.coordinates[i][0]=i;}
    values_.controls={0,0,1,0};values_.viewport={0,0,1,1};
}
void FixedVertexState::Layout(std::uint32_t fvf){
    const auto position=fvf&0x00eu;
    if((position!=2 && position!=4) || (fvf&~0xffff0fd6u))throw Incomplete("unsupported original FVF position/flags");
    unsigned size=position==4?16:12;
    if(fvf&0x10)size+=12;if(fvf&0x40)size+=4;if(fvf&0x80)size+=4;
    unsigned count=(fvf>>8)&15;if(count>8)throw Incomplete("FVF texcoord count exceeds eight");
    std::array<unsigned,8> dims{};
    for(unsigned i=0;i<count;++i){const unsigned encoded=(fvf>>(16+2*i))&3;dims[i]=encoded==0?2:encoded==1?3:encoded==2?4:1;size+=4*dims[i];}
    if(position==4 && (fvf&0x10))throw Incomplete("transformed FVF cannot contain normals");
    fvf_=fvf;stride_=size;dimensions_=dims;
    values_.controls[3]=((fvf&0x40)?1u:0u)|((fvf&0x80)?2u:0u)|(position==4?4u:0u);
    for(unsigned i=0;i<8;++i)values_.coordinates[i][2]=dimensions_[i];
}
void FixedVertexState::Transform(std::uint32_t t,const Matrix& m){for(float x:m)Finite(x);if(t==2)view_=m;else if(t==3)projection_=m;else if(t==256)world_=m;else if(t>=16 && t<24)values_.texture[t-16]=m;else throw Incomplete("unknown native transform");}
Matrix FixedVertexState::Transform(std::uint32_t t)const{if(t==2)return view_;if(t==3)return projection_;if(t==256)return world_;if(t>=16 && t<24)return values_.texture[t-16];throw Incomplete("unknown native transform query");}
void FixedVertexState::Stage(std::uint32_t stage,std::uint32_t reg,std::uint32_t value){
    auto& s=values_.coordinates.at(stage);
    if(reg==11){const unsigned mode=value&0xffff0000u;if((value&0xffffu)>7 || (mode!=0 && mode!=0x10000 && mode!=0x20000 && mode!=0x30000))throw Incomplete("unsupported TCI selector");s[0]=value;}
    else if(reg==24){if(value!=0 && value!=2)throw Incomplete("unsupported original texture transform flags");s[1]=value;}
    else throw Incomplete("unknown coordinate register");
}
std::uint32_t FixedVertexState::Stage(std::uint32_t s,std::uint32_t r)const{if(r==11)return values_.coordinates.at(s)[0];if(r==24)return values_.coordinates.at(s)[1];throw Incomplete("unknown coordinate query");}
void FixedVertexState::Material(const platform3g::VideoMaterial& m){
    const auto d=ColorValue(m.Diffuse),a=ColorValue(m.Ambient),e=ColorValue(m.Emissive);for(auto c:{d,a,e})for(float x:c)Finite(x);
    for(float x:ColorValue(m.Specular))Finite(x);Finite(m.Power);material_=m;
    values_.material_diffuse=d;values_.material_ambient=a;values_.material_emissive=e;
}
void FixedVertexState::Light(std::uint32_t index,const platform3g::VideoLight& light){
    if(index>=8 || light.Type!=3)throw Incomplete("only original directional lights are implemented");
    for(float x:ColorValue(light.Diffuse))Finite(x);for(float x:ColorValue(light.Ambient))Finite(x);
    for(float x:{light.Direction.x,light.Direction.y,light.Direction.z})Finite(x);
    if(light.Direction.x==0 && light.Direction.y==0 && light.Direction.z==0)throw std::invalid_argument("zero light direction");lights_[index]=light;
}
void FixedVertexState::EnableLight(std::uint32_t i,bool value){if(i>=8 || (value && lights_[i].Type!=3))throw Incomplete("undefined directional light");enabled_[i]=value;}
bool FixedVertexState::Render(std::uint32_t r,std::uint32_t v){
    if(r==137 || r==143 || r==141){if(v>1)throw std::invalid_argument("fixed vertex boolean");values_.controls[r==137?0:r==143?1:2]=v;return true;}
    if(r==139){values_.global_ambient=Argb(v);return true;}
    if(r==145 || r==147 || r==148){if(v>2)throw Incomplete("unknown material color source");values_.sources[r==145?0:r==147?1:2]=v;return true;}
    if(r==29 && v)throw Incomplete("native specular vertex lighting not implemented");
    if(r==146 && v!=0)throw Incomplete("specular material source requires specular lighting");
    return r==29 || r==146;
}
std::uint32_t FixedVertexState::Render(std::uint32_t r)const{
    if(r==137)return values_.controls[0];if(r==143)return values_.controls[1];if(r==141)return values_.controls[2];
    if(r==139){const auto& c=values_.global_ambient;auto byte=[](float v){return std::uint32_t(std::lround(v*255));};return (byte(c[3])<<24)|(byte(c[0])<<16)|(byte(c[1])<<8)|byte(c[2]);}
    if(r==145)return values_.sources[0];if(r==147)return values_.sources[1];if(r==148)return values_.sources[2];
    if(r==29 || r==146)return 0;
    throw Incomplete("unknown fixed vertex render query");
}
void FixedVertexState::Viewport(const platform3g::VideoViewport& v){if(!v.Width || !v.Height || v.X>16384 || v.Y>16384 || v.Width>16384 || v.Height>16384 || v.MinZ!=0 || v.MaxZ!=1)throw Incomplete("native viewport extent/depth range");values_.viewport={float(v.X),float(v.Y),float(v.Width),float(v.Height)};}
platform3g::VideoViewport FixedVertexState::Viewport()const noexcept{return {unsigned(values_.viewport[0]),unsigned(values_.viewport[1]),unsigned(values_.viewport[2]),unsigned(values_.viewport[3]),0,1};}
std::vector<Vertex> FixedVertexState::Decode(std::span<const std::byte> bytes,std::uint32_t stride)const{
    if(stride<stride_ || bytes.size()%stride)throw std::invalid_argument("FVF staging stride/range");
    std::vector<Vertex> out(bytes.size()/stride);
    for(std::size_t i=0;i<out.size();++i){auto* p=bytes.data()+i*stride;auto& v=out[i];
        auto floats=[&](float* to,unsigned count){std::memcpy(to,p,count*4);p+=count*4;for(unsigned j=0;j<count;++j)Finite(to[j]);};
        floats(v.position.data(),(fvf_&0xe)==4?4:3);if(fvf_&0x10)floats(v.normal.data(),3);
        for(auto pair:{std::pair{0x40u,&v.diffuse},std::pair{0x80u,&v.specular}})if(fvf_&pair.first){std::uint32_t c;std::memcpy(&c,p,4);p+=4;*pair.second=Argb(c);}
        for(unsigned j=0;j<8;++j){v.uv[j]={0,0,0,1};if(dimensions_[j])floats(v.uv[j].data(),dimensions_[j]);}
    }
    return out;
}
VertexUniforms FixedVertexState::Prepare()const{
    auto out=values_;out.camera=Multiply(world_,view_);out.clip=Multiply(out.camera,projection_);
    bool needs_normal=out.controls[0] && !(out.controls[3]&4);
    for(const auto& coordinate:out.coordinates){const auto mode=coordinate[0]&0xffff0000u;needs_normal|=mode==0x10000 || mode==0x30000;}
    out.normal=needs_normal?NormalMatrix(out.camera):Identity();
    for(unsigned i=0;i<8;++i)if(enabled_[i]){const auto& l=lights_[i];out.lights[i].direction=Unit(TransformVector({l.Direction.x,l.Direction.y,l.Direction.z,0},view_));out.lights[i].direction[3]=1;out.lights[i].diffuse=ColorValue(l.Diffuse);out.lights[i].ambient=ColorValue(l.Ambient);}
    return out;
}
VertexResult EvaluateVertex(const VertexUniforms& u,const Vertex& v){
    VertexResult out;auto camera=TransformVector({v.position[0],v.position[1],v.position[2],1},u.camera);
    auto n=TransformVector({v.normal[0],v.normal[1],v.normal[2],0},u.normal);if(u.controls[1])n=Unit(n);
    out.clip=TransformVector({v.position[0],v.position[1],v.position[2],1},u.clip);
    if(u.controls[3]&4){if(v.position[3]<=0)throw std::invalid_argument("invalid RHW");const auto w=1/v.position[3];out.clip={(2*(v.position[0]-u.viewport[0])/u.viewport[2]-1)*w,(1-2*(v.position[1]-u.viewport[1])/u.viewport[3])*w,v.position[2]*w,w};}
    out.diffuse=v.diffuse;
    if(u.controls[0] && !(u.controls[3]&4)){
        auto source=[&](unsigned s,Color material){if(u.controls[2] && s==1 && (u.controls[3]&1))return v.diffuse;if(u.controls[2] && s==2 && (u.controls[3]&2))return v.specular;return material;};
        auto d=source(u.sources[0],u.material_diffuse),a=source(u.sources[1],u.material_ambient),e=source(u.sources[2],u.material_emissive);
        out.diffuse=e;out.diffuse[3]=d[3];for(unsigned c=0;c<3;++c)out.diffuse[c]+=a[c]*u.global_ambient[c];
        for(const auto& l:u.lights)if(l.direction[3]){const float lambert=std::max(-Dot(n,l.direction),0.f);for(unsigned c=0;c<3;++c)out.diffuse[c]+=d[c]*l.diffuse[c]*lambert+a[c]*l.ambient[c];}
        for(float& x:out.diffuse)x=std::clamp(x,0.f,1.f);
    }
    for(unsigned i=0;i<8;++i){const auto& s=u.coordinates[i];const auto mode=s[0]&0xffff0000u,index=s[0]&0xffffu;
        Color t=v.uv[index];if(mode==0x10000)t={n[0],n[1],n[2],1};else if(mode==0x20000)t=camera;
        else if(mode==0x30000){auto eye=Unit({-camera[0],-camera[1],-camera[2],0});const auto dot=Dot(n,eye);for(unsigned c=0;c<3;++c)t[c]=2*n[c]*dot-eye[c];t[3]=1;}
        if(s[1]){if(!mode && u.coordinates[index][2]<=2)t={t[0],t[1],1,0};t=TransformVector(t,u.texture[i]);}
        out.uv[i]={t[0],t[1]};
    }
    return out;
}
}
