// Executes original typed texture/union owners with a declared CPU-only upload model.
#include <cassert>
#include <cstdio>
#include "MatrixMapTexture.hpp"
#include "native_texture.hpp"
#include "bitmap_array_owner.hpp"
namespace nt=srhd_awa::matrixgame::native_texture;
namespace p=srhd_awa::matrixgame::platform3g;
namespace lt=srhd_awa::matrixgame::legacy_texture;
struct TextureRoutingDevice final:p::Device,nt::Provider{
    std::shared_ptr<nt::Session> session;
    explicit TextureRoutingDevice(std::shared_ptr<nt::Session> s):session(std::move(s)){}
    std::shared_ptr<nt::Session> TextureSession()override{return session;}
    p::DeviceCaps ReadCaps()override{throw p::Unsupported("typed CPU texture fixture has no GPU caps");}
    std::uint64_t AvailableTextureBytes()override{throw p::Unsupported("typed CPU texture fixture has no device budget");}
    p::GammaRamp ReadGamma(std::uint32_t)override{throw p::Unsupported("typed CPU texture fixture has no gamma");}
    void RestoreGamma(std::uint32_t,const p::GammaRamp&)noexcept override{std::terminate();}
};
void VerifyOriginalBC1Owner(const std::shared_ptr<nt::Session>& session,bool& fail){
    for(unsigned stop=0;stop<5;++stop){CBitmap* slots[4]{};
        try{srhd_awa::matrixgame::BitmapArrayOwner owner(slots,nullptr);
            for(unsigned i=0;i<4;++i){if(i==stop)throw std::bad_alloc();slots[i]=HNew(nullptr) CBitmap();slots[i]->CreateRGBA(64,64);}}
        catch(const std::bad_alloc&){}
        for(auto* value:slots)assert(!value);
    }
    auto* baseline=p::Current().device;const bool was_ready=p::Current().ready;TextureRoutingDevice device(session);p::Current().ready=true;p::Current().device=&device;
    struct Restore {p::Device* saved;bool ready;~Restore(){p::Current().device=saved;p::Current().ready=ready;}} restore{baseline,was_ready};
    const auto registry=lt::RegistrySize();
    CBitmap bitmap;bitmap.CreateRGB(1024,1024);bitmap.Fill(Base::CPoint(0,0),bitmap.Size(),0xff425168);
    {
        CBottomTextureUnion original;original.MakeFromBitmap(bitmap);auto* first=original.GetTexture();assert(first && first->GetSizeX()==1024 && first->GetSizeY()==1024);
        auto retained=nt::Find(first->Tex());assert(retained->format==0x31545844 && retained->levels.size()==6);
        auto packet=nt::Resident(retained);fail=true;bool failed=false;
        try{original.MakeFromBitmap(bitmap);}catch(const std::exception&){failed=true;}fail=false;
        assert(failed && original.GetTexture()==first && lt::RegistrySize()==registry+1);
        original.MakeFromBitmap(bitmap);assert(original.GetTexture()!=first && retained->image==packet);
        retained.reset();packet.reset();assert(lt::RegistrySize()==registry+1);
    }
    assert(lt::RegistrySize()==registry && nt::HandleCount()==0);
    std::puts("original CBottomTextureUnion -> CTextureManaged -> native BC1: create/replacement/OOM/retained-packet/destruction PASS CPU_MODEL_ONLY");
}
