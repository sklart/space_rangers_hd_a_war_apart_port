// Same Math3D CPU cases execute against original x86/D3DX9 and native DirectXMath.
#include "Math3D.hpp"
#include "CStorage.hpp"
#include "CFile.hpp"
#include "utils.hpp"
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif
#ifndef MATRIXGAME_ORIGINAL_MATH_REFERENCE
#include "pick_viewport.hpp"
int g_ScreenX=1280,g_ScreenY=720;
namespace srhd_awa::matrixgame {
PickViewport ReadPickViewport() noexcept {return {g_ScreenX,g_ScreenY};}
}
#else
extern int g_ScreenX,g_ScreenY;
#endif
#define CHECK(e) do { if(!(e)){std::fprintf(stderr,"CHECK %s:%d: %s\n",__FILE__,__LINE__,#e);std::exit(3);} } while(0)
using namespace Base;
namespace fs=std::filesystem;
#if defined(MATRIXGAME_ORIGINAL_MATH_REFERENCE)
const char* backend="D3DX9-x86";
#elif defined(_XM_ARM_NEON_INTRINSICS_)
const char* backend="DirectXMath-NEON";
#elif defined(_XM_SSE_INTRINSICS_)
const char* backend="DirectXMath-SSE";
#else
const char* backend="DirectXMath-scalar";
#endif
FILE* trace;
size_t floats=0,integers=0;
void f(float value){CHECK(std::isfinite(value));std::fprintf(trace,"F %.9g\n",double(value));++floats;}
void u(uint32_t value){std::fprintf(trace,"I %u\n",value);++integers;}
void v(const D3DXVECTOR3& value){f(value.x);f(value.y);f(value.z);}
void matrix(const D3DXMATRIX& value){for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)f(value.m[r][c]);}
bool CloseMath(float a,float b,float epsilon=0.00003f){return std::abs(a-b)<=epsilon*std::max(1.0f,std::max(std::abs(a),std::abs(b)));}
void point(const D3DXVECTOR3& a,const D3DXVECTOR3& b){CHECK(CloseMath(a.x,b.x)&&CloseMath(a.y,b.y)&&CloseMath(a.z,b.z));}
uint32_t state=0x529d1357;
float next(){state=state*1664525+1013904223;return float(int((state>>16)%641)-320)/16.0f;}
D3DXVECTOR3 sample(){return {next(),next(),next()};}
D3DXMATRIX identity(){D3DXMATRIX out;std::memset(&out,0,sizeof(out));out._11=out._22=out._33=out._44=1;return out;}
void trajectories(){
    const D3DXVECTOR3 points[]={{0,0,0},{1,2,0},{2,3,1},{4,5,2},{6,3,1},{8,2,0},{9,1,-1},{10,0,0}};
    const D3DXVECTOR3 more[]={{12,1,0},{14,2,1},{16,0,0}};
    for(unsigned mode=1;mode<=2;++mode){
        CTrajectory path(nullptr);
        if(mode==1)path.Init1(points,8);else path.Init2(points,8);
        for(unsigned i=0;i<=64;++i){D3DXVECTOR3 p;path.CalcPoint(p,float(i)/64);v(p);}
        f(path.CalcLength());
        for(unsigned i=0;i<128;++i){path.Move(0.25f);D3DXVECTOR3 p;path.GetCurPos(p);v(p);u(path.NeedContinue());}
        if(mode==1)path.Continue1(more,3);else path.Continue2(more,3);
        for(unsigned i=0;i<=64;++i){D3DXVECTOR3 p;path.CalcPoint(p,float(i)/64);v(p);}
        f(path.CalcLength());
#ifndef MATRIXGAME_ORIGINAL_MATH_REFERENCE
        const float before=path.CalcLength();
        path.Init2(points,4);const auto after=path.CalcLength();CHECK(!CloseMath(before,after));
        D3DXVECTOR3 end;path.CalcPoint(end,1);
        CHECK(path.CutBefore(2)==1);D3DXVECTOR3 cut;path.CalcPoint(cut,1);point(cut,end);
        CTrajectory fresh(nullptr);fresh.Init2(points,4);D3DXVECTOR3 original;
        fresh.CalcPoint(original,0.5f);const auto cut_t=fresh.CutBefore(0.5f);
        fresh.CalcPoint(cut,cut_t/2.0f);point(cut,original);
#endif
    }
}
void controlled(){
    g_ScreenX=1280;g_ScreenY=720;
    for(unsigned i=0;i<512;++i){
        const auto a=sample(),b=sample();D3DXVECTOR3 cross,normal;
        D3DXVec3Cross(&cross,&a,&b);D3DXVec3Normalize(&normal,&a);
        f(D3DXVec3Dot(&a,&b));f(D3DXVec3LengthSq(&a));f(D3DXVec3Length(&a));v(cross);v(normal);
        D3DXVECTOR3 alias=a;D3DXVec3Normalize(&alias,&alias);point(alias,normal);
        D3DXMATRIX frame,rotation;
        VecToMatrixX(frame,b,normal);matrix(frame);VecToMatrixY(frame,b,normal);matrix(frame);
        BuildRotateMatrix(rotation,b,normal,float(i%91)/64.0f);matrix(rotation);
        D3DXMATRIX inverse;float determinant=0;CHECK(D3DXMatrixInverse(&inverse,&determinant,&rotation));matrix(inverse);f(determinant);
        for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c){double sum=0;for(unsigned k=0;k<4;++k)sum+=double(rotation.m[r][k])*inverse.m[k][c];CHECK(std::abs(sum-double(r==c))<0.0001);}
        auto projection=identity();projection._11=1.25f;projection._22=1.75f;
        D3DXVECTOR3 origin,direction;CalcPick(CPoint(int(i*17%1280),int(i*11%720)),projection,rotation,&origin,&direction);v(origin);v(direction);CHECK(CloseMath(D3DXVec3Length(&direction),1));
        f(DistOtrezokPoint(a,b,{0,0,0}));f(DistLinePoint(a,b,{0,0,0}));v(Vec3Projection(a,b));
        f(TableSin(float(i)/32));f(TableCos(float(i)/32));
        float input=float(i+1)/37,reciprocal;FP_INV(reciprocal,input);f(reciprocal);
        f(float(AngleNorm(double(i)/13-20)));f(float(AngleDist(double(i)/13-20,2.1)));
        u(LIC(0xffaabbcc,0x00112233,float(i)/511));
        u(FP_NORM_TO_BYTE2(float(i)/511));u(FP_NORM_TO_BYTE3(float(i)/511));
        float neg=float(i)/17;NEG_FLOAT(neg);u(std::bit_cast<uint32_t>(neg));MAKE_ABS_FLOAT(neg);u(std::bit_cast<uint32_t>(neg));
        double dn=-double(i)/17;MAKE_ABS_DOUBLE(dn);f(float(dn));
    }
    const D3DXVECTOR3 zero{0,0,0};D3DXVECTOR3 result;D3DXVec3Normalize(&result,&zero);point(result,zero);
    for(uint32_t signs=0;signs<8;++signs){D3DXVECTOR3 z{std::bit_cast<float>((signs&1)<<31),std::bit_cast<float>(((signs>>1)&1)<<31),std::bit_cast<float>(((signs>>2)&1)<<31)};u(IS_ZERO_VECTOR(z));}
    D3DXVECTOR3 a{-1,-1,0},b{0,1,0},c{1,-1,0};float t=-1,x=-1,y=-1;
    CHECK(IntersectTriangle({0,0,-4},{0,0,1},a,b,c,t,x,y));CHECK(CloseMath(t,4)&&CloseMath(x,0.5f)&&CloseMath(y,0.25f));f(t);f(x);f(y);
    CHECK(!IntersectTriangle({0,0,-4},{0,0,1},a,c,b,t,x,y));
    for(unsigned i=1;i<128;++i){CHECK(IsIntersectSphere(D3DXVECTOR3{0,0,0},float(i),D3DXVECTOR3{-float(i)-2,0,0},D3DXVECTOR3{1,0,0},t));CHECK(CloseMath(t,2));f(t);}
    D3DXMATRIX singular;std::memset(&singular,0,sizeof(singular));auto unchanged=identity();CHECK(!D3DXMatrixInverse(&unchanged,nullptr,&singular));CHECK(unchanged._11==1);
    D3DXPLANE plane;D3DXPlaneFromPoints(&plane,&a,&b,&c);f(plane.a);f(plane.b);f(plane.c);f(plane.d);
#ifndef MATRIXGAME_ORIGINAL_MATH_REFERENCE
    SPlane native;SPlane::BuildFromPointNormal(native,{0,0,3},{0,0,1});CHECK(CloseMath(native.Distance({1,2,3}),0)&&CloseMath(native.dist,-3));
    SPlane::BuildFromPoints(native,a,b,c);CHECK(CloseMath(native.norm.z,-1)&&CloseMath(native.Distance({0,0,0}),0));
    bool rejected=false;try{CTrajectory invalid(nullptr);invalid.CalcLength();}catch(const CException&){rejected=true;}CHECK(rejected);
    rejected=false;try{CTrajectory invalid(nullptr);invalid.Init1(&a,1);}catch(const CException&){rejected=true;}CHECK(rejected);
#endif
    D3DXVECTOR2 crossing;CHECK(IntersectLine({0,0},{4,4},{0,4},{4,0},crossing));CHECK(CloseMath(crossing.x,2)&&CloseMath(crossing.y,2));
    CHECK(IntersectLine({0,0},{4,4},{0,4},{4,0},&t,&x));CHECK(CloseMath(t,0.5f)&&CloseMath(x,0.5f));f(t);f(x);
    CHECK(IsIntersectSphere(D3DXVECTOR2{0,0},1,D3DXVECTOR2{-2,0},D3DXVECTOR2{2,0},t));CHECK(CloseMath(t,1));f(t);
    trajectories();
}
uint32_t PayloadHash(const uint8_t* bytes,size_t size){uint32_t hash=2166136261u;for(size_t i=0;i<size;++i)hash=(hash^bytes[i])*16777619u;return hash;}
void real(const char* package,const char* entry){
    CFile::StaticInit();const auto filename=utils::to_wstring(fs::absolute(package).string());CFile::AddPackFile(filename.c_str());CFile::OpenPackFiles();
    size_t hits=0,vertices_count=0;uint32_t vertices_hash=0,indices_hash=0;
    {
        CStorage storage;CHECK(storage.Load(utils::to_wstring(entry).c_str()));
        auto* vertices=storage.GetBuf(L"verts",L"data",ST_BYTE);auto* indices=storage.GetBuf(L"idxs",L"data",ST_BYTE);CHECK(vertices&&indices);
        auto* raw=vertices->GetFirst<uint8_t>(0);vertices_count=vertices->GetArrayLength(0)/32;
        CHECK(vertices->GetArrayLength(0)%32==0);std::vector<D3DXVECTOR3> positions(vertices_count);
        for(size_t i=0;i<vertices_count;++i)std::memcpy(&positions[i],raw+i*32,12);
        auto* triangles=indices->GetFirst<uint8_t>(0);const auto count=indices->GetArrayLength(0)/6;
        vertices_hash=PayloadHash(raw,vertices->GetArrayLength(0));indices_hash=PayloadHash(triangles,indices->GetArrayLength(0));
        for(size_t i=0;i<count && hits<128;++i){uint16_t ids[3];std::memcpy(ids,triangles+i*6,6);for(auto id:ids)CHECK(id<positions.size());
            const auto a=positions[ids[0]],b=positions[ids[1]],c=positions[ids[2]];auto e1=b-a,e2=c-a;D3DXVECTOR3 normal;D3DXVec3Cross(&normal,&e1,&e2);
            if(D3DXVec3LengthSq(&normal)<0.01f)continue;D3DXVec3Normalize(&normal,&normal);
            auto center=(a+b+c)/3.0f;auto origin=center+normal*32.0f;auto direction=-normal;float t,u,vv;
            CHECK(IntersectTriangle(origin,direction,a,b,c,t,u,vv));CHECK(CloseMath(t,32,0.0003f)&&CloseMath(u,1.0f/3,0.0003f)&&CloseMath(vv,1.0f/3,0.0003f));
            f(t);f(u);f(vv);++hits;
        }
    }
    CFile::ReleasePackFiles();CHECK(hits>0);
    std::printf("{\"real_vertices\":%zu,\"real_triangle_hits\":%zu,\"vertices_fnv32\":\"%08x\",\"indices_fnv32\":\"%08x\"}\n",vertices_count,hits,vertices_hash,indices_hash);
}
int main(int argc,char**argv){
#ifdef _MSC_VER
    _set_error_mode(_OUT_TO_STDERR);_set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
    try{CHECK(argc==2||argc==4);fs::path out=fs::absolute(argv[1]);fs::create_directories(out);trace=std::fopen((out/"math-trace.txt").string().c_str(),"w");CHECK(trace);controlled();if(argc==4)real(argv[2],argv[3]);CHECK(std::fclose(trace)==0);std::printf("{\"backend\":\"%s\",\"float_values\":%zu,\"integer_values\":%zu}\n",backend,floats,integers);return 0;}
    catch(const CException& error){auto msg=error.Info();std::fprintf(stderr,"Math3D: %s\n",std::string(msg.begin(),msg.end()).c_str());return 2;}
    catch(const std::exception& error){std::fprintf(stderr,"Math3D: %s\n",error.what());return 2;}
}
