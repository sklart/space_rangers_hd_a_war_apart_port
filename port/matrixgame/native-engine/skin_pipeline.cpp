// SPDX-License-Identifier: GPL-2.0-or-later
#include "skin_pipeline.hpp"
namespace srhd_awa::matrixgame {
namespace {
struct BoundSkin:SSkin {std::shared_ptr<const NativeSkin> resource;};
struct Recorder final:NativeDrawSink {
    explicit Recorder(const MeshDraw& value):draw(value){}
    MeshDraw draw;
    SkinDrawBatch batch;
    const SSkin* active{};
    CachedFrameMaterial material{};
    bool layout{},texture{};
    void SetVertexLayout(std::uint32_t value) override {
        if(value!=buffers::VoVertexLayout)throw std::invalid_argument("unsupported skin vertex layout");
        layout=true;
    }
    void Emit(const MeshRange& range) override {
        if(!layout || !active || !texture)throw std::logic_error("incomplete native skin draw state");
        batch.submissions.push_back({range,material.state.draw,material.state.texture_slot,material.state.sampling,material.state.raster});
    }
};
Recorder& CurrentRecorder(){return dynamic_cast<Recorder&>(CurrentNativeDrawSink());}
const BoundSkin& Bound(const SSkin* skin) {
    // This callback is installed only on BoundSkin, which owns the SSkin base.
    const auto& bound=*static_cast<const BoundSkin*>(skin);
    if(!bound.resource || !bound.resource->Valid())throw std::logic_error("bound skin resource expired");
    return bound;
}
void Preload(const SSkin* skin) {
    Bound(skin).resource->Preload();
    if(auto* recorder=dynamic_cast<Recorder*>(InspectNativeDrawSink()))++recorder->batch.preload;
}
bool Stages(const SSkin* skin,uintptr_t user,int pass) {
    if(user || pass!=0)throw std::logic_error("UNSUPPORTED: burn/object or additional skin pass");
    auto& recorder=CurrentRecorder();
    if(recorder.active)throw std::logic_error("skin pass not cleared");
    const auto& resource=*Bound(skin).resource;
    auto material=NativeSinglePass(resource,1,recorder.draw);
    const auto image=material.texture.Read();
    std::uint32_t slot{};
    for(const auto& existing:recorder.batch.textures)if(existing.asset==image){slot=existing.slot;break;}
    if(!slot) {
        slot=std::uint32_t(recorder.batch.textures.size()+1);
        if(slot>=64)throw std::length_error("native skin texture slots");
        recorder.batch.textures.push_back({slot,image});
    }
    material.state.texture_slot=slot;recorder.material=std::move(material);
    recorder.active=skin;recorder.texture=false;++recorder.batch.stages;
    return false; // The supported ordinal/base branch has exactly one pass.
}
void Texture(const SSkin* skin,uintptr_t user,int pass) {
    (void)Bound(skin);auto& recorder=CurrentRecorder();
    if(user || pass || recorder.active!=skin)throw std::logic_error("skin texture callback order");
    recorder.texture=true;++recorder.batch.set_texture;
}
void Clear(const SSkin* skin,uintptr_t user) {
    (void)Bound(skin);auto& recorder=CurrentRecorder();
    if(user || recorder.active!=skin)throw std::logic_error("skin clear callback order");
    recorder.active=nullptr;recorder.texture=false;++recorder.batch.clear;
}
void Shadow(const SSkin*) {throw std::logic_error("UNSUPPORTED: native projected skin shadow");}
}
std::shared_ptr<const SSkin> NativeSkinPipeline::AcquireSkin(const wchar_t* name,std::uint32_t group) {
    if(!runtime_.CpuReady())throw std::logic_error("skin pipeline cache not active");
    auto resource=skins_.Acquire(name,group);
    const auto found=callbacks_.find(resource.get());
    if(found!=callbacks_.end())if(auto existing=found->second.lock())return existing;
    auto skin=std::make_shared<BoundSkin>();
    skin->m_Preload=Preload;skin->m_SetupStages=Stages;skin->m_SetupTex=Texture;
    skin->m_SetupClear=Clear;skin->m_SetupTexShadow=Shadow;skin->resource=std::move(resource);
    callbacks_[skin->resource.get()]=skin;return skin;
}
void NativeSkinPipeline::Load(CVectorObject& object,EObjectLoad flags,std::uint32_t group) {
    if(!runtime_.CpuReady() || object.m_Cache!=g_Cache)throw std::logic_error("skin VO owner mismatch");
    SkinLoadScope provider(*this);
    object.LoadSpecial(flags,AcquireOwnedSkin,group);
}
SkinDrawBatch NativeSkinPipeline::Prepare(CVectorObject& object,int frame,const MeshDraw& draw,
    std::shared_ptr<const SSkin> default_skin) {
    if(!runtime_.CpuReady() || object.m_Cache!=g_Cache || !object.IsLoaded())
        throw std::logic_error("skinned VO not loaded in this cache");
    if(frame<0 || frame>=object.GetFramesCnt())throw std::out_of_range("skinned VO frame");
    Recorder recorder(draw);NativeDrawScope scope(recorder);
    object.BeforeDraw();
    CVectorObject::DrawBegin();object.Draw(frame,0,default_skin.get());CVectorObject::DrawEnd();
    if(recorder.active)throw std::logic_error("uncleared skin draw state");
    std::vector<MeshRange> ranges;for(const auto& s:recorder.batch.submissions)ranges.push_back(s.range);
    (void)buffers::ValidateBoundVO(buffers::Current(),ranges);
    return std::move(recorder.batch);
}
}
