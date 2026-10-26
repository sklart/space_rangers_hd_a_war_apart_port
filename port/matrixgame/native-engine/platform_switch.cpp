// SPDX-License-Identifier: GPL-2.0-or-later
#include "platform_switch.hpp"
#include "../deko/legacy_raster_viewport.hpp"
#include "../deko/stencil_deko.hpp"
#include <switch.h>
#include <deko3d.hpp>
#include <limits>
#include <map>
#include <cstring>
#include "../deko/material_program.hpp"
#include "../deko/texture_store.hpp"
#include "../deko/alpha_target.hpp"
#include "material_shaders.hpp"
#include "staging_draw.hpp"
#include "native_device_state.hpp"
#include "native_presentation.hpp"
#include "native_reset_resources.hpp"
#include "native_draw_batch.hpp"
#include "native_command_memory.hpp"
#include "applet_window_lifecycle.hpp"
#include "applet_engine_events.hpp"
#include "native_input_switch.hpp"
#include <cstdio>
namespace srhd_awa::matrixgame::platform3g {
namespace {
struct NativeAttachment final:WindowAttachment {
    applet_window::Lifecycle& lifecycle;native_input::SwitchInput& input;native_cursor::Store& cursor;
    native_cursor::Store::State saved;bool live{true};
    NativeAttachment(applet_window::Lifecycle& value,native_input::SwitchInput& controls,native_cursor::Store& store)
        :lifecycle(value),input(controls),cursor(store),saved(store.Read()){
        input.Attach();try{lifecycle.Attach();}catch(...){input.Detach();throw;}
    }
    ~NativeAttachment(){Restore();}
    void Restore()noexcept override{if(live){const auto result=lifecycle.Detach();input.Detach();
        try{cursor.Restore(saved);}catch(...){std::terminate();}live=false;
        if(result)std::fprintf(stderr,"applet visibility restore failed: 0x%08x\n",unsigned(result));}}
};
void CheckNx(Result result,const char* call){if(R_FAILED(result))throw DeviceError(call,static_cast<DeviceResult>(result));}
std::uint32_t AllocationSize(std::uint64_t bytes){
    if(!bytes || bytes>std::numeric_limits<std::uint32_t>::max()-(DK_MEMBLOCK_ALIGNMENT-1))throw std::length_error("deko allocation extent");
    return static_cast<std::uint32_t>((bytes+DK_MEMBLOCK_ALIGNMENT-1)&~std::uint64_t(DK_MEMBLOCK_ALIGNMENT-1));
}
}
// libnx applet notification hooks run synchronously on the message-pump thread.
// All provider operations, including any owner's pump, must use that thread.
class NxAppletBackend final:public applet_window::Backend {
    AppletHookCookie cookie_{};
    inline static NxAppletBackend* owner_{};
    std::array<applet_window::Event,256> events_{};
    unsigned first_{},count_{};bool overflow_{},exit_{};
    static applet_window::Focus Focus(){
        switch(appletGetFocusState()){
        case AppletFocusState_InFocus:return applet_window::Focus::Foreground;
        case AppletFocusState_OutOfFocus:return applet_window::Focus::Unfocused;
        case AppletFocusState_Background:return applet_window::Focus::Background;
        default:throw std::runtime_error("invalid applet focus state");
        }
    }
    static void Hook(AppletHookType type,void* param)noexcept{
        auto& self=*static_cast<NxAppletBackend*>(param);
        if(type==AppletHookType_OnExitRequest)self.exit_=true;
        if(type!=AppletHookType_OnFocusState && type!=AppletHookType_OnExitRequest && type!=AppletHookType_OnResume)return;
        if(self.count_==self.events_.size()){self.overflow_=true;return;}
        const auto focus=appletGetFocusState();
        if(focus!=AppletFocusState_InFocus && focus!=AppletFocusState_OutOfFocus && focus!=AppletFocusState_Background){self.overflow_=true;return;}
        self.events_[(self.first_+self.count_)%self.events_.size()]={
            focus==AppletFocusState_InFocus?applet_window::Focus::Foreground:
            focus==AppletFocusState_OutOfFocus?applet_window::Focus::Unfocused:applet_window::Focus::Background,self.exit_};
        ++self.count_;
    }
public:
    void Start()override{
        if(owner_)throw std::logic_error("applet visibility already borrowed");
        if(!serviceIsActive(appletGetServiceSession_WindowController()) ||
           !serviceIsActive(appletGetServiceSession_CommonStateGetter()) || !appletGetAppletResourceUserId())
            throw Unsupported("live applet window/common-state services required");
        (void)Focus();first_=count_=0;overflow_=false;exit_=false;
        appletHook(&cookie_,Hook,this);owner_=this;
    }
    void Stop()noexcept override{if(owner_==this){appletUnhook(&cookie_);owner_=nullptr;}first_=count_=0;}
    applet_window::Event Read()override{return {Focus(),exit_};}
    bool Next(applet_window::Event& event)override{
        if(overflow_)throw std::runtime_error("applet notification queue overflow or invalid focus");
        if(count_){event=events_[first_];first_=(first_+1)%events_.size();--count_;return true;}
        u32 message{};const Result result=appletGetMessage(&message);
        if(result==MAKERESULT(128,3))return false; // libnx AM: no messages.
        CheckNx(result,"appletGetMessage");
        if(!appletProcessMessage(message))exit_=true;
        if(count_){event=events_[first_];first_=(first_+1)%events_.size();--count_;}
        else event=Read();
        return true;
    }
    void SetVisible(bool value)override{CheckNx(appletSetAppletWindowVisibility(value),"appletSetAppletWindowVisibility");}
    void Wait()override{
        if(count_ || overflow_)return;
        const auto result=eventWait(appletGetMessageEvent(),50'000'000);
        if(result!=KERNELRESULT(TimedOut))CheckNx(result,"applet message eventWait");
    }
    std::uint32_t RestoreVisible(bool value)noexcept override{return appletSetAppletWindowVisibility(value);}
};
struct SwitchWindow::LifecycleImpl {
    void* geometry_owner{};std::function<void(std::int32_t,std::int32_t)> resize;
    native_cursor::Store cursor;native_input::SwitchInput input{cursor};
    struct Events final:applet_window::Observer {
        native_input::SwitchInput& input;AppletEngineEvents engine;
        explicit Events(native_input::SwitchInput& value):input(value){}
        void FocusChanged(bool active)override{input.Focus(active);engine.FocusChanged(active);}
        void Exit()noexcept override{engine.Exit();}
    } events{input};
    NxAppletBackend backend;applet_window::Lifecycle session;
    explicit LifecycleImpl(bool visible):session(backend,events,visible){}
};
SwitchWindow::SwitchWindow(NWindow& window,bool owner_visible):window_(window),lifecycle_(std::make_unique<LifecycleImpl>(owner_visible)){
    if(!nwindowIsValid(&window_))throw std::invalid_argument("invalid native libnx window");
    (void)ReadClientRect();
}
SwitchWindow::~SwitchWindow(){if(lifecycle_->session.Attached() || lifecycle_->geometry_owner)std::terminate();}
void* SwitchWindow::Identity()const noexcept{return &window_;}
WindowRect SwitchWindow::ReadClientRect(){
    u32 width{},height{};CheckNx(nwindowGetDimensions(&window_,&width,&height),"nwindowGetDimensions");
    if(!width || !height || width>32768 || height>32768)throw std::invalid_argument("native display extent");
    return {0,0,static_cast<std::int32_t>(width),static_cast<std::int32_t>(height)};
}
WindowRect SwitchWindow::ReadWindowRect(){return ReadClientRect();} // Borderless applet layer, no desktop decorations.
void SwitchWindow::BindGeometry(void* owner,std::function<void(std::int32_t,std::int32_t)> resize){
    if(!owner || !resize || lifecycle_->geometry_owner)throw std::logic_error("native geometry owner already present");
    lifecycle_->resize=std::move(resize);lifecycle_->geometry_owner=owner;
}
void SwitchWindow::UnbindGeometry(void* owner)noexcept{if(lifecycle_->geometry_owner==owner){lifecycle_->resize={};lifecycle_->geometry_owner=nullptr;}}
void SwitchWindow::ResizeOuter(std::int32_t width,std::int32_t height){
    if(width<=0 || height<=0 || width>8192 || height>8192)throw std::invalid_argument("native window resize extent");
    if(!lifecycle_->resize)throw Unsupported("native window resize requires owned presentation");
    lifecycle_->resize(width,height); // ReadClientRect reflects committed actual buffer geometry.
}
void SwitchWindow::SetFullscreenStyle(){
    if(!lifecycle_->geometry_owner)throw Unsupported("native fullscreen requires owned presentation");
    lifecycle_->session.Show(true);CheckNx(nwindowSetCrop(&window_,0,0,0,0),"native fullscreen layer crop");
}
void SwitchWindow::MoveFullscreen(std::int32_t width,std::int32_t height){ResizeOuter(width,height);}
std::unique_ptr<WindowAttachment> SwitchWindow::AttachNative(){return std::make_unique<NativeAttachment>(lifecycle_->session,lifecycle_->input,lifecycle_->cursor);}
std::unique_ptr<WindowAttachment> SwitchWindow::Attach(MessageHandler){throw Unsupported("Switch does not install Win32 message hooks");}
void SwitchWindow::Show(Visibility value){lifecycle_->session.Show(value!=Visibility::Minimized);}
void SwitchWindow::Update(){lifecycle_->session.Update();const auto r=ReadClientRect();lifecycle_->input.Poll(r.right,r.bottom,Drawable());}
void SwitchWindow::WaitForDrawable(){lifecycle_->session.WaitForDrawable();const auto r=ReadClientRect();lifecycle_->input.Poll(r.right,r.bottom,Drawable());}
bool SwitchWindow::Drawable()const noexcept{return lifecycle_->session.Drawable();}
std::uint32_t SwitchWindow::LastVisibilityRestoreResult()const noexcept{return lifecycle_->session.RestoreResult();}
CursorHandle SwitchWindow::ExchangeCursor(CursorHandle value){return {lifecycle_->cursor.Exchange(value.value)};}
CursorHandle SwitchWindow::ReadCursor(){return {lifecycle_->cursor.Read().selected};}
void SwitchWindow::DestroyCursorIcon(IconHandle value){lifecycle_->cursor.Destroy(value.value);}
native_cursor::Store& SwitchWindow::CursorStore(){return lifecycle_->cursor;}
void SwitchWindow::ClientPointToScreen(Base::CPoint& point){
    // A real NWindow layer is borderless and fills this coordinate domain.
    // Query the live geometry; there is no desktop non-client origin on VI.
    const auto rect=ReadWindowRect();point.x+=rect.left;point.y+=rect.top;
}
void SwitchWindow::ClipCursorToWindow(){
    const auto rect=ReadClientRect();lifecycle_->input.ConstrainToExtent(rect.right,rect.bottom);
}
void SwitchWindow::ReleaseCursor(){throw Unsupported("Switch has no borrowed Win32 cursor clipping");}
std::intptr_t SwitchWindow::DefaultMessage(std::uint32_t,std::uintptr_t,std::intptr_t){throw Unsupported("Switch has no Win32 default message procedure");}
struct DekoDevice::Impl: presentation::Driver {
    SwitchWindow& window;
    dk::UniqueDevice device;
    dk::UniqueQueue queue;
    WindowRect extent,baseline_extent;
    std::uint32_t baseline_interval{};BqRect baseline_crop{};
    struct DisplayImages {dk::UniqueMemBlock color_memory[3],depth_memory;dk::Image color[3],depth;unsigned count{};};
    struct TargetSet final:presentation::Targets {
        std::shared_ptr<DisplayImages> images=std::make_shared<DisplayImages>();dk::UniqueSwapchain swapchain;
        std::shared_ptr<native_surfaces::Surface> color_surface,depth_surface;
        std::shared_ptr<NativeRenderState> defaults;
        explicit TargetSet(const Presentation& p):Targets(p),defaults(std::make_shared<NativeRenderState>()){
            defaults->vertex.Viewport({0,0,p.BackBufferWidth,p.BackBufferHeight,0,1});
        }
    };
    presentation::Transaction presentation_state{*this};
    std::shared_ptr<presentation::Lifetime> lifetime=std::make_shared<presentation::Lifetime>(presentation::Lifetime{&presentation_state});
    TargetSet& Targets(){auto* p=presentation_state.Active();if(!p)throw Unsupported("native presentation unavailable");return *static_cast<TargetSet*>(const_cast<presentation::Targets*>(p));}
    bool Healthy()const noexcept override{return queue && !dkQueueIsInErrorState(queue);}
    bool Busy()const noexcept override{return bool(frame) || image_slot>=0;}
    bool DefaultResourcesLive()const noexcept override{
        return DefaultResourcesBlockReset(state,texture_session.get(),surface_session.get());
    }
    std::unique_ptr<presentation::Targets> Allocate(const Presentation& p)override{
        auto next=std::make_unique<TargetSet>(p);dk::ImageLayout color_layout,depth_layout;
        const auto color_format=p.BackBufferFormat==PixelFormat::Rgb565?DkImageFormat_BGR565_Unorm:p.BackBufferFormat==PixelFormat::Xrgb8?DkImageFormat_RGBX8_Unorm:DkImageFormat_RGBA8_Unorm;
        const auto depth_format=p.AutoDepthStencilFormat==PixelFormat::Depth16?DkImageFormat_Z16:p.AutoDepthStencilFormat==PixelFormat::Depth24?DkImageFormat_Z24X8:DkImageFormat_Z24S8;
        dk::ImageLayoutMaker{device}.setFlags(DkImageFlags_UsageRender|DkImageFlags_UsagePresent).setFormat(color_format)
            .setDimensions(p.BackBufferWidth,p.BackBufferHeight).initialize(color_layout);
        dk::ImageLayoutMaker{device}.setFlags(DkImageFlags_UsageRender).setFormat(depth_format)
            .setDimensions(p.BackBufferWidth,p.BackBufferHeight).initialize(depth_layout);
        const auto color_bytes=AllocationSize(color_layout.getSize()),depth_bytes=AllocationSize(depth_layout.getSize());
        if(std::uint64_t(color_bytes)*p.BackBufferCount+depth_bytes>256*1024*1024)throw std::bad_alloc();
        for(unsigned i=0;i<p.BackBufferCount;++i){next->images->color_memory[i]=dk::MemBlockMaker{device,color_bytes}.setFlags(DkMemBlockFlags_GpuCached|DkMemBlockFlags_Image).create();
            if(!next->images->color_memory[i])throw std::bad_alloc();next->images->color[i].initialize(color_layout,next->images->color_memory[i],0);}
        next->images->depth_memory=dk::MemBlockMaker{device,depth_bytes}.setFlags(DkMemBlockFlags_GpuCached|DkMemBlockFlags_Image).create();
        if(!next->images->depth_memory)throw std::bad_alloc();next->images->depth.initialize(depth_layout,next->images->depth_memory,0);next->images->count=p.BackBufferCount;
        auto color=std::make_shared<SurfaceResource>();color->display=next->images;
        auto depth=std::make_shared<SurfaceResource>();depth->display=next->images;depth->depth=true;
        next->color_surface=surface_session->Make({unsigned(p.BackBufferFormat),0,0,p.BackBufferWidth,p.BackBufferHeight},false,false,true,color);
        next->depth_surface=surface_session->Make({unsigned(p.AutoDepthStencilFormat),0,0,p.BackBufferWidth,p.BackBufferHeight},true,false,true,depth);return next;
    }
    void Quiesce()override{
        if(Busy())throw std::logic_error("native Reset during acquired frame/scene/submission");
        if(!Healthy())throw Unsupported("native Reset on failed queue");queue.waitIdle();
        if(!Healthy())throw Unsupported("native Reset queue completion failed");ReleaseCompleted();batch.Retired();
    }
    void Attach(presentation::Targets& value)override{
        auto& next=static_cast<TargetSet&>(value);const DkImage* images[]{&next.images->color[0],&next.images->color[1],&next.images->color[2]};
        next.swapchain=dk::SwapchainMaker{device,&window.NativeWindow(),images,next.value.BackBufferCount}.create();
        if(!next.swapchain)throw std::runtime_error("native swapchain recreation failed");
        CheckNx(nwindowSetSwapInterval(&window.NativeWindow(),next.value.PresentationInterval==0x80000000u?0u:1u),"native swap interval");
        CheckNx(nwindowSetCrop(&window.NativeWindow(),0,0,0,0),"native presentation full crop");
    }
    void Detach(presentation::Targets& value)override{
        auto& old=static_cast<TargetSet&>(value);old.swapchain=nullptr;
        // SDK destruction releases buffers too, but discards the Result. Check
        // explicitly so a failed IPC is never converted into Reset success.
        CheckNx(nwindowReleaseBuffers(&window.NativeWindow()),"native release presentation buffers");
    }
    void Commit(presentation::Targets& value)noexcept override{
        auto& next=static_cast<TargetSet&>(value);extent={0,0,int(next.value.BackBufferWidth),int(next.value.BackBufferHeight)};
        state.ResetPrepared(std::move(next.defaults),state.Outstanding()!=0);
        surface_session->Presentation(std::move(next.color_surface),std::move(next.depth_surface));
    }

    std::unique_ptr<materials::DekoCombinerProgram> programs;
    NativeStateStore state;
    std::unique_ptr<native_texture::DekoTextureStore> texture_store;
    std::shared_ptr<native_texture::Session> texture_session;
    std::shared_ptr<native_surfaces::Session> surface_session=std::make_shared<native_surfaces::Session>();
    struct SurfaceResource final:native_surfaces::Resource {
        std::shared_ptr<DisplayImages> display;std::shared_ptr<native_texture::DekoTextureStore::ResidentImage> allocation;bool depth{};
        std::shared_ptr<native_texture::Texture> texture;unsigned level{};
        const std::shared_ptr<native_texture::Texture>* ResetTextureReference()const noexcept override{return &texture;}
        std::shared_ptr<native_texture::DekoTextureStore::ResidentImage> Resolve()const{if(!texture)return allocation;auto image=std::dynamic_pointer_cast<native_texture::DekoTextureStore::ResidentImage>(native_texture::Resident(texture));if(!image)throw std::invalid_argument("foreign texture image backend");return image;}
        const dk::Image& Image(int slot)const{if(texture || allocation)return Resolve()->image;if(!display)throw Unsupported("offscreen image expired");if(depth)return display->depth;if(slot<0 || unsigned(slot)>=display->count)throw std::logic_error("primary surface requires acquired display image");return display->color[slot];}
    };
    std::shared_ptr<debug_font::native::Session> font_session;
    struct FontAtlas final:debug_font::native::Atlas {std::shared_ptr<native_texture::Texture> texture;};
    std::shared_ptr<const native_cursor::Icon> cursor_icon;
    std::shared_ptr<native_texture::Texture> cursor_texture;
    dk::UniqueCmdBuf commands;
    dk::UniqueMemBlock command_memory,command_discard;
    struct CommandDriver {
        using Command=DkCmdBuf;using Block=dk::UniqueMemBlock;Impl& self;
        Block Allocate(std::uint32_t bytes){return self.Allocate(bytes);}
        void Attach(Command command,const Block& block,std::uint32_t bytes)noexcept{dkCmdBufAddMemory(command,block,0,bytes);}
        void AttachDiscard(Command command)noexcept{dkCmdBufAddMemory(command,self.command_discard,0,NativeCommandMemory<CommandDriver>::QuotaBytes);}
        [[noreturn]] void RejectRequest(std::uint64_t minimum)noexcept{
            std::fprintf(stderr,"native SDK command request exceeds complete command quota: %llu\n",static_cast<unsigned long long>(minimum));std::terminate();
        }
    };
    NativeCommandMemory<CommandDriver> command_pages{CommandDriver{*this}};
    static_assert(NativeCommandMemory<CommandDriver>::Alignment==DK_MEMBLOCK_ALIGNMENT);
    static_assert(NativeCommandMemory<CommandDriver>::Alignment%DK_CMDMEM_ALIGNMENT==0);
    static void RefillCommands(void* context,DkCmdBuf command,std::size_t minimum)noexcept{
        static_cast<Impl*>(context)->command_pages.Refill(command,minimum);
    }
    struct DrawPacket {
        dk::UniqueMemBlock vertices,indices,sampler_descriptors,image_descriptors;
        std::array<std::shared_ptr<native_texture::Image>,8> images;
        materials::DekoCombinerProgram::Packet material;
        std::uint32_t bytes{};
    };
    std::vector<DrawPacket> packets;
    NativeDrawBatch batch;
    int image_slot{-1};NativeFrameScope frame;bool recording{};
    std::vector<std::shared_ptr<native_surfaces::Surface>> surface_packets;
    std::vector<std::shared_ptr<native_texture::Image>> target_images;
    static constexpr unsigned command_bytes=NativeCommandMemory<CommandDriver>::BaseBytes,frame_budget=NativeDrawBatch::ByteLimit;
    dk::UniqueMemBlock Allocate(std::uint64_t bytes){
        auto value=dk::MemBlockMaker{device,AllocationSize(bytes)}.setFlags(DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached).create();
        if(!value)throw std::bad_alloc();return value;
    }
    bool presenting{};const dk::Image* bound_color{};const dk::Image* bound_depth{};
    void StartRecording(){
        if(recording){command_pages.Check();return;}if(!Healthy())throw Unsupported("native render queue lost");
        if(!frame){window.WaitForDrawable();queue.waitIdle();if(!Healthy())throw Unsupported("native render queue completion failed");packets.clear();surface_packets.clear();target_images.clear();batch.Retired();frame.Acquire();state.Frame(true);}
        command_pages.Retire([&]()noexcept{commands.clear();commands.addMemory(command_memory,0,command_bytes);});bound_color=bound_depth=nullptr;recording=true;
    }
    void AcquirePrimary(){
        if(image_slot>=0)return;if(!presentation_state.Ready())throw Unsupported("native presentation requires successful Reset");window.WaitForDrawable();const auto size=window.ReadClientRect();
        if(size.right!=extent.right || size.bottom!=extent.bottom)throw Unsupported("native drawable extent changed; recreation required");
        image_slot=queue.acquireImage(Targets().swapchain);if(image_slot<0 || unsigned(image_slot)>=Targets().value.BackBufferCount)throw std::runtime_error("native swapchain acquire failed");
    }
    void BindTargets(bool primary=false){
        StartRecording();if(!primary)surface_session->RequireDraw();auto color=primary?surface_session->primary_color:surface_session->color;auto depth=primary?surface_session->primary_depth:surface_session->depth;
        if(!color || !color->resource)throw Unsupported("native color target expired");auto c=std::dynamic_pointer_cast<SurfaceResource>(color->resource);auto d=depth?std::dynamic_pointer_cast<SurfaceResource>(depth->resource):nullptr;
        if(!c || (depth && !d))throw std::invalid_argument("native target belongs to another GPU backend");if(c->display)AcquirePrimary();
        auto ci_owner=c->Resolve();auto di_owner=d?d->Resolve():nullptr;
        const auto* ci=&c->Image(image_slot);const auto* di=d?&d->Image(image_slot):nullptr;
        if(bound_color==ci && bound_depth==di)return;
        target_images.reserve(target_images.size()+2);if(ci_owner)target_images.push_back(ci_owner);if(di_owner)target_images.push_back(di_owner);
        surface_packets.reserve(surface_packets.size()+2);surface_packets.push_back(color);if(depth)surface_packets.push_back(depth);
        dk::ImageView cv{*ci};commands.barrier(DkBarrier_Full,DkInvalidateFlags_Image|DkInvalidateFlags_L2Cache);
        if(di){dk::ImageView dv{*di};commands.bindRenderTargets(&cv,&dv);}else commands.bindRenderTargets(&cv,nullptr);bound_color=ci;bound_depth=di;
        commands.setScissors(0,{{0,0,color->desc.Width,color->desc.Height}});
        command_pages.Check();
    }
    void SubmitAndWait(){
        if(!recording)throw std::logic_error("native batch submission without recording");
        command_pages.Check();
        commands.barrier(DkBarrier_Full,DkInvalidateFlags_Image|DkInvalidateFlags_L2Cache);
        const auto list=command_pages.FinishForSubmission([&]{return commands.finishList();});
        queue.submitCommands(list);queue.flush();queue.waitIdle();
        if(!Healthy())throw Unsupported("native draw batch queue completion failed");
    }
    void ReleaseCompleted()noexcept{
        command_pages.Retire([&]()noexcept{if(commands){commands.clear();commands.addMemory(command_memory,0,command_bytes);}});recording=false;bound_color=bound_depth=nullptr;
        packets.clear();surface_packets.clear();target_images.clear();
    }
    void DrainBatch(){DrainNativeDrawBatch(frame,batch,*this);}
    void FlushRecording(){
        if(frame)frame.RequirePresent();if(recording)DrainBatch();
        if(image_slot<0){frame.Abort();state.Frame(false);}
    }
    void SurfaceCallbacks(){
        surface_session->textures=texture_session;
        surface_session->import_texture=[](const std::shared_ptr<native_texture::Texture>& texture,unsigned level){auto r=std::make_shared<SurfaceResource>();r->texture=texture;r->level=level;(void)r->Resolve();return r;};
        surface_session->viewport=[this](const VideoViewport& v){state.Edit().vertex.Viewport(v);};
        surface_session->allocate=[this](const offscreen::SurfaceDesc& d,bool depth){if(!Healthy())throw Unsupported("native offscreen allocation on lost queue");auto resource=std::make_shared<SurfaceResource>();resource->depth=depth;resource->allocation=texture_store->AllocateTarget(d.Width,d.Height,d.Format);return resource;};
        surface_session->read=[this](const std::shared_ptr<native_surfaces::Surface>& s){
            FlushRecording();auto r=std::dynamic_pointer_cast<SurfaceResource>(s->resource);if(!r || r->depth)throw std::invalid_argument("native color readback resource");
            auto bytes=texture_store->ReadImage(r->Image(image_slot),s->desc.Width,s->desc.Height,native_surfaces::PixelBytes(s->desc.Format),r->level);
            if(r->display && s->desc.Format!=23)for(std::size_t i=0;i<bytes.size();i+=4)std::swap(bytes[i],bytes[i+2]);return bytes;
        };
        surface_session->write=[this](const std::shared_ptr<native_surfaces::Surface>& s,std::span<const std::byte> bytes){
            FlushRecording();auto r=std::dynamic_pointer_cast<SurfaceResource>(s->resource);if(!r || r->depth)throw std::invalid_argument("native color writeback resource");
            if(r->texture && !(r->texture->usage&1u)){auto next=*r->texture;auto& level=next.levels.at(r->level);level.bytes.assign(bytes.begin(),bytes.end());for(unsigned i=0;i<next.levels.size();++i){if(i!=r->level && next.levels[i].locked)throw std::logic_error("other mip level locked during surface upload");}level.locked=false;auto image=texture_store->Upload(next);r->texture->levels.at(r->level).bytes.swap(level.bytes);r->texture->image=std::move(image);r->texture->dirty=false;return;}
            if(r->display && s->desc.Format!=23){auto converted=std::vector<std::byte>(bytes.begin(),bytes.end());for(std::size_t i=0;i<converted.size();i+=4)std::swap(converted[i],converted[i+2]);texture_store->WriteImage(r->Image(image_slot),s->desc.Width,s->desc.Height,4,converted);}
            else texture_store->WriteImage(r->Image(image_slot),s->desc.Width,s->desc.Height,native_surfaces::PixelBytes(s->desc.Format),bytes,r->level);
        };
    }
    Impl(SwitchWindow& window,std::uint32_t texture_bytes):window(window),extent(window.ReadClientRect()),baseline_extent(extent),baseline_interval(window.NativeWindow().swap_interval),baseline_crop(window.NativeWindow().crop){
        // Do not adopt or disrupt a swapchain belonging to the main game.
        if(window.NativeWindow().slots_configured)throw std::logic_error("native window already has configured display buffers");
        struct Rollback {Impl& self;bool committed{};~Rollback(){if(!committed){try{if(self.queue)self.presentation_state.Release();}catch(...){std::fprintf(stderr,"native construction presentation rollback failed\n");}self.RestoreWindow();}}} rollback{*this};
        device=dk::DeviceMaker{}.setFlags(DkDeviceFlags_DepthZeroToOne|DkDeviceFlags_OriginUpperLeft).create();
        if(!device)throw std::runtime_error("deko device creation failed");
        queue=dk::QueueMaker{device}.setFlags(DkQueueFlags_Graphics).create();
        if(!queue)throw std::runtime_error("deko queue creation failed");
        programs=std::make_unique<materials::DekoCombinerProgram>(device,
            std::as_bytes(std::span{materials::compiled::frag}),std::as_bytes(std::span{materials::compiled::vert}));
        if(!programs->VertexReady())throw Unsupported("native material shader pair did not initialize");
        state.Edit().vertex.Viewport({0,0,std::uint32_t(extent.right),std::uint32_t(extent.bottom),0,1});
        command_memory=Allocate(command_bytes);
        command_discard=Allocate(NativeCommandMemory<CommandDriver>::QuotaBytes);
        commands=dk::CmdBufMaker{device}.setUserData(this).setCbAddMem(RefillCommands).create();if(!commands)throw std::bad_alloc();
        Presentation initial{};initial.BackBufferWidth=extent.right;initial.BackBufferHeight=extent.bottom;
        initial.BackBufferFormat=PixelFormat::Argb8;initial.hDeviceWindow=window.Identity();initial.Windowed=true;initial.SwapEffect=1;
        initial.EnableAutoDepthStencil=true;initial.AutoDepthStencilFormat=PixelFormat::Depth24Stencil8;initial.PresentationInterval=1;initial.Flags=2;
        initial=presentation::Plan(initial,window.Identity(),0);
        CheckDeviceResult(presentation_state.Reset(initial),"native initial presentation");
        texture_store=std::make_unique<native_texture::DekoTextureStore>(device,queue,texture_bytes);
        texture_session=std::make_shared<native_texture::Session>();
        texture_session->upload=[this](const native_texture::Texture& t){return texture_store->Upload(t);};
        SurfaceCallbacks();
        rollback.committed=true;
    }
    DeviceResult Reset(Presentation& request,std::uint32_t refresh=0)noexcept{
        Presentation planned;
        try{planned=presentation::Plan(request,window.Identity(),refresh);}
        catch(const std::invalid_argument&){return presentation::InvalidCall;}
        catch(...){return presentation::NotAvailable;}
        const auto result=presentation_state.Reset(planned);if(result==DeviceSuccess)request=planned;return result;
    }
    void RestoreWindow()noexcept{
        const auto size=nwindowSetDimensions(&window.NativeWindow(),baseline_extent.right,baseline_extent.bottom);
        const auto interval=nwindowSetSwapInterval(&window.NativeWindow(),baseline_interval);
        const auto crop=nwindowSetCrop(&window.NativeWindow(),baseline_crop.left,baseline_crop.top,baseline_crop.right,baseline_crop.bottom);
        if(size || interval || crop)std::fprintf(stderr,"native window baseline restore failed: %08x/%08x/%08x\n",unsigned(size),unsigned(interval),unsigned(crop));
    }
    void Record(std::span<const materials::Vertex> vertices,std::span<const std::byte> indices,
                  DkIdxFormat index_format,std::uint32_t count,std::int32_t base,DkPrimitive primitive,const NativeRenderState* override_state=nullptr){
        if(!frame)throw Unsupported("original draw requires a live native acquire/frame scope");
        const auto& saved_state=override_state?*override_state:state.Read();
        saved_state.ValidateDraw(primitive==DkPrimitive_Points,primitive==DkPrimitive_Lines || primitive==DkPrimitive_LineStrip);
        const auto stencil=saved_state.Stencil();const auto stencil_depth=presenting?surface_session->primary_depth:surface_session->depth;
        RequireStencilAttachment(stencil,bool(stencil_depth),stencil_depth?stencil_depth->desc.Format:0);
        frame.RequireDraw(override_state!=nullptr);
        const auto& vertex=saved_state.vertex;const auto& combiner=saved_state.combiner;
        const auto& textures=saved_state.textures;const auto& samplers=saved_state.samplers;
        const auto target=presenting?surface_session->primary_color:surface_session->color;const bool alpha_target=target->desc.Format==28;
        const auto raster=alpha_target?AlphaTargetRaster(saved_state.raster):saved_state.raster;const auto cull=saved_state.cull;
        const auto uniforms=vertex.Prepare();const auto fog=saved_state.PixelFog();
        const std::uint64_t bytes=std::uint64_t(AllocationSize(vertices.size_bytes()))+(indices.empty()?0:AllocationSize(indices.size()))+
            AllocationSize(sizeof(materials::Uniforms))+AllocationSize(sizeof(materials::VertexUniforms))+AllocationSize(8*sizeof(DkSamplerDescriptor))+AllocationSize(8*sizeof(DkImageDescriptor));
        if(batch.NeedsDrain(bytes) || command_pages.NeedsDrain())DrainBatch();
        BindTargets(presenting);
        DrawPacket packet;packet.vertices=Allocate(vertices.size_bytes());std::memcpy(packet.vertices.getCpuAddr(),vertices.data(),vertices.size_bytes());
        if(!indices.empty()){packet.indices=Allocate(indices.size());std::memcpy(packet.indices.getCpuAddr(),indices.data(),indices.size());}
        packet.sampler_descriptors=Allocate(8*sizeof(DkSamplerDescriptor));
        packet.image_descriptors=Allocate(8*sizeof(DkImageDescriptor));
        std::memset(packet.image_descriptors.getCpuAddr(),0,8*sizeof(DkImageDescriptor));
        for(unsigned i=0;i<8;++i)if(textures[i]){
            if(target->texture==textures[i])throw Unsupported("render target texture feedback is invalid");
            packet.images[i]=native_texture::Resident(textures[i]);
            auto image=std::dynamic_pointer_cast<native_texture::DekoTextureStore::ResidentImage>(packet.images[i]);
            if(!image)throw std::logic_error("native texture image belongs to another backend");
            std::memcpy(static_cast<std::byte*>(packet.image_descriptors.getCpuAddr())+i*sizeof(DkImageDescriptor),&image->descriptor,sizeof(DkImageDescriptor));
        }
        for(unsigned i=0;i<8;++i){const auto& s=samplers[i];s.Validate();
            auto filter=[](TextureFilter f){return f==TextureFilter::Point?DkFilter_Nearest:DkFilter_Linear;};
            auto wrap=[](TextureAddress a){return a==TextureAddress::Clamp?DkWrapMode_ClampToEdge:DkWrapMode_Repeat;};
            dk::Sampler sampler;sampler.setFilter(filter(s.min),filter(s.mag),s.mip==TextureMipFilter::None?DkMipFilter_None:s.mip==TextureMipFilter::Point?DkMipFilter_Nearest:DkMipFilter_Linear)
                .setWrapMode(wrap(s.u),wrap(s.v),wrap(s.w)).setLodBias(s.lod_bias).setMaxAnisotropy(s.min==TextureFilter::Anisotropic?float(s.max_anisotropy):1.f);
            dk::SamplerDescriptor descriptor;descriptor.initialize(sampler);std::memcpy(static_cast<std::byte*>(packet.sampler_descriptors.getCpuAddr())+i*sizeof(descriptor),&descriptor,sizeof(descriptor));}
        std::array<DkResHandle,8> handles;for(unsigned i=0;i<8;++i)handles[i]=dkMakeTextureHandle(i,i);
        packet.material=programs->Prepare(combiner,handles,&uniforms,alpha_target,&fog,saved_state.shade);
        packets.push_back(std::move(packet));auto& saved=packets.back();batch.Add(bytes);
        const auto raster_viewport=LegacyRasterViewport(uniforms.viewport);
        commands.setViewports(0,{{raster_viewport[0],raster_viewport[1],raster_viewport[2],raster_viewport[3],0,1}});
        const auto clip=LegacyRasterClip(uniforms.viewport);
        commands.setScissors(0,{{clip[0],clip[1],clip[2],clip[3]}});
        dk::RasterizerState rs;rs.setFrontFace(DkFrontFace_CW).setProvokingVertex(DkProvokingVertex_First).setCullMode(cull==1?DkFace_None:cull==2?DkFace_Front:DkFace_Back);commands.bindRasterizerState(rs);
        if(primitive==DkPrimitive_Lines || primitive==DkPrimitive_LineStrip){
            // Bind actual aliased line state on every draw; neither inherited
            // width nor a foreign line stipple may alter original one-pixel UI.
            commands.setLineWidth(1.f);commands.setLineStipple(false,1,0xffff);
        }
        dk::ColorState color;color.setBlendEnable(0,raster.blend_enable).setAlphaCompareOp(raster.alpha_test?static_cast<DkCompareOp>(raster.alpha_compare):DkCompareOp_Always);
        dk::ColorWriteState write;write.setMask(0,raster.color_write);
        dk::DepthStencilState depth;depth.setDepthTestEnable(raster.depth_test).setDepthWriteEnable(raster.depth_write).setDepthCompareOp(static_cast<DkCompareOp>(raster.depth_compare));
        PrepareStencil(commands,depth,stencil);
        dk::BlendState blend;blend.setFactors(static_cast<DkBlendFactor>(raster.source),static_cast<DkBlendFactor>(raster.destination),static_cast<DkBlendFactor>(raster.source),static_cast<DkBlendFactor>(raster.destination))
            .setOps(static_cast<DkBlendOp>(raster.operation),static_cast<DkBlendOp>(raster.operation));
        commands.bindMultisampleState(dk::MultisampleState{});commands.bindColorState(color);commands.bindColorWriteState(write);commands.bindDepthStencilState(depth);commands.bindBlendStates(0,{blend});commands.setAlphaRef(float(raster.alpha_reference)/255);
        std::array<DkVtxAttribState,12> attributes;for(unsigned i=0;i<12;++i)attributes[i]={0,0,i*16,DkVtxAttribSize_4x32,DkVtxAttribType_Float,0};
        commands.bindVtxAttribState(attributes);commands.bindVtxBufferState({{sizeof(materials::Vertex),0}});commands.bindVtxBuffer(0,saved.vertices.getGpuAddr(),vertices.size_bytes());
        commands.bindImageDescriptorSet(saved.image_descriptors.getGpuAddr(),8);commands.bindSamplerDescriptorSet(saved.sampler_descriptors.getGpuAddr(),8);
        programs->Bind(commands,saved.material);commands.barrier(DkBarrier_None,DkInvalidateFlags_Shader|DkInvalidateFlags_Descriptors|DkInvalidateFlags_L2Cache);
        if(!indices.empty()){commands.bindIdxBuffer(index_format,saved.indices.getGpuAddr());commands.drawIndexed(primitive,count,1,0,base,0);}else commands.draw(primitive,count,1,0,0);
        command_pages.Check();
    }
    void DrawCursor(){
        auto& cursors=window.CursorStore();const auto icon=cursors.Visible();
        if(!icon || !window.Drawable()){cursor_icon.reset();cursor_texture.reset();return;}
        if(icon!=cursor_icon){
            auto texture=std::make_shared<native_texture::Texture>();texture->session=texture_session;texture->format=21;texture->pool=1;
            native_texture::Level level{icon->width,icon->height,icon->width*4,icon->height,native_cursor::Bgra(*icon)};
            texture->levels.push_back(std::move(level));(void)native_texture::Resident(texture);
            texture_session->textures.push_back(texture);cursor_texture=std::move(texture);cursor_icon=icon;
        }
        NativeRenderState overlay;overlay.vertex.Layout(0x104);overlay.vertex.Viewport({0,0,unsigned(extent.right),unsigned(extent.bottom),0,1});
        overlay.vertex.Render(137,0);overlay.cull=1;overlay.raster.depth_test=overlay.raster.depth_write=false;overlay.raster.blend_enable=true;
        overlay.textures[0]=cursor_texture;overlay.combiner.Bind(0,true);overlay.combiner.Set(0,1,2);overlay.combiner.Set(0,2,2);overlay.combiner.Set(0,4,2);overlay.combiner.Set(0,5,2);overlay.combiner.Set(1,1,1);
        overlay.samplers[0].u=overlay.samplers[0].v=TextureAddress::Clamp;
        overlay.samplers[0].min=overlay.samplers[0].mag=TextureFilter::Point;overlay.samplers[0].mip=TextureMipFilter::None;
        const auto quad=native_cursor::Quad(*icon,cursors.Read());
        std::array<materials::Vertex,4> vertices;
        for(unsigned i=0;i<4;++i){vertices[i].position={quad[i].x,quad[i].y,0,1};vertices[i].uv[0]={quad[i].u,quad[i].v,0,1};}
        Record(vertices,{},DkIdxFormat_Uint16,4,0,DkPrimitive_TriangleStrip,&overlay);
    }
    std::shared_ptr<debug_font::native::Session> Fonts(){
        if(font_session)return font_session;
        auto session=std::make_shared<debug_font::native::Session>();session->data=debug_font::native::Data::Load(debug_font::native::OriginalAsset());
        session->upload=[this](const debug_font::native::Data& source){
            auto atlas=std::make_shared<FontAtlas>();auto texture=std::make_shared<native_texture::Texture>();
            texture->session=texture_session;texture->format=21;texture->pool=1;
            texture->levels.push_back({source.width,source.height,source.width*4,source.height,source.rgba});
            (void)native_texture::Resident(texture);texture_session->textures.push_back(texture);atlas->texture=std::move(texture);return atlas;
        };
        session->draw=[this](const std::shared_ptr<debug_font::native::Atlas>& value,std::span<const debug_font::native::Quad> quads){
            auto atlas=std::dynamic_pointer_cast<FontAtlas>(value);if(!atlas)throw std::invalid_argument("debug atlas belongs to another backend");
            if(quads.size()>65536)throw std::length_error("debug font glyph draw budget");
            NativeRenderState overlay;overlay.vertex.Layout(0x144);overlay.vertex.Viewport({0,0,unsigned(extent.right),unsigned(extent.bottom),0,1});
            overlay.vertex.Render(137,0);overlay.cull=1;overlay.raster.depth_test=overlay.raster.depth_write=false;overlay.raster.blend_enable=true;
            overlay.textures[0]=atlas->texture;overlay.combiner.Bind(0,true);
            for(unsigned op:{1u,4u})overlay.combiner.Set(0,op,4);
            for(unsigned argument:{2u,5u})overlay.combiner.Set(0,argument,2);
            for(unsigned argument:{3u,6u})overlay.combiner.Set(0,argument,0);
            overlay.combiner.Set(1,1,1);overlay.samplers[0].u=overlay.samplers[0].v=TextureAddress::Clamp;
            overlay.samplers[0].min=overlay.samplers[0].mag=TextureFilter::Point;overlay.samplers[0].mip=TextureMipFilter::None;
            std::vector<materials::Vertex> vertices;vertices.reserve(quads.size()*6);
            for(const auto& q:quads){std::array<materials::Vertex,4> corners;
                for(unsigned i=0;i<4;++i){auto& v=corners[i];v.position={(i&1?q.right:q.left)-0.5f,(i&2?q.bottom:q.top)-0.5f,0,1};
                    v.uv[0]={i&1?q.u1:q.u0,i&2?q.v1:q.v0,0,1};
                    v.diffuse={float((q.color>>16)&255)/255,float((q.color>>8)&255)/255,float(q.color&255)/255,float(q.color>>24)/255};}
                for(unsigned index:{0u,1u,2u,2u,1u,3u})vertices.push_back(corners[index]);}
            if(!vertices.empty())Record(vertices,{},DkIdxFormat_Uint16,unsigned(vertices.size()),0,DkPrimitive_Triangles,&overlay);
        };
        font_session=session;return session;
    }
    ~Impl(){
        lifetime->owner=nullptr;if(queue)queue.waitIdle();commands=nullptr;packets.clear();surface_packets.clear();target_images.clear();
        if(surface_session)surface_session->Close();surface_session.reset();
        try{presentation_state.Release();}catch(...){std::fprintf(stderr,"native presentation teardown failed\n");}
        if(font_session)font_session->Close();font_session.reset();cursor_texture.reset();cursor_icon.reset();
        state.Close();if(texture_session)texture_session->Shutdown();texture_session.reset();texture_store.reset();queue=nullptr;RestoreWindow();
    }
};
DekoDevice::DekoDevice(SwitchWindow& window,std::uint32_t bytes):impl_(std::make_unique<Impl>(window,bytes)){
    window.BindGeometry(this,[this](std::int32_t width,std::int32_t height){
        auto value=impl_->Targets().value;value.BackBufferWidth=width;value.BackBufferHeight=height;
        CheckDeviceResult(ResetPresentation(value),"native window resize presentation");
    });
}
DekoDevice::~DekoDevice(){impl_->window.UnbindGeometry(this);const auto result=AbortNativeFrame();if(result)std::fprintf(stderr,"native frame cleanup failed: %08x\n",unsigned(result));}
CreationParameters DekoDevice::ReadCreationParameters(){
    if(!impl_->device || !impl_->queue || !impl_->programs->VertexReady())throw Unsupported("native hardware device not created");
    // Native adapter zero is this hardware device; no COM/Win32 identity or
    // Windows CPU/mixed vertex-processing creation flags are impersonated.
    return {0,1,0,impl_->window.Identity()};
}
DisplayMode DekoDevice::ReadAdapterDisplayMode(std::uint32_t adapter){
    if(adapter)throw std::invalid_argument("native adapter ordinal");
    if(&impl_->window.NativeWindow()!=nwindowGetDefault())throw Unsupported("native display query requires default NWindow or an explicit display binding");
    impl_->window.WaitForDrawable();ViDisplay display{};CheckNx(viOpenDefaultDisplay(&display),"viOpenDefaultDisplay");
    struct DisplayOwner {ViDisplay& display;~DisplayOwner(){const auto result=viCloseDisplay(&display);if(result)std::fprintf(stderr,"viCloseDisplay failed: %08x\n",unsigned(result));}} display_owner{display};
    s32 width{},height{},after_width{},after_height{};CheckNx(viGetDisplayResolution(&display,&width,&height),"viGetDisplayResolution");
    if(width<=0 || height<=0 || width>16384 || height>16384)throw std::invalid_argument("native VI display extent");
    Event event{};CheckNx(viGetDisplayVsyncEvent(&display,&event),"viGetDisplayVsyncEvent");
    struct EventOwner {Event& event;~EventOwner(){eventClose(&event);}} event_owner{event};
    std::array<std::uint64_t,9> timestamps{};
    for(auto& timestamp:timestamps){if(!event.autoclear)CheckNx(eventClear(&event),"display VBlank clear");
        CheckNx(eventWait(&event,250'000'000),"display VBlank wait");timestamp=armTicksToNs(armGetSystemTick());}
    CheckNx(viGetDisplayResolution(&display,&after_width,&after_height),"VI display resolution recheck");
    if(width!=after_width || height!=after_height)throw Unsupported("display mode changed during native query");
    // VI does not return a D3D scanout format. Report the actual native layer
    // buffer format, which is what ApplyVideoParams can choose/reset here.
    const auto format=impl_->window.NativeWindow().format;PixelFormat native;
    if(format==PIXEL_FORMAT_RGBA_8888 || format==PIXEL_FORMAT_BGRA_8888)native=PixelFormat::Argb8;
    else if(format==PIXEL_FORMAT_RGBX_8888)native=PixelFormat::Xrgb8;
    else if(format==PIXEL_FORMAT_RGB_565)native=PixelFormat::Rgb565;
    else throw Unsupported("native layer display format unsupported");
    return {unsigned(width),unsigned(height),presentation::RefreshFromVblanks(timestamps),native};
}
DeviceResult DekoDevice::CheckMultisample(std::uint32_t adapter,std::uint32_t type,PixelFormat format,bool windowed,std::uint32_t samples){return presentation::Multisample(adapter,type,format,windowed,samples);}
DeviceResult DekoDevice::ResetPresentation(Presentation& value){
    if(!impl_->state.Outstanding())throw Unsupported("native Reset requires captured presentation owner");
    const auto refresh=value.FullScreen_RefreshRateInHz?ReadAdapterDisplayMode(0).RefreshRate:0;
    const auto result=impl_->Reset(value,refresh);
    std::fprintf(stderr,"native presentation Reset %ux%u result=%08x\n",value.BackBufferWidth,value.BackBufferHeight,unsigned(result));return result;
}
WindowRect DekoDevice::DrawableExtent()const noexcept{return impl_->extent;}
DeviceResult DekoDevice::SetTextureStageState(std::uint32_t s,TextureStageRegister r,std::uint32_t v){auto& state=impl_->state.Edit();if(unsigned(r)==11 || unsigned(r)==24)state.vertex.Stage(s,unsigned(r),v);else state.combiner.Set(s,unsigned(r),v);return 0;}
std::uint32_t DekoDevice::ReadTextureStageState(std::uint32_t s,TextureStageRegister r){const auto& state=impl_->state.Read();if(unsigned(r)==11 || unsigned(r)==24)return state.vertex.Stage(s,unsigned(r));return state.combiner.Get(s,unsigned(r));}
DeviceResult DekoDevice::SetTextureMatrix(std::uint32_t s,const WorldMatrix& m){if(s>=8)throw std::out_of_range("texture transform stage");impl_->state.Edit().vertex.Transform(16+s,m);return 0;}
DeviceResult DekoDevice::SetWorldTransform(const WorldMatrix& m){return NativeTransform(256,m);}
WorldMatrix DekoDevice::ReadWorldTransform(){return impl_->state.Read().vertex.Transform(256);}
DeviceResult DekoDevice::NativeTransform(std::uint32_t t,const WorldMatrix& m){impl_->state.Edit().vertex.Transform(t,m);return 0;}
DeviceResult DekoDevice::SetFixedVertexLayout(std::uint32_t fvf){impl_->state.Edit().vertex.Layout(fvf);return 0;}
DeviceResult DekoDevice::SetVideoViewport(const VideoViewport& v){impl_->state.Edit().vertex.Viewport(v);return 0;}
VideoViewport DekoDevice::NativeReadViewport()const{return impl_->state.Read().vertex.Viewport();}
DeviceResult DekoDevice::NativeCooperativeLevel()const{
    // Query the real queue; never synthesize an OK state after a GPU failure.
    if(!impl_->Healthy())return presentation::DeviceLost;
    if(!impl_->presentation_state.Ready())return static_cast<DeviceResult>(0x88760869u);
    return DeviceSuccess;
}
std::uint32_t DekoDevice::NativeRasterCaps()const{
    if(!impl_->programs || !impl_->programs->VertexReady())throw Unsupported("native pixel fog shader pair unavailable");
    return materials::PixelFogCaps;
}
std::uint32_t DekoDevice::NativeStencilCaps()const{
    if(!impl_->programs || !impl_->programs->VertexReady())throw Unsupported("native stencil/flat shader pair unavailable");
    return srhd_awa::matrixgame::NativeStencilCaps;
}
DeviceResult DekoDevice::SetVideoMaterial(const VideoMaterial& m){impl_->state.Edit().vertex.Material(m);return 0;}
DeviceResult DekoDevice::SetVideoLight(std::uint32_t i,const VideoLight& l){impl_->state.Edit().vertex.Light(i,l);return 0;}
DeviceResult DekoDevice::EnableVideoLight(std::uint32_t i,bool on){impl_->state.Edit().vertex.EnableLight(i,on);return 0;}
DeviceResult DekoDevice::SetSamplerState(std::uint32_t i,SamplerRegister r,std::uint32_t v){impl_->state.Edit().Sampler(i,unsigned(r),v);return DeviceSuccess;}
std::uint32_t DekoDevice::ReadSamplerState(std::uint32_t i,SamplerRegister r){return impl_->state.Read().Sampler(i,unsigned(r));}
DeviceResult DekoDevice::SetRenderState(RasterRegister r,std::uint32_t v){impl_->state.Edit().Render(unsigned(r),v);return DeviceSuccess;}
std::uint32_t DekoDevice::ReadRenderState(RasterRegister r){return impl_->state.Read().Render(unsigned(r));}
NativeStateStore& DekoDevice::StoredStates(){return impl_->state;}
DeviceResult DekoDevice::ClearTexture(std::uint32_t i){auto& state=impl_->state.Edit();state.textures.at(i).reset();state.combiner.Bind(i,false);return 0;}
bool DekoDevice::TextureBound(std::uint32_t i){return impl_->state.Read().combiner.Bound().at(i);}
DeviceResult DekoDevice::BindBorrowedTexture(std::uint32_t i,void* texture){
    if(!texture)return ClearTexture(i);
    if(i>=8)throw std::out_of_range("native texture sampler");
    auto resource=native_texture::Find(texture);
    if(resource->session!=impl_->texture_session)throw std::invalid_argument("native texture belongs to another device");
    (void)native_texture::Resident(resource); // Failure leaves the old binding intact.
    auto& state=impl_->state.Edit();state.textures[i]=std::move(resource);state.combiner.Bind(i,true);return 0;
}
DeviceResult DekoDevice::BindStagingVertex(std::uint32_t stream,buffers::VertexBuffer* buffer,std::uint32_t offset,std::uint32_t stride){
    const auto current=buffers::Current().Vertex();
    if(stream || current.buffer!=buffer || current.offset!=offset || current.stride!=stride)throw Unsupported("native vertex binding differs from retained staging context");return 0;
}
DeviceResult DekoDevice::BindStagingIndex(buffers::IndexBuffer* buffer){if(buffers::Current().Index()!=buffer)throw Unsupported("native index binding differs from retained staging context");return 0;}
namespace {
DkPrimitive NativePrimitive(std::uint32_t p){switch(p){case 1:return DkPrimitive_Points;case 2:return DkPrimitive_Lines;case 3:return DkPrimitive_LineStrip;case 4:return DkPrimitive_Triangles;case 5:return DkPrimitive_TriangleStrip;case 6:return DkPrimitive_TriangleFan;default:throw Unsupported("native primitive topology");}}
}
DeviceResult DekoDevice::NativeDraw(std::uint32_t kind,std::uint32_t start,std::uint32_t primitives){
    const auto elements=staging_draw::ElementCount(kind,primitives);if(!elements)return 0;
    if(elements>Impl::frame_budget/sizeof(materials::Vertex))throw std::length_error("native vertex draw budget");
    const auto binding=buffers::Current().Vertex();for(std::uint64_t i=0;i<elements;++i)staging_draw::Vertex(binding,std::uint64_t(start)+i);
    const auto bytes=binding.buffer->Storage().subspan(binding.offset+std::uint64_t(start)*binding.stride,elements*binding.stride);
    auto vertices=impl_->state.Read().vertex.Decode(bytes,binding.stride);impl_->Record(vertices,{},DkIdxFormat_Uint16,elements,0,NativePrimitive(kind));return 0;
}
DeviceResult DekoDevice::NativeDrawUP(std::uint32_t kind,std::uint32_t primitives,std::span<const std::byte> bytes,std::uint32_t stride){
    const auto elements=staging_draw::ElementCount(kind,primitives);if(!elements)return 0;
    if(elements>Impl::frame_budget/sizeof(materials::Vertex) || std::uint64_t(stride)*elements!=bytes.size())throw std::length_error("native immediate draw extent");
    auto vertices=impl_->state.Read().vertex.Decode(bytes,stride);impl_->Record(vertices,{},DkIdxFormat_Uint16,elements,0,NativePrimitive(kind));return 0;
}
DeviceResult DekoDevice::NativeDrawIndexed(std::uint32_t kind,int base,std::uint32_t minimum,std::uint32_t count,std::uint32_t start,std::uint32_t primitives){
    const auto elements=staging_draw::ElementCount(kind,primitives);if(!elements)return 0;
    if(count>Impl::frame_budget/sizeof(materials::Vertex) || minimum>std::uint32_t(std::numeric_limits<int>::max()))throw std::length_error("native indexed vertex extent");
    if(elements>Impl::frame_budget/2)throw std::length_error("native index draw budget");
    auto& context=buffers::Current();staging_draw::ValidateIndexed(context,kind,base,minimum,count,start,primitives);
    const auto binding=context.Vertex();const auto* index=context.Index();const unsigned width=unsigned(index->Width());
    const auto indices=index->Storage().subspan(std::uint64_t(start)*width,elements*width);
    std::vector<materials::Vertex> vertices(count);std::vector<bool> decoded(count);
    for(std::size_t i=0;i<elements;++i){std::uint32_t value{};if(width==2){std::uint16_t v;std::memcpy(&v,indices.data()+i*2,2);value=v;}else std::memcpy(&value,indices.data()+i*4,4);
        const auto local=value-minimum;if(!decoded.at(local)){const auto offset=binding.offset+std::uint64_t(std::int64_t(base)+value)*binding.stride;
            vertices[local]=impl_->state.Read().vertex.Decode(binding.buffer->Storage().subspan(offset,binding.stride),binding.stride).at(0);decoded[local]=true;}}
    // Binding begins at the declared minimum; indices and their order stay
    // unchanged. The GPU base compensates this upload view's byte origin.
    impl_->Record(vertices,indices,width==2?DkIdxFormat_Uint16:DkIdxFormat_Uint32,elements,-int(minimum),NativePrimitive(kind));return 0;
}
void DekoDevice::SetVertexLayout(std::uint32_t fvf){(void)SetFixedVertexLayout(fvf);}
void DekoDevice::Emit(const MeshRange& range){if(range.index_count%3)throw std::invalid_argument("original VO triangle count");(void)NativeDrawIndexed(4,range.base_vertex,range.minimum_vertex,range.vertex_count,range.first_index,range.index_count/3);}
void DekoDevice::AcquireNativeFrame(){
    auto& v=*impl_;if(v.frame)throw std::logic_error("native frame already acquired");
    // Native frame entry binds the display without changing the original
    // game viewport; only the explicit D3D-style SetTarget resets viewport.
    v.surface_session->color=v.surface_session->primary_color;v.surface_session->depth=v.surface_session->primary_depth;
    v.StartRecording();v.BindTargets(true);
}
void DekoDevice::BeginNativeFrame(std::uint32_t argb){
    AcquireNativeFrame();auto& v=*impl_;
    v.commands.clearColor(0,DkColorMask_RGBA,float((argb>>16)&255)/255,float((argb>>8)&255)/255,float(argb&255)/255,float(argb>>24)/255);
    v.commands.clearDepthStencil(true,1.f,v.Targets().value.AutoDepthStencilFormat==PixelFormat::Depth24Stencil8?0xff:0,0);
    v.command_pages.Check();
}
void DekoDevice::PresentNativeFrame(){
    auto& v=*impl_;v.frame.RequirePresent();v.BindTargets(true);
    struct PresentScope {bool& flag;explicit PresentScope(bool& f):flag(f){flag=true;}~PresentScope(){flag=false;}} scope{v.presenting};v.DrawCursor();
    v.command_pages.Check();v.commands.barrier(DkBarrier_Full,DkInvalidateFlags_L2Cache);
    const auto list=v.command_pages.FinishForSubmission([&]{return v.commands.finishList();});
    v.queue.submitCommands(list);v.frame.Submitted();v.queue.presentImage(v.Targets().swapchain,v.image_slot);v.image_slot=-1;v.queue.flush();v.queue.waitIdle();if(!v.Healthy())throw std::runtime_error("native presentation queue error");
    v.ReleaseCompleted();v.batch.Retired();v.frame.Presented();v.state.Frame(false);
}
DeviceResult DekoDevice::AbortNativeFrame()noexcept{
    auto& v=*impl_;if(!v.queue || !v.Healthy())return presentation::DeviceLost;
    try{
        if(v.frame || v.image_slot>=0 || v.recording){v.queue.waitIdle();if(!v.Healthy())return presentation::DeviceLost;
            if(v.image_slot>=0){NvMultiFence fence{};const auto result=nwindowCancelBuffer(&v.window.NativeWindow(),v.image_slot,&fence);if(result)return static_cast<DeviceResult>(result);}
            v.ReleaseCompleted();v.batch.Retired();v.image_slot=-1;v.frame.Abort();v.state.Frame(false);}
        if(v.surface_session->color!=v.surface_session->primary_color)v.surface_session->Color(v.surface_session->primary_color);
        v.surface_session->Depth(v.surface_session->primary_depth);return DeviceSuccess;
    }catch(const std::bad_alloc&){return native_texture::OutOfMemory;}catch(...){return presentation::DeviceLost;}
}
DeviceResult DekoDevice::NativeClear(std::span<const ClearRectangle> rectangles,std::uint32_t flags,std::uint32_t argb,float depth,std::uint32_t stencil){
    auto& v=*impl_;v.state.Read().ValidateClear();v.surface_session->RequireDraw();const auto color=v.surface_session->color,ds=v.surface_session->depth;
    if((flags&6) && !ds)return presentation::InvalidCall;if((flags&4) && ds->desc.Format!=75)return presentation::InvalidCall;
    const auto plan=PlanNativeClear(v.state.Read().vertex.Viewport(),color->desc.Width,color->desc.Height,rectangles,flags,depth,stencil);
    v.BindTargets();v.frame.RequireClear();
    for(const auto& r:plan){
        if(v.command_pages.NeedsDrain()){v.DrainBatch();v.BindTargets();}
        v.commands.setScissors(0,{{unsigned(r.left),unsigned(r.top),unsigned(r.right-r.left),unsigned(r.bottom-r.top)}});
        if(flags&1)v.commands.clearColor(0,DkColorMask_RGBA,float((argb>>(color->desc.Format==28?24:16))&255)/255,float((argb>>8)&255)/255,float(argb&255)/255,float(argb>>24)/255);
        if(flags&6)v.commands.clearDepthStencil(flags&2,depth,flags&4?0xff:0,stencil);v.command_pages.Check();}
    v.commands.setScissors(0,{{0,0,color->desc.Width,color->desc.Height}});v.command_pages.Check();return DeviceSuccess;
}
DeviceResult DekoDevice::NativeBeginScene(){impl_->StartRecording();impl_->frame.BeginScene();return DeviceSuccess;}
DeviceResult DekoDevice::NativeEndScene(){impl_->frame.RequireDraw();impl_->commands.barrier(DkBarrier_Full,DkInvalidateFlags_Image|DkInvalidateFlags_L2Cache);impl_->command_pages.Check();impl_->frame.EndScene();return DeviceSuccess;}
DeviceResult DekoDevice::NativePresent(){PresentNativeFrame();return DeviceSuccess;}
DeviceCaps DekoDevice::ReadCaps(){
    if(!impl_->programs || !impl_->programs->VertexReady())throw Unsupported("native material shader pair unavailable");
    return {false,8,16,1,8}; // Eight implemented cascade/sampler slots; one actual swapchain.
}
GammaRamp DekoDevice::ReadGamma(std::uint32_t){throw Unsupported("native display gamma readback unavailable");}
void DekoDevice::RestoreGamma(std::uint32_t,const GammaRamp&)noexcept{std::terminate();}
std::uint64_t DekoDevice::AvailableTextureBytes(){return impl_->texture_store->Available();}
std::span<const std::byte> DekoDevice::TextFontAsset(unsigned id){return text_backend::native::OriginalAsset(id);}
std::shared_ptr<native_surfaces::Session> DekoDevice::SurfaceSession(){return impl_->surface_session;}
std::shared_ptr<native_texture::Session> DekoDevice::TextureSession(){return impl_->texture_session;}
std::shared_ptr<debug_font::native::Session> DekoDevice::DebugFontSession(){return impl_->Fonts();}
namespace {
class TargetAndRenderLease final:public DeviceStateLease {
    std::unique_ptr<DeviceStateLease> targets_,render_;
public:
    TargetAndRenderLease(std::unique_ptr<DeviceStateLease> targets,std::unique_ptr<DeviceStateLease> render):targets_(std::move(targets)),render_(std::move(render)){}
    DeviceResult Restore()noexcept override{const auto result=targets_->Restore();return result?result:render_->Restore();}
};
}
std::unique_ptr<DeviceStateLease> DekoDevice::CaptureState(){
    if(!impl_->presentation_state.Ready())throw Unsupported("capture requires live native presentation");
    auto targets=impl_->surface_session->Capture();auto render=impl_->state.Capture();
    return std::make_unique<presentation::RestoreLease>(impl_->lifetime,impl_->Targets().value,std::make_unique<TargetAndRenderLease>(std::move(targets),std::move(render)));
}
std::size_t DekoDevice::OutstandingStateLeases()const{return impl_->state.Outstanding();}
}
