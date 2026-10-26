// Production native font CPU lifecycle/layout against original Windows HAL pixels.
#include "native_debug_font.hpp"
#include "MatrixDebugInfo.hpp"
#include <cassert>
#include <fstream>
#include <cmath>
#include <cstdio>
namespace f=srhd_awa::matrixgame::debug_font::native;
namespace p=srhd_awa::matrixgame::platform3g;
std::vector<std::byte> Read(const char* path){std::ifstream in(path,std::ios::binary|std::ios::ate);assert(in);const auto n=in.tellg();assert(n>=0);std::vector<std::byte> bytes(std::size_t(n),std::byte{});in.seekg(0);in.read(reinterpret_cast<char*>(bytes.data()),bytes.size());assert(in);return bytes;}
template<class F>void Fails(F operation){bool failed=false;try{operation();}catch(const std::exception&){failed=true;}assert(failed);}
struct Image final:f::Atlas {inline static unsigned alive{};Image(){++alive;}~Image(){--alive;}};
struct Device final:p::Device,f::Provider {
    std::shared_ptr<f::Session> session;
    p::DeviceCaps ReadCaps()override{throw p::Unsupported("CPU fixture is not a GPU");}
    p::GammaRamp ReadGamma(std::uint32_t)override{throw p::Unsupported("CPU fixture is not a display");}
    void RestoreGamma(std::uint32_t,const p::GammaRamp&)noexcept override{std::terminate();}
    std::uint64_t AvailableTextureBytes()override{throw p::Unsupported("CPU fixture is not a texture arena");}
    std::shared_ptr<f::Session> DebugFontSession()override{return session;}
};
int main(int argc,char** argv){assert(argc==3);auto bytes=Read(argv[1]);auto data=f::Data::Load(bytes);
    assert(data->line_height==25 && data->ascent==20 && data->descent==5 && data->charset==204 && data->glyphs.size()==222);
    assert(data->width==512 && data->height==448 && data->map['A']!=0xffff && data->map[0x416]!=0xffff);
    auto bad=bytes;bad[0]=std::byte{};Fails([&]{f::Data::Load(bad);});
    for(auto length:{std::size_t(8),std::size_t(44),bytes.size()-1})Fails([&]{f::Data::Load(std::span{bytes}.first(length));});
    bad=bytes;bad[44+16]=std::byte{};bad[44+17]=std::byte{};bad[44+18]=std::byte{};bad[44+19]=std::byte{};Fails([&]{f::Data::Load(bad);});
    const auto map_offset=44+24*data->glyphs.size();bad=bytes;bad[map_offset]=std::byte{254};bad[map_offset+1]=std::byte{127};Fails([&]{f::Data::Load(bad);});
    bad=bytes;for(std::size_t n=map_offset+131072+3;n<bad.size();n+=4)bad[n]=std::byte{};Fails([&]{f::Data::Load(bad);});
    Fails([&]{f::Place(*data,L"\u4e2d",1,{0,0,10,19},0x124,~0u);});
    Fails([&]{f::Place(*data,L"a",-2,{},0x124,~0u);});Fails([&]{f::Place(*data,L"a",1,{},0x2000,~0u);});
    auto goldens=Read(argv[2]);std::size_t offset=0;auto word=[&](){assert(offset+4<=goldens.size());unsigned value{};for(unsigned n=0;n<4;++n)value|=unsigned(goldens[offset++])<<(8*n);return value;};
    unsigned cases=0;
    while(offset<goldens.size()){
        const auto count=word();std::wstring text;for(unsigned n=0;n<count;++n)text.push_back(wchar_t(word()));
        const auto flags=word(),height=word();const int left=int(word()),top=int(word()),right=int(word()),bottom=int(word());
        auto layout=f::Place(*data,text.c_str(),int(text.size()),{10,10,250,29},flags,~0u);
        assert(layout.result==int(height) && layout.bounds.left==left && layout.bounds.top==top && layout.bounds.right==right && layout.bounds.bottom==bottom);
        std::vector<std::byte> actual(512*64);
        for(const auto& quad:layout.quads){const int x=int(quad.left),y=int(quad.top),width=int(quad.right-quad.left),height=int(quad.bottom-quad.top);
            const unsigned u=unsigned(std::lround(quad.u0*data->width)),v=unsigned(std::lround(quad.v0*data->height));
            for(int row=0;row<height;++row)for(int column=0;column<width;++column){if(x+column<0 || x+column>=512 || y+row<0 || y+row>=64)continue;
                const auto alpha=data->rgba[(std::size_t(v+row)*data->width+u+column)*4+3];assert(alpha==std::byte{} || alpha==std::byte{255});
                if(alpha!=std::byte{})actual[std::size_t(y+row)*512+x+column]=alpha;}}
        assert(offset+actual.size()<=goldens.size());assert(std::equal(actual.begin(),actual.end(),goldens.begin()+offset));offset+=actual.size();++cases;
    }
    assert(cases==699);unsigned uploads=0,draws=0;bool refuse=false;
    auto session=std::make_shared<f::Session>();session->data=data;
    session->upload=[&](const f::Data& input)->std::shared_ptr<f::Atlas>{assert(input.rgba==data->rgba);if(refuse)throw std::bad_alloc();++uploads;return std::make_shared<Image>();};
    session->draw=[&](const auto& atlas,std::span<const f::Quad> quads){assert(atlas && !quads.empty());++draws;};
    refuse=true;Fails([&]{f::Font::Create(session);});assert(!Image::alive);refuse=false;
    auto font=f::Font::Create(session);assert(Image::alive==1 && uploads==1);srhd_awa::matrixgame::debug_font::TextRect rect{10,10,250,29};
    assert(font->Draw(L"MatrixGame",-1,rect,0x124,0x80ffffff)==22 && draws==1);
    font->Lost();font->Lost();assert(!Image::alive);Fails([&]{font->Draw(L"A",1,rect,0x124,~0u);});
    refuse=true;Fails([&]{font->Reset();});assert(font->lost && !Image::alive);refuse=false;font->Reset();assert(!font->lost && Image::alive==1);
    auto replacement=std::make_shared<f::Session>();replacement->data=data;replacement->upload=session->upload;replacement->draw=session->draw;
    auto wrong=std::make_shared<f::Session>(*replacement);auto wrong_data=std::make_shared<f::Data>(*data);++wrong_data->average;wrong->data=wrong_data;
    Fails([&]{font->Rebind(wrong);});assert(Image::alive==1 && font->session==session);
    font->Rebind(replacement);assert(Image::alive==1);session->Close();assert(Image::alive==1);font->Draw(L"A",1,rect,0x124,~0u);
    replacement->Close();assert(!Image::alive);Fails([&]{font->Reset();});Fails([&]{font->Draw(L"A",1,rect,0x124,~0u);});font.reset();
    Device device;device.session=std::make_shared<f::Session>();device.session->data=data;device.session->upload=[&](const auto&){return std::make_shared<Image>();};device.session->draw=[&](const auto&,auto){++draws;};
    ID3DXFont* token{};f::CreateFor(device,&token);assert(token && Image::alive==1);Fails([&]{f::CreateFor(device,&token);});f::Find(token).Lost();f::ResetFor(device,token);assert(Image::alive==1);
    Device new_device;new_device.session=std::make_shared<f::Session>(*device.session);
    f::ResetFor(new_device,token);device.session->Close();assert(Image::alive==1);
    auto dead=token;f::Release(token);assert(!Image::alive);Fails([&]{f::Find(dead);});f::Release(nullptr);
    // CPU device fixture supplies only the font protocol. No fake Init/caps/window.
    auto& state=p::Current();state.device=&new_device;state.ready=true;
    {CMatrixDebugInfo original;original.T(L"native",L"original owner",20,0);original.Takt(1);assert(Image::alive==1);original.OnLostDevice();assert(!Image::alive);original.OnResetDevice();assert(Image::alive==1);}
    assert(!Image::alive);state.ready=false;state.device=nullptr;new_device.session->Close();
    std::printf("native debug font CPU PASS glyphs=222 original_HAL_goldens=%u original_CMatrixDebugInfo_lifecycle=executed create_failure=checked lost_reset_rebind=checked expired_owner=checked empty_atlas=rejected GPU_UPLOAD_MODEL_ONLY SWITCH_HARDWARE_NOT_RUN\n",cases);
}
