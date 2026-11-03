// SPDX-License-Identifier: GPL-2.0-or-later
#include "native_device_state.hpp"
#include "native_state_layout.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
namespace srhd_awa::matrixgame::platform3g {
namespace {
constexpr DeviceResult InvalidCall=static_cast<std::int32_t>(0x8876086cu);
constexpr DeviceResult DeviceLost=static_cast<std::int32_t>(0x88760868u);
constexpr std::array<unsigned,9> VertexRegisters{137,143,141,139,145,147,148,29,146};
constexpr std::array<unsigned,5> ExtraSamplerRegisters{4,9,11,12,13};
constexpr unsigned MaximumRenderId=[](){unsigned n=0;for(const auto& w:NativeRenderWords)n=std::max(n,w.id);return n;}();
struct RegisterLookup {std::uint8_t word{},raster{};bool vertex{};};
static_assert(NativeRenderWords.size()<255 && RasterRegisters.size()<255);
constexpr auto RenderLookup=[](){
    std::array<RegisterLookup,MaximumRenderId+1> table{};
    for(unsigned i=0;i<NativeRenderWords.size();++i){auto& entry=table[NativeRenderWords[i].id];if(entry.word)throw "duplicate native render metadata";entry.word=std::uint8_t(i+1);}
    for(unsigned i=0;i<RasterRegisters.size();++i){auto& entry=table[unsigned(RasterRegisters[i])];if(!entry.word)throw "missing native raster metadata";entry.raster=std::uint8_t(i+1);}
    for(auto id:VertexRegisters){if(!table[id].word)throw "missing native vertex metadata";table[id].vertex=true;}
    return table;
}();
const RegisterLookup& Lookup(std::uint32_t id){
    if(id>=RenderLookup.size() || !RenderLookup[id].word)throw std::out_of_range("unknown native render register");
    return RenderLookup[id];
}
const NativeStateWord& Word(std::uint32_t id){
    return NativeRenderWords[Lookup(id).word-1];
}
constexpr bool Is(std::string_view name,std::initializer_list<std::string_view> names){return std::find(names.begin(),names.end(),name)!=names.end();}
// Keep the original serialized-ID order of logical-word validation stable.
static_assert([](){for(unsigned i=1;i<NativeRenderWords.size();++i)if(NativeRenderWords[i-1].id>=NativeRenderWords[i].id)return false;return true;}());
enum class DrawRule {None,Zero,One,Fill,FloatZero,SeparateAlpha};
constexpr auto DrawRules=[](){
    std::array<DrawRule,NativeRenderWords.size()> rules{};
    for(unsigned i=0;i<rules.size();++i){const auto n=NativeRenderWords[i].name;
        if(Is(n,{"DITHERENABLE","VERTEXBLEND","CLIPPLANEENABLE","INDEXEDVERTEXBLENDENABLE","ENABLEADAPTIVETESSELLATION","SRGBWRITEENABLE"}) || n.starts_with("WRAP"))rules[i]=DrawRule::Zero;
        else if(n=="CLIPPING")rules[i]=DrawRule::One;
        else if(n=="FILLMODE")rules[i]=DrawRule::Fill;
        else if(Is(n,{"SLOPESCALEDEPTHBIAS","DEPTHBIAS"}))rules[i]=DrawRule::FloatZero;
        else if(n=="SEPARATEALPHABLENDENABLE")rules[i]=DrawRule::SeparateAlpha;
    }return rules;
}();
void LogicalValue(const NativeStateWord& w,std::uint32_t v){
    const auto n=w.name;
    if(Is(n,{"LASTPIXEL","DITHERENABLE","FOGENABLE","RANGEFOGENABLE","STENCILENABLE","CLIPPING","LOCALVIEWER","POINTSPRITEENABLE","POINTSCALEENABLE","MULTISAMPLEANTIALIAS","INDEXEDVERTEXBLENDENABLE","SCISSORTESTENABLE","ANTIALIASEDLINEENABLE","ENABLEADAPTIVETESSELLATION","TWOSIDEDSTENCILMODE","SRGBWRITEENABLE","SEPARATEALPHABLENDENABLE"}) && v>1)
        throw std::invalid_argument("native scalar BOOL");
    if(Is(n,{"FOGSTART","FOGEND","FOGDENSITY","POINTSIZE","POINTSIZE_MIN","POINTSIZE_MAX","POINTSCALE_A","POINTSCALE_B","POINTSCALE_C","TWEENFACTOR","SLOPESCALEDEPTHBIAS","DEPTHBIAS","MINTESSELLATIONLEVEL","MAXTESSELLATIONLEVEL","ADAPTIVETESS_X","ADAPTIVETESS_Y","ADAPTIVETESS_Z","ADAPTIVETESS_W"}) && !std::isfinite(std::bit_cast<float>(v)))
        throw std::invalid_argument("nonfinite native scalar float");
    if((n=="FILLMODE" && (v<1 || v>3)) || (Is(n,{"FOGTABLEMODE","FOGVERTEXMODE"}) && v>3))throw std::invalid_argument("native scalar enum");
    if((n.starts_with("STENCIL") || n.starts_with("CCW_STENCIL")) && Is(n,{"STENCILFAIL","STENCILZFAIL","STENCILPASS","STENCILFUNC","CCW_STENCILFAIL","CCW_STENCILZFAIL","CCW_STENCILPASS","CCW_STENCILFUNC"}) && (v<1 || v>8))throw std::invalid_argument("native stencil enum");
    if((n.starts_with("WRAP") || n.starts_with("COLORWRITEENABLE")) && v>15)throw std::invalid_argument("native scalar mask");
    if(Is(n,{"SRCBLENDALPHA","DESTBLENDALPHA"}) && (v<1 || v>17))throw std::invalid_argument("native alpha blend factor");
    if(n=="BLENDOPALPHA" && (v<1 || v>5))throw std::invalid_argument("native alpha blend operation");
}
}
NativeRenderState::NativeRenderState(){
    for(const auto& word:NativeRenderWords)registers.emplace(word.id,word.initial);
}
void NativeRenderState::Render(std::uint32_t id,std::uint32_t value){
    const auto& word=Word(id);LogicalValue(word,value);
    if(id==60)combiner.Factor(value);
    else if(vertex.Render(id,value)){}
    else if(id==22){if(value<1 || value>3)throw Unsupported("native cull mode");cull=value;}
    else if(id==9){if(value!=1 && value!=2)throw Unsupported("native shading mode outside FLAT/GOURAUD");shade=value;}
    else if(std::find(RasterRegisters.begin(),RasterRegisters.end(),static_cast<RasterRegister>(id))!=RasterRegisters.end())raster.Apply(static_cast<RasterRegister>(id),value);
    // Inactive parameters remain real logical state. ValidateDraw/ValidateClear
    // reject any requested operation whose GPU semantics are not implemented.
    registers.at(id)=value;
}
std::uint32_t NativeRenderState::Render(std::uint32_t id)const{
    const auto& lookup=Lookup(id);
    if(id==22)return cull;if(id==9)return shade;
    if(id==60){const auto& c=combiner.State().factor;auto byte=[](float v){return std::uint32_t(std::lround(v*255));};return (byte(c[3])<<24)|(byte(c[0])<<16)|(byte(c[1])<<8)|byte(c[2]);}
    if(lookup.vertex)return vertex.Render(id);
    if(lookup.raster)return raster.Words()[lookup.raster-1];
    return registers.at(id);
}
void NativeRenderState::Sampler(std::uint32_t slot,std::uint32_t id,std::uint32_t value){
    auto& state=samplers.at(slot);
    for(unsigned i=0;i<ExtraSamplerRegisters.size();++i)if(id==ExtraSamplerRegisters[i]){
        if(id==11 && value>1)throw std::invalid_argument("native sRGB sampler BOOL");
        sampler_extra.at(slot)[i]=value;return;
    }
    state.Apply(static_cast<SamplerRegister>(id),value);
}
std::uint32_t NativeRenderState::Sampler(std::uint32_t slot,std::uint32_t id)const{
    const auto words=samplers.at(slot).Words();for(unsigned i=0;i<SamplerRegisters.size();++i)if(unsigned(SamplerRegisters[i])==id)return words[i];
    for(unsigned i=0;i<ExtraSamplerRegisters.size();++i)if(id==ExtraSamplerRegisters[i])return sampler_extra.at(slot)[i];
    throw std::out_of_range("unknown native sampler register");
}
void NativeRenderState::ValidateClear()const{
    if(registers.at(174))throw Unsupported("native SCISSORTESTENABLE requires scissor provider");
}
materials::PixelFogUniforms NativeRenderState::PixelFog()const{
    return materials::PreparePixelFog(Render(28)!=0,Render(35),std::bit_cast<float>(Render(36)),std::bit_cast<float>(Render(37)),std::bit_cast<float>(Render(38)),Render(34),vertex.Transform(3));
}
void NativeRenderState::ValidateDraw(bool points,bool lines,std::uint32_t samples,std::uint32_t quality)const{
    (void)PrepareDraw(points,lines,samples,quality);
}
PreparedDrawState NativeRenderState::PrepareDraw(bool points,bool lines,std::uint32_t samples,std::uint32_t quality)const{
    // D3D9's enable and write-mask registers have no raster effect on a
    // single-sample target, but remain observable/restorable logical state.
    // A true multisample attachment still needs a separate pipeline/resolve.
    if(samples || quality)throw Unsupported("native multisample target/resolve pipeline unavailable");
    // One draw-local view of raw words. No allocation or cross-draw cache;
    // presence is separate so even inactive missing words still fail at read.
    static_assert(MaximumRenderId<256,"review draw-local register view stack bound");
    std::array<std::uint32_t,MaximumRenderId+1> raw_words;
    std::array<bool,MaximumRenderId+1> present{};
    for(const auto& [id,value]:registers){if(id>MaximumRenderId)break;raw_words[id]=value;present[id]=true;}
    const auto raster_words=raster.Words();
    auto raw=[&](std::uint32_t id){return present[id]?raw_words[id]:registers.at(id);};
    auto read=[&](std::uint32_t id){
        const auto& kind=RenderLookup[id];
        if(kind.vertex || id==9 || id==22 || id==60)return Render(id);
        if(kind.raster)return raster_words[kind.raster-1];
        return raw(id);
    };
    // Keep the original preflight order, including true std::map::at errors.
    if(raw(174))throw Unsupported("native SCISSORTESTENABLE requires scissor provider");
    PreparedDrawState draw;
    draw.fog=materials::PreparePixelFog(read(28)!=0,read(35),std::bit_cast<float>(read(36)),std::bit_cast<float>(read(37)),std::bit_cast<float>(read(38)),read(34),vertex.Transform(3));
    draw.stencil=ReadStencilState(read);
    auto require=[&](std::string_view name,bool ok){if(!ok)throw Unsupported("native draw requires "+std::string(name));};
    // Read EVERY logical word in the original order, including inactive words.
    for(unsigned i=0;i<NativeRenderWords.size();++i){const auto& word=NativeRenderWords[i];const auto n=word.name;const auto v=read(word.id);
        switch(DrawRules[i]){
        case DrawRule::None:break;
        case DrawRule::Zero:require(n,v==0);break;
        case DrawRule::One:require(n,v==1);break;
        case DrawRule::Fill:require(n,v==2 || v==3);break;
        case DrawRule::FloatZero:require(n,std::bit_cast<float>(v)==0);break;
        case DrawRule::SeparateAlpha:if(raster.blend_enable)require(n,v==0);break;
        }
    }
    require("point rasterization",!points);
    if(lines || Render(8)==2){
        // Original minimap/selection use solid one-pixel aliased lines with
        // LASTPIXEL=TRUE. The SDK has no independent last-pixel toggle; do not
        // claim the unrepresented FALSE or antialiased modes are supported.
        require("LASTPIXEL=TRUE native lines",Render(16)==1);
        require("aliased native lines",Render(176)==0);
    }
    draw.combiner=combiner.Prepare();
    for(unsigned slot=0;slot<8;++slot)if(draw.combiner.stages[slot].alpha[3]&1u){
        for(unsigned i=1;i<ExtraSamplerRegisters.size();++i)
            if(sampler_extra[slot][i])throw Unsupported("native sampled texture requires "+std::string(NativeSamplerWords[ExtraSamplerRegisters[i]-1].name));
    }
    return draw;
}
MeshStencilState NativeRenderState::Stencil()const{return ReadStencilState([&](unsigned id){return Render(id);});}
struct NativeStateStore::Control {
    std::shared_ptr<NativeRenderState> state=std::make_shared<NativeRenderState>();
    std::size_t leases{};
    bool alive{true},frame{};
};
struct NativeStateStore::Lease final:DeviceStateLease {
    std::shared_ptr<Control> owner;
    std::shared_ptr<NativeRenderState> saved;
    buffers::BindingSnapshot bindings;
    explicit Lease(std::shared_ptr<Control> c,buffers::BindingSnapshot b)
        :owner(std::move(c)),saved(owner->state),bindings(std::move(b)){
        if(owner->leases==std::numeric_limits<std::size_t>::max())throw std::overflow_error("native state lease count");
        ++owner->leases;
    }
    ~Lease(){--owner->leases;}
    DeviceResult Restore()noexcept override {
        if(!owner->alive)return DeviceLost;
        if(owner->frame || !bindings.CanRestore())return InvalidCall;
        // No heap allocation, uploads, setters, or partial per-register Apply.
        // Deko Record emits this complete authoritative state on the next draw.
        if(!bindings.Restore())return InvalidCall;
        owner->state=saved;return DeviceSuccess;
    }
};
NativeStateStore::NativeStateStore():control_(std::make_shared<Control>()){}
NativeStateStore::~NativeStateStore(){Close();}
const NativeRenderState& NativeStateStore::Read()const {if(!control_->alive)throw Unsupported("native state device lifetime ended");return *control_->state;}
NativeRenderState& NativeStateStore::Edit(){
    if(!control_->alive)throw Unsupported("native state device lifetime ended");
    // Snapshots are immutable. All device setters enter through this COW gate.
    if(control_->state.use_count()!=1)control_->state=std::make_shared<NativeRenderState>(*control_->state);
    return *control_->state;
}
std::unique_ptr<DeviceStateLease> NativeStateStore::Capture(){
    if(!control_->alive)throw Unsupported("native state device lifetime ended");
    if(control_->frame)throw Unsupported("native state capture requires a quiescent frame");
    auto* context=buffers::BoundContext();
    auto bindings=context?context->CaptureBindings():buffers::BindingSnapshot{};
    return std::make_unique<Lease>(control_,std::move(bindings));
}
std::size_t NativeStateStore::Outstanding()const noexcept{return control_->leases;}
void NativeStateStore::AddDeviceTextureReferences(native_texture::DeviceReferences& references)const noexcept{
    if(!control_->alive || !control_->state){references.Deny();return;}
    // A snapshot sharing this same state is an external logical owner even
    // though its texture fields have not yet been copied by the COW gate.
    if(control_->state.use_count()==1)for(const auto& field:control_->state->textures)references.Add(field);
}
void NativeStateStore::ResetPrepared(std::shared_ptr<NativeRenderState> value,bool clear_bindings)noexcept{
    if(!control_->alive || control_->frame || !value)std::terminate();
    control_->state=std::move(value);
    if(clear_bindings)if(auto* context=buffers::BoundContext())context->ClearBindings();
}
void NativeStateStore::Frame(bool value)noexcept{control_->frame=value;}
void NativeStateStore::Close()noexcept{control_->alive=false;control_->state.reset();}
}
