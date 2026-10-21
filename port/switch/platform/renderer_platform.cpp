#include "renderer_platform.hpp"
#include "e2e_clock.hpp"
#include "e2e_stage.hpp"
#include "e2e5_stage_profiler.hpp"
#include "e2e_threads.hpp"
#include "e2e515_perf.hpp"
#include "units/GI_MessageLoop.hpp"
#if defined(E2E5_HOST_SDL_WINDOW)
#include "e2e3_generation_state.hpp"
#include "e2e515_host_frame_capture.hpp"
#include "e2e516_map_probe.hpp"
#include <array>
#include <cstdlib>
#endif

#include "units/GR_DX.hpp"
#include "units/GR_GraphBuf.hpp"
#include "units/GR_Main.hpp"
#include "units/EC_BlockPar.hpp"
#include "units/EC_Str.hpp"
#include "units/GlobalsV.hpp"
#include "units/System.hpp"
#include "types/aMyFunction.hpp"

#if defined(__SWITCH__)
#include "win32_compat_window.hpp"
#include <SDL2/SDL.h>
#endif

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

namespace srhd_awa::platform::renderer_platform {
namespace {
void* g_native_window{};
bool g_initialized{};
std::uint64_t g_presentation_count{};
std::uint64_t g_last_presentation_hash{};
PresentationDiagnostics g_presentation_diagnostics{};
bool g_presentation_failure_logged{};
struct SceneBackdrop {
  const void* scene;
  std::int32_t depth;
  GR_GraphBuf::TGraphBufGR* buffer;
};
// BGBuf controls borrow these objects and can outlive their Run() invocation.
// Keep their addresses stable until renderer shutdown, unlike frame canvases.
std::vector<SceneBackdrop> g_scene_backdrops;
SceneCanvas* g_active_canvas{};
std::uint64_t g_canvas_generation{};
bool SceneDiagnosticsEnabled() {
#if defined(E2E515_PERF_DIAGNOSTICS) && E2E515_PERF_DIAGNOSTICS
  return perf515::SceneEnabled();
#else
  return false;
#endif
}
void SceneAsyncLog(const char* prefix,const char* text) {
#if defined(E2E515_PERF_DIAGNOSTICS) && E2E515_PERF_DIAGNOSTICS
  perf515::AsyncLog(prefix,text);
#else
  (void)prefix;(void)text;
#endif
}

int SceneId(const void* scene) {
  if(!scene)return -1;
  for(int id=1;id<42;++id)if(GlobalsV::RegisteredScreens[id]==scene)return id;
  return -1;
}
void SceneImage(const char* event,int id,std::uint64_t generation,const void* owner,
                GR_GraphBuf::TGraphBufGR* buffer,std::int64_t turn=-1,
                std::int64_t film=-1,std::int64_t order=-1,std::uint64_t frame=0) {
  if(!SceneDiagnosticsEnabled() || !buffer)return;
  int x=-1000,y=-1000;
  if(GI_MessageLoop::MessageLoopStack && pas::list_count(GI_MessageLoop::MessageLoopStack)) {
    auto* loop=pas::list_at<GI_MessageLoop::TMessageLoopGI>(GI_MessageLoop::MessageLoopStack,
        pas::list_count(GI_MessageLoop::MessageLoopStack)-1);
    if(loop->CursorControl){x=loop->CursorControl->LocalPosition.X;y=loop->CursorControl->LocalPosition.Y;}
  }
#if defined(E2E515_PERF_DIAGNOSTICS) && E2E515_PERF_DIAGNOSTICS
  perf515::SceneImage(event,id,generation,owner,buffer,buffer->GetPixels(),buffer->PitchBytes,
      buffer->Width,buffer->Height,x,y,turn,film,order,frame);
#endif
}
void SceneLog(const char* event,const void* owner,std::uint64_t generation,
              const void* buffer,const void* source,const void* parent,int id) {
  if(!SceneDiagnosticsEnabled())return;
  char stack[240]{};std::size_t offset{};
  const int depth=GI_MessageLoop::MessageLoopStack?pas::list_count(GI_MessageLoop::MessageLoopStack):0;
  for(int i=0;i<depth && i<8;++i) {
    const auto* entry=pas::list_get(GI_MessageLoop::MessageLoopStack,i);
    const int n=std::snprintf(stack+offset,sizeof(stack)-offset,"%s%d@%p",i?"/":"",SceneId(entry),entry);
    if(n<0 || static_cast<std::size_t>(n)>=sizeof(stack)-offset)break;
    offset+=static_cast<std::size_t>(n);
  }
  char line[600]{};
  std::snprintf(line,sizeof(line),"event=%s screen=%d generation=%llu owner=%p buffer=%p source=%p parent=%p depth=%d stack=%s stack_truncated=%d",
      event,id,static_cast<unsigned long long>(generation),owner,buffer,source,parent,
      depth,depth?stack:"empty",int(depth>8));
  SceneAsyncLog("[SCENE] ",line);
}
struct CapturedBackground {
  GR_GraphBuf::TGraphBufGR* buffer{};
  const GR_GraphBuf::TGraphBufGR* source{};
  std::uint64_t source_hash{};
  bool valid{};
} g_captured_background;

#if defined(__SWITCH__) && defined(E2E3_TRACE) && E2E3_TRACE
void CaptureSceneSample(const void* pixels, int pitch, int width, int height) {
  static unsigned counts[256]{};
  unsigned id = GlobalsV::CurrentScreenId;
  if (GI_MessageLoop::MessageLoopStack && pas::list_count(GI_MessageLoop::MessageLoopStack)) {
    const auto top = pas::list_get(GI_MessageLoop::MessageLoopStack,
                                  pas::list_count(GI_MessageLoop::MessageLoopStack) - 1);
    for (unsigned i = 1; i < 42; ++i) if (GlobalsV::RegisteredScreens[i] == top) { id = i; break; }
  }
  const auto count = ++counts[id];
  if (count != 1 && count != 60) return;
  char path[256]{};
  std::snprintf(path, sizeof(path), "%s/logs/scene-%u-%u.ppm", e2e_stage::kRoot, id, count);
  FILE* file = std::fopen(path, "wb");
  if (!file) return;
  bool ok = std::fprintf(file, "P6\n320 180\n255\n") > 0;
  unsigned char row[320 * 3]{};
  for (int y = 0; y < 180 && ok; ++y) {
    const auto* source = reinterpret_cast<const std::uint16_t*>(
        static_cast<const unsigned char*>(pixels) + (y * height / 180) * pitch);
    for (int x = 0; x < 320; ++x) {
      const auto color = source[x * width / 320];
      row[x * 3] = ((color >> 11) & 31) * 255 / 31;
      row[x * 3 + 1] = ((color >> 5) & 63) * 255 / 63;
      row[x * 3 + 2] = (color & 31) * 255 / 31;
    }
    ok = std::fwrite(row, 1, sizeof(row), file) == sizeof(row);
  }
  if (std::fclose(file)) ok = false;
  char detail[320]{};
  std::snprintf(detail, sizeof(detail), "scene sample %s path=%s", ok ? "written" : "failed", path);
  e2e_stage::Log(detail);
}
#endif

#if defined(__SWITCH__)
SDL_Renderer* g_sdl_renderer{};
SDL_Texture* g_sdl_texture{};
std::int32_t g_texture_width{};
std::int32_t g_texture_height{};
#if defined(E2E_DEBUG_CURSOR_OVERLAY) && E2E_DEBUG_CURSOR_OVERLAY
bool g_cursor_overlay_logged{};
#endif
std::uint32_t g_rate_start_tick{};
std::uint64_t g_rate_start_count{};
std::uint64_t g_present_us_sum{};
std::uint64_t g_present_us_max{};
std::uint32_t g_last_hash_tick{};

#if defined(E2E_DEBUG_CURSOR_OVERLAY) && E2E_DEBUG_CURSOR_OVERLAY
bool DrawInputCursor(const SDL_Rect& destination, std::int32_t width,
                     std::int32_t height) {
  std::int32_t input_x{}, input_y{};
  win32_compat::InputCursor(&input_x, &input_y);
  const int x = destination.x + input_x * destination.w / width;
  const int y = destination.y + input_y * destination.h / height;
  // Explicit diagnostic overlay. The original game cursor is drawn in RGB565.
  const SDL_Rect shadow_vertical{x - 2, y - 12, 5, 25};
  const SDL_Rect shadow_horizontal{x - 12, y - 2, 25, 5};
  const SDL_Rect pointer_vertical{x, y - 10, 1, 21};
  const SDL_Rect pointer_horizontal{x - 10, y, 21, 1};
  const SDL_Rect pointer_center{x - 1, y - 1, 3, 3};
  if (SDL_SetRenderDrawColor(g_sdl_renderer, 0, 0, 0, 255) != 0 ||
      SDL_RenderFillRect(g_sdl_renderer, &shadow_vertical) != 0 ||
      SDL_RenderFillRect(g_sdl_renderer, &shadow_horizontal) != 0 ||
      SDL_SetRenderDrawColor(g_sdl_renderer, 255, 230, 0, 255) != 0 ||
      SDL_RenderFillRect(g_sdl_renderer, &pointer_vertical) != 0 ||
      SDL_RenderFillRect(g_sdl_renderer, &pointer_horizontal) != 0 ||
      SDL_RenderFillRect(g_sdl_renderer, &pointer_center) != 0)
    return false;
  if (!g_cursor_overlay_logged) {
    e2e_stage::Log("controller cursor overlay active");
    g_cursor_overlay_logged = true;
  }
  return true;
}
#endif
#endif

bool ValidDimensions(std::int32_t width, std::int32_t height, std::int32_t pitch) {
  if (width <= 0 || height <= 0 || width > 16384 || height > 16384 ||
      pitch < width * 2) return false;
  return static_cast<std::uint64_t>(pitch) * static_cast<std::uint64_t>(height) <=
         static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max());
}

bool PresentationFailure(const char* reason) {
#if defined(__SWITCH__)
  if (!g_presentation_failure_logged) {
    char detail[384]{};
    std::snprintf(detail, sizeof(detail), "present failed reason=%s sdl=%s",
                  reason, SDL_GetError());
    e2e_stage::Log(detail);
    g_presentation_failure_logged = true;
  }
#else
  static_cast<void>(reason);
#endif
  return false;
}

std::uint64_t HashRgb565(const void* pixels, std::int32_t pitch, std::int32_t width,
                         std::int32_t height) {
  const auto* rows = static_cast<const std::uint8_t*>(pixels);
  // Legacy project fingerprint basis, retained for Mxx hardware evidence.
  // Standard FNV-1a-64 starts at 14695981039346656037.
  std::uint64_t value = UINT64_C(1469598103934665603);
  for (std::int32_t y = 0; y < height; ++y) {
    for (std::int32_t x = 0; x < width * 2; ++x) {
      value ^= rows[y * pitch + x];
      value *= UINT64_C(1099511628211);
    }
  }
  return value;
}

void ClearSoftwareGlobals() {
  pas::free(g_captured_background.buffer);
  g_captured_background = {};
  for (const auto& backdrop : g_scene_backdrops) pas::free(backdrop.buffer);
  g_scene_backdrops.clear();
  GR_Main::FreeScreenRenderBuffers();
  pas::free(GR_Main::CurrentPixelFormat);
  GR_Main::CurrentPixelFormat = nullptr;
  GR_DX::PendingPoints = nullptr;
  GR_DX::PendingPointCount = 0;
  GR_DX::PendingPointCapacity = 0;
  GR_Main::GameScreenWidth = 0;
  GR_Main::GameScreenHeight = 0;
  GR_Main::ExtraScreenWidth = 0;
  GR_Main::ExtraScreenHeight = 0;
  GR_Main::PresentationWidth = 0;
  GR_Main::PresentationHeight = 0;
}
}  // namespace

void SetNativeWindow(void* native_window) { g_native_window = native_window; }

void PreserveCapturedBackground() {
  auto& captured = g_captured_background;
  auto* source = GR_Main::ScreenRenderBuffer;
  auto* prepared = GR_Main::AuxRenderBuffer;
  auto* owner = g_active_canvas;
  if (owner && source != owner->canvas_) return;
  auto& ready = owner ? owner->captured_ready_ : captured.valid;
  ready = false;
  if (!source || !prepared || !source->GetPixels() || !prepared->GetPixels() ||
      source->Width != prepared->Width || source->Height != prepared->Height) return;
  auto& buffer = owner ? owner->captured_ : captured.buffer;
  if (!buffer && owner && !captured.valid) {
    buffer = captured.buffer;
    captured.buffer = nullptr;
  }
  if (!buffer)
    buffer = pas::construct_call<GR_GraphBuf::TGraphBufGR>(GR_GraphBuf::TGraphBufGR_Create, false);
  buffer->AllocateNativePitch(prepared->Width, prepared->Height, prepared->PitchBytes);
  GR_Main::Ex_OKGR_Copy_XY_XY_WORD(buffer->GetPixels(), buffer->PitchBytes, 0, 0,
      prepared->GetPixels(), prepared->PitchBytes, 0, 0, prepared->Width, prepared->Height);
  const auto source_hash = HashRgb565(source->GetPixels(), source->PitchBytes, source->Width, source->Height);
  if (owner) owner->captured_source_hash_ = source_hash;
  else { captured.source = source; captured.source_hash = source_hash; }
  ready = true;
  if(owner) {
    SceneLog("background-capture",owner->scene_,owner->generation_,buffer,source,owner->parent_,owner->screen_id_);
    SceneImage("background-capture",owner->screen_id_,owner->generation_,owner->scene_,prepared);
  }
  char detail[224]{};
  std::snprintf(detail, sizeof(detail), "scene background captured id=%u width=%d height=%d source_hash=%016llx prepared_hash=%016llx",
      static_cast<unsigned>(GlobalsV::CurrentScreenId), prepared->Width, prepared->Height,
      static_cast<unsigned long long>(source_hash),
      static_cast<unsigned long long>(HashRgb565(prepared->GetPixels(), prepared->PitchBytes, prepared->Width, prepared->Height)));
  e2e_stage::Log(detail);
}

SceneCanvas::SceneCanvas(bool inherit_background, const void* scene)
    : parent_(GR_Main::ScreenRenderBuffer), parent_aux_(GR_Main::AuxRenderBuffer),
      parent_scope_(g_active_canvas), started_ms_(e2e_clock::Microseconds() / 1000) {
  if (!parent_) return;
  scene_=scene;screen_id_=SceneId(scene);generation_=++g_canvas_generation;
  nested_ = GI_MessageLoop::MessageLoopStack &&
      pas::list_count(GI_MessageLoop::MessageLoopStack) != 0;
  auto* prepared = nested_ && parent_scope_ && parent_scope_->captured_ready_ ? parent_scope_->captured_ :
      (!nested_ && g_captured_background.valid && g_captured_background.source == parent_ ? g_captured_background.buffer : nullptr);
  const bool use_prepared = inherit_background && prepared &&
      prepared->Width == parent_->Width && prepared->Height == parent_->Height;
  // Capture freezes an intentional image. Cursor updates and save-preview
  // rendering may change this same scene afterwards; they do not revoke it.
  // Entry to another independent scene and child exit invalidate ownership.
  canvas_ = pas::construct_call<GR_GraphBuf::TGraphBufGR>(
      GR_GraphBuf::TGraphBufGR_Create, false);
  try {
    canvas_->AllocateNativePitch(parent_->Width, parent_->Height, parent_->PitchBytes);
    if (inherit_background) {
      GR_Main::Ex_OKGR_Copy_XY_XY_WORD(canvas_->GetPixels(), canvas_->PitchBytes, 0, 0,
          parent_->GetPixels(), parent_->PitchBytes, 0, 0, parent_->Width, parent_->Height);
    } else {
      canvas_->FillPixels16(0);
    }
  } catch (...) {
    pas::free(canvas_);
    throw;
  }
  const auto depth = GI_MessageLoop::MessageLoopStack ? pas::list_count(GI_MessageLoop::MessageLoopStack) : 0;
  GR_GraphBuf::TGraphBufGR* aux{};
  for (const auto& entry : g_scene_backdrops)
    if (entry.scene == scene && entry.depth == depth) { aux = entry.buffer; break; }
  try {
    if (!aux) {
      aux = pas::construct_call<GR_GraphBuf::TGraphBufGR>(GR_GraphBuf::TGraphBufGR_Create, false);
      try { g_scene_backdrops.push_back({scene, depth, aux}); }
      catch (...) { pas::free(aux); throw; }
    }
    if (inherit_background) {
      aux->AllocateNativePitch(parent_->Width, parent_->Height, parent_->PitchBytes);
      auto* backdrop = use_prepared ? prepared : parent_;
      GR_Main::Ex_OKGR_Copy_XY_XY_WORD(aux->GetPixels(), aux->PitchBytes, 0, 0,
          backdrop->GetPixels(), backdrop->PitchBytes, 0, 0, parent_->Width, parent_->Height);
    } else aux->Clear();
  } catch (...) { pas::free(canvas_); canvas_ = nullptr; throw; }
  GR_Main::AuxRenderBuffer = aux;
  if (!nested_) g_captured_background.valid = false;
  GR_Main::ScreenRenderBuffer = canvas_;
  g_active_canvas = this;
  SceneLog("scene-enter",scene_,generation_,canvas_,use_prepared?prepared:parent_,parent_scope_?parent_scope_->scene_:nullptr,screen_id_);
  SceneImage("scene-enter",screen_id_,generation_,scene_,canvas_);
  if(inherit_background)SceneImage("background-restore",screen_id_,generation_,scene_,aux);
  GR_Main::FullFrameRedrawRequested = true;
  char detail[160]{};
  std::snprintf(detail, sizeof(detail), "scene canvas id=%u inherit=%u nested=%u aux_width=%d aux_height=%d prepared=%u",
      static_cast<unsigned>(GlobalsV::CurrentScreenId), static_cast<unsigned>(inherit_background),
      static_cast<unsigned>(nested_), aux->Width, aux->Height, static_cast<unsigned>(use_prepared));
  e2e_stage::Log(detail);
}

SceneCanvas::~SceneCanvas() {
  if (!canvas_) return;
  SceneLog("scene-exit",scene_,generation_,canvas_,nullptr,parent_scope_?parent_scope_->scene_:nullptr,screen_id_);
  // A sequential overlay may capture the last completed scene in OnOpen.
  // Nested windows instead reveal their unchanged, still-running parent.
  if (!nested_) {
    GR_Main::Ex_OKGR_Copy_XY_XY_WORD(parent_->GetPixels(), parent_->PitchBytes, 0, 0,
        canvas_->GetPixels(), canvas_->PitchBytes, 0, 0, canvas_->Width, canvas_->Height);
  }
  if (!nested_) {
    // Transfer the closing scene's frozen image past OnClose/Clear. Nested
    // previews keep their own captures and never consume/replace their parent's.
    g_captured_background.valid = captured_ready_;
    if (captured_ready_) {
      pas::free(g_captured_background.buffer);
      g_captured_background.buffer = captured_;
      captured_ = nullptr;
      g_captured_background.source = parent_;
      g_captured_background.source_hash = captured_source_hash_;
    }
  }
  pas::free(captured_);
  GR_Main::ScreenRenderBuffer = parent_;
  GR_Main::AuxRenderBuffer = parent_aux_;
  GR_Main::FullFrameRedrawRequested = true;
  if(parent_scope_) {
    parent_scope_->pending_samples_=1;
    SceneLog("background-restore",parent_scope_->scene_,parent_scope_->generation_,parent_,parent_,scene_,parent_scope_->screen_id_);
  }
  pas::free(canvas_);
  g_active_canvas = parent_scope_;
  if (parent_scope_) parent_scope_->nested_millis_ += e2e_clock::Microseconds() / 1000 - started_ms_;
}

void SceneFrameBegin(const void* scene,std::int64_t turn,std::int64_t film,std::int64_t order) {
  if(!SceneDiagnosticsEnabled() || !g_active_canvas || g_active_canvas->scene_!=scene)return;
  auto& c=*g_active_canvas;
  // Battle diagnostics remain independent of this bounded scene experiment.
  if(c.screen_id_==21 || c.screen_id_==40)return;
  ++c.frames_;
  const bool state_changed=c.screen_id_==16 && (c.turn_!=turn || c.order_!=order);
  c.turn_=turn;c.film_=film;c.order_=order;
  if(state_changed)SceneAction("map-state-observed",scene,turn,order);
  c.sample_frame_=c.frames_==1 || c.frames_==60 || c.pending_samples_;
  if(c.pending_samples_)--c.pending_samples_;
  if(c.sample_frame_) {
    const auto* loop=static_cast<const GI_MessageLoop::TMessageLoopGI*>(scene);
    char line[320]{};
    std::snprintf(line,sizeof(line),"event=frame-compose screen=%d generation=%llu frame=%llu owner=%p buffer=%p aux=%p saved_pixels=%d saved_lines=%d",
        c.screen_id_,static_cast<unsigned long long>(c.generation_),static_cast<unsigned long long>(c.frames_),scene,
        GR_Main::ScreenRenderBuffer,GR_Main::AuxRenderBuffer,loop->SavedPixelCount16,loop->SavedLineCount);
    SceneAsyncLog("[SCENE] ",line);
    SceneImage("compose-before",c.screen_id_,c.generation_,c.scene_,GR_Main::ScreenRenderBuffer,turn,film,order,c.frames_);
  }
}
void SceneFrameEnd(const void* scene) {
  if(!SceneDiagnosticsEnabled() || !g_active_canvas || g_active_canvas->scene_!=scene)return;
  auto& c=*g_active_canvas;
  if(c.sample_frame_)SceneImage("compose-after",c.screen_id_,c.generation_,c.scene_,GR_Main::ScreenRenderBuffer,c.turn_,c.film_,c.order_,c.frames_);
}
void SceneAction(const char* event,const void* scene,std::int64_t turn,std::int64_t value) {
  if(!SceneDiagnosticsEnabled() || !g_active_canvas || g_active_canvas->scene_!=scene)return;
  auto& c=*g_active_canvas;c.pending_samples_=2;
  char line[320]{};
  std::snprintf(line,sizeof(line),"event=%s screen=%d generation=%llu owner=%p turn=%lld value=%lld",
      event,c.screen_id_,static_cast<unsigned long long>(c.generation_),scene,
      static_cast<long long>(turn),static_cast<long long>(value));
  SceneAsyncLog("[SCENE] ",line);
}
void SceneCache(const char* event,const void* scene,const void* control,
                GR_GraphBuf::TGraphBufGR* buffer,bool alpha,bool borrowed) {
  if(!SceneDiagnosticsEnabled() || !g_active_canvas || !buffer)return;
  const auto& c=*g_active_canvas;
  if(event[0]=='d' && !c.sample_frame_)return;
  const void* source_owner=nullptr;int source_depth=-1;
  for(const auto& entry:g_scene_backdrops)if(entry.buffer==buffer){source_owner=entry.scene;source_depth=entry.depth;break;}
  char line[512]{};
  std::snprintf(line,sizeof(line),"event=ui-cache-%s screen=%d generation=%llu frame=%llu owner=%p control=%p control_loop=%p buffer=%p source_owner=%p source_depth=%d width=%d height=%d pitch=%d bpp=%d alpha=%d borrowed=%d",
      event,c.screen_id_,static_cast<unsigned long long>(c.generation_),static_cast<unsigned long long>(c.frames_),c.scene_,control,scene,
      buffer,source_owner,source_depth,buffer->Width,buffer->Height,buffer->PitchBytes,buffer->BitsPerPixel,int(alpha),int(borrowed));
  SceneAsyncLog("[SCENE] ",line);
}

std::uint64_t NestedLoopMillis() { return g_active_canvas ? g_active_canvas->NestedMillis() : 0; }

bool InitializeSoftwareRenderer(const RendererConfig& config, std::string* error) {
  if (g_initialized) {
    if (error) *error = "software renderer is already initialized";
    return false;
  }
  if (config.game_width <= 0 || config.game_height <= 0 || config.game_width > 16384 ||
      config.game_height > 16384 ||
      !ValidDimensions(config.game_width, config.game_height, config.game_width * 2) ||
      config.minimap_buffer_size <= 0 || config.minimap_buffer_size > 4096) {
    if (error) *error = "invalid software renderer dimensions";
    return false;
  }
  ClearSoftwareGlobals();
  GR_Main::GameScreenWidth = config.game_width;
  GR_Main::GameScreenHeight = config.game_height;
  // Mirror the original display-mode setup before UI InitializeLayout runs.
  GR_Main::ExtraScreenWidth = config.game_width - 1024;
  GR_Main::ExtraScreenHeight = config.game_height - 768;
  GR_Main::PresentationWidth = config.presentation_width > 0 ? config.presentation_width : config.game_width;
  GR_Main::PresentationHeight = config.presentation_height > 0 ? config.presentation_height : config.game_height;
  // GR_DXInit normally enumerates these modes.  The software renderer bypasses
  // that path, but Options still indexes the selected mode during OnOpen.
  const GR_Main::TDisplayModeGR fixed_mode{
      static_cast<std::uint32_t>(config.game_width),
      static_cast<std::uint32_t>(config.game_height), 60u, 0u};
  GR_Main::DesktopDisplayMode = fixed_mode;
  GR_Main::GameDisplayModes.set_length(1);
  GR_Main::RobotDisplayModes.set_length(1);
  GR_Main::GameDisplayModes[0] = fixed_mode;
  GR_Main::RobotDisplayModes[0] = fixed_mode;
  GR_Main::GameDisplayModeCount = 1;
  GR_Main::RobotDisplayModeCount = 1;
  GR_Main::SelectedGameDisplayMode = 0;
  GR_Main::SelectedRobotDisplayMode = 0;
  GR_Main::SmallestGameDisplayMode = 0;
#if defined(__SWITCH__)
  {
    char detail[128]{};
    std::snprintf(detail, sizeof(detail), "renderer geometry game=%dx%d presentation=%dx%d extra=%d,%d",
        GR_Main::GameScreenWidth, GR_Main::GameScreenHeight,
        GR_Main::PresentationWidth, GR_Main::PresentationHeight,
        GR_Main::ExtraScreenWidth, GR_Main::ExtraScreenHeight);
    e2e_stage::Log(detail);
  }
#endif
  GR_Main::GameScreenRect = {0, 0, config.game_width, config.game_height};
  GR_Main::PresentationRect = {0, 0, GR_Main::PresentationWidth, GR_Main::PresentationHeight};
  GR_Main::VSyncEnabled = config.vsync;
  GR_Main::PresentWithoutLimit = !config.frame_limit;
  GR_Main::PresentationFrameRate = 60;
  GlobalsV::ScaleViewportToWindow = config.scale_viewport;
  // The Switch backend is software-only, but keeps the user request for diagnostics.
  GlobalsV::HardwareRenderingEnabled = false;

  GR_Main::CurrentPixelFormat = pas::make_object<GR_GraphBuf::TPixelFormatGR>();
  GR_Main::CurrentPixelFormat->RedMask = 0xf800u;
  GR_Main::CurrentPixelFormat->GreenMask = 0x07e0u;
  GR_Main::CurrentPixelFormat->BlueMask = 0x001fu;
  GR_Main::CurrentPixelFormat->AlphaMask = 0u;
  GR_Main::CurrentPixelFormat->BytesPerPixel = 2;
  GR_Main::CurrentPixelFormat->RebuildChannelMetrics();
  GR_Main::ScreenRenderBuffer = pas::construct_call<GR_GraphBuf::TGraphBufGR>(
      GR_GraphBuf::TGraphBufGR_Create, false);
  GR_Main::RenderScratchBuffer = pas::construct_call<GR_GraphBuf::TGraphBufGR>(
      GR_GraphBuf::TGraphBufGR_Create, false);
  GR_Main::AuxRenderBuffer = pas::construct_call<GR_GraphBuf::TGraphBufGR>(
      GR_GraphBuf::TGraphBufGR_Create, false);
  GR_Main::ScreenRenderBuffer->AllocateNativePitch(config.game_width, config.game_height,
                                                     config.game_width * 2);
  GR_Main::RenderScratchBuffer->AllocateNative(config.minimap_buffer_size, config.minimap_buffer_size);
  GR_Main::AuxRenderBuffer->AllocateNative(config.minimap_buffer_size, config.minimap_buffer_size);
  GR_Main::BlendPixel16 = GR_Main::TBlendPixel16(GR_Main::OKGR_PixelAlpha_16);
  GR_Main::TriangleRasterizer16 = GR_Main::TTriangleRasterizer16(GR_Main::OKGF_Triangle_16);
  GR_Main::LineRasterizer16 = GR_Main::TLineRasterizer16(GR_Main::OKGF_LineIp_16);
  for (std::int32_t index = 0; index <= 360; ++index) {
    const float angle = index * pas::constant(aMyFunction::GamePi / 1.8E+2L);
    GR_DX::CircleCos[index] = System::Cos(angle);
    GR_DX::CircleSin[index] = System::Sin(angle);
  }
  for (std::int32_t index = 0; index <= 359; ++index)
    GR_DX::LineAlphaTable[index] = System::Trunc(System::Cos(
        pas::real_divide(index, 1.8E+2L) * 3.14159265354L) * 127.0L + 128.0L);
  GR_DX::PendingPointCapacity = 1024;
  GR_DX::PendingPoints.set_length(GR_DX::PendingPointCapacity);
  g_initialized = true;
  return true;
}

bool InitializeReleaseCompatibleDefaults(std::string* error) {
  RendererConfig config;
  if (GR_Main::GameScreenWidth > 0) config.game_width = GR_Main::GameScreenWidth;
  if (GR_Main::GameScreenHeight > 0) config.game_height = GR_Main::GameScreenHeight;
  if (GR_Main::PresentationWidth > 0) config.presentation_width = GR_Main::PresentationWidth;
  if (GR_Main::PresentationHeight > 0) config.presentation_height = GR_Main::PresentationHeight;
  config.vsync = GR_Main::VSyncEnabled;
  config.scale_viewport = GlobalsV::ScaleViewportToWindow;
  config.frame_limit = !GR_Main::PresentWithoutLimit;
#if defined(__SWITCH__)
  if (GR_Main::GameDataConfig && GR_Main::GameDataConfig->CountParams(u"MiniMapBufSize"_wref.get()) > 0) {
    const auto size = EC_Str::ExtractDigitsToIntW(pas::view(GR_Main::GameDataConfig->GetParam(u"MiniMapBufSize"sv)));
    if (size > 0 && size <= 4096) config.minimap_buffer_size = size;
  }
#endif
  return InitializeSoftwareRenderer(config, error);
}

bool ApplyVSyncSetting(bool enabled) {
#if defined(__SWITCH__)
  if (g_sdl_renderer && SDL_RenderSetVSync(g_sdl_renderer, enabled ? 1 : 0) != 0)
    return PresentationFailure("SDL_RenderSetVSync");
#else
  (void)enabled;
#endif
  return true;
}

bool PresentRgb565(const void* pixels, std::int32_t pitch, std::int32_t width, std::int32_t height) {
  using e2e5_stage_profiler::Window;
  e2e5_stage_profiler::SetScreen(GlobalsV::CurrentScreenId);
  static Window presentation_profile{"present"};
  static std::int32_t last_screen = -1;
  static std::uint64_t previous_present_us{};
#if defined(__SWITCH__)
  if (!g_presentation_count) {
    e2e_threads::LogPriority("game-present", threadGetCurHandle(), -1, 0);
    u32 cpu_hz{};
    const Result clock_init = clkrstInitialize();
    Result clock_read = clock_init;
    if (R_SUCCEEDED(clock_init)) {
      ClkrstSession session{};
      clock_read = clkrstOpenSession(&session, PcvModuleId_CpuBus, 3);
      if (R_SUCCEEDED(clock_read)) {
        clock_read = clkrstGetClockRate(&session, &cpu_hz);
        clkrstCloseSession(&session);
      }
      clkrstExit();
    }
    char detail[192]{};
    std::snprintf(detail, sizeof(detail),
        "runtime applet_type=%u game_core=%d cpu_hz=%u clock_result=%u",
        static_cast<unsigned>(appletGetAppletType()), svcGetCurrentProcessorNumber(),
        cpu_hz, clock_read);
    e2e_stage::Log(detail);
  }
#endif
  const auto present_start_us = e2e_clock::Microseconds();
  if (!g_initialized) return PresentationFailure("renderer not initialized");
  if (!pixels) return PresentationFailure("pixels missing");
  if (!ValidDimensions(width, height, pitch)) return PresentationFailure("invalid dimensions");
#if defined(E2E5_HOST_SDL_WINDOW)
  if(GlobalsV::CurrentScreenId==16)map516::Present(pixels,pitch,width,height);
#endif
#if defined(E2E515_PERF_DIAGNOSTICS) && E2E515_PERF_DIAGNOSTICS
  if(perf515::SceneEnabled() && g_active_canvas && g_active_canvas->sample_frame_) {
    auto& c=*g_active_canvas;
    // This is the exact CPU input of SDL_UpdateTexture below, not GPU readback.
    perf515::SceneImage("present-input",c.screen_id_,c.generation_,c.scene_,GR_Main::ScreenRenderBuffer,
        pixels,pitch,width,height,-1000,-1000,c.turn_,c.film_,c.order_,c.frames_);
    c.sample_frame_=false;
  }
#endif
#if defined(E2E5_HOST_SDL_WINDOW)
  // Explicit opt-in diagnostic capture, always from the game/UI thread.
  if (const auto* directory = std::getenv("E2E511_HOST_FRAMES")) {
    static unsigned frame_counts[256]{};
    auto screen = static_cast<unsigned>(GlobalsV::CurrentScreenId);
    bool registered_owner = false;
    if (GI_MessageLoop::MessageLoopStack && pas::list_count(GI_MessageLoop::MessageLoopStack)) {
      const auto top = pas::list_get(GI_MessageLoop::MessageLoopStack,
                                    pas::list_count(GI_MessageLoop::MessageLoopStack) - 1);
      for (unsigned id = 1; id < 42; ++id)
        if (GlobalsV::RegisteredScreens[id] == top) { screen = id; registered_owner = true; break; }
    }
    const auto frame = ++frame_counts[screen];
    if (std::getenv("E2E515_HOST_FRAME_CAPTURE")) {
      host_frame_capture::Requested(directory, screen, pixels, pitch, width, height,
          GI_MessageLoop::MessageLoopStack ? pas::list_count(GI_MessageLoop::MessageLoopStack) : 0,
          registered_owner,
          GR_Main::AuxRenderBuffer ? GR_Main::AuxRenderBuffer->Width : 0,
          GR_Main::AuxRenderBuffer ? GR_Main::AuxRenderBuffer->Height : 0,
          HashRgb565(pixels, pitch, width, height));
    }
    if (frame == 1 || frame == 60) {
      char path[1024]{};
      std::snprintf(path, sizeof(path), "%s/screen-%u-%u.ppm", directory, screen, frame);
      if (FILE* file = std::fopen(path, "wb")) {
        std::fprintf(file, "P6\n%d %d\n255\n", width, height);
        for (int y = 0; y < height; ++y) {
          const auto* row = reinterpret_cast<const std::uint16_t*>(
              static_cast<const unsigned char*>(pixels) + y * pitch);
          for (int x = 0; x < width; ++x) {
            const unsigned char rgb[]{static_cast<unsigned char>(((row[x] >> 11) & 31) * 255 / 31),
              static_cast<unsigned char>(((row[x] >> 5) & 63) * 255 / 63),
              static_cast<unsigned char>((row[x] & 31) * 255 / 31)};
            std::fwrite(rgb, 1, 3, file);
          }
        }
        std::fclose(file);
      }
    }
  }
#endif
#if defined(__SWITCH__) && defined(E2E_DIAGNOSTICS) && !E2E_DIAGNOSTICS
  const auto hash_tick = e2e_clock::Milliseconds();
  if (!g_presentation_count || last_screen != GlobalsV::CurrentScreenId ||
      static_cast<std::uint32_t>(hash_tick - g_last_hash_tick) >= 5000u) {
    g_last_presentation_hash = HashRgb565(pixels, pitch, width, height);
    g_last_hash_tick = hash_tick;
  }
#else
  g_last_presentation_hash = HashRgb565(pixels, pitch, width, height);
#endif
#if defined(E2E5_HOST_SDL_WINDOW)
  static std::array<unsigned, 64> ui_capture_frames{};
  const auto capture_screen = static_cast<unsigned>(GlobalsV::CurrentScreenId);
  if (std::getenv("E2E5_HOST_CAPTURE_UI") && capture_screen < ui_capture_frames.size() &&
      ++ui_capture_frames[capture_screen] == 60) {
    char name[80]{};
    std::snprintf(name, sizeof(name), "ui-screen-%u.ppm", capture_screen);
    if (FILE* output = std::fopen(name, "wb")) {
      std::fprintf(output, "P6\n%d %d\n255\n", width, height);
      std::array<unsigned char, 16384 * 3> row{};
      for (int y = 0; y < height; ++y) {
        auto* source = reinterpret_cast<const std::uint16_t*>(
            static_cast<const unsigned char*>(pixels) + y * pitch);
        for (int x = 0; x < width; ++x) {
          row[x * 3] = ((source[x] >> 11) & 31) * 255 / 31;
          row[x * 3 + 1] = ((source[x] >> 5) & 63) * 255 / 63;
          row[x * 3 + 2] = (source[x] & 31) * 255 / 31;
        }
        std::fwrite(row.data(), 3, width, output);
      }
      std::fclose(output);
    }
  }
  static bool intro_captured = false;
  if (!intro_captured && GlobalsV::CurrentScreenId == 3 &&
      std::getenv("E2E511_HOST_CAPTURE_INTRO") &&
      e2e3_generation_state::g_host_continue_ready.load()) {
    // Capture the actual original UI framebuffer in this isolated host run.
    // No resource files are altered and no image is included in CI artifacts.
    if (FILE* output = std::fopen("intro-ready.ppm", "wb")) {
      std::fprintf(output, "P6\n%d %d\n255\n", width, height);
      std::array<unsigned char, 16384 * 3> row{};
      for (int y = 0; y < height; ++y) {
        auto* source = reinterpret_cast<const std::uint16_t*>(
            static_cast<const unsigned char*>(pixels) + y * pitch);
        for (int x = 0; x < width; ++x) {
          row[x * 3] = ((source[x] >> 11) & 31) * 255 / 31;
          row[x * 3 + 1] = ((source[x] >> 5) & 63) * 255 / 63;
          row[x * 3 + 2] = (source[x] & 31) * 255 / 31;
        }
        std::fwrite(row.data(), 3, width, output);
      }
      const bool ok = !std::ferror(output);
      std::fclose(output);
      intro_captured = ok;
      e2e_stage::Log(ok ? "HOST introduction framebuffer captured intro-ready.ppm" :
                         "HOST introduction framebuffer capture FAILED");
    }
  }
#endif
  presentation_profile.Measure(40, e2e_clock::Microseconds() - present_start_us);
  g_presentation_diagnostics = {};
#if defined(__SWITCH__)
  if (!g_native_window) return PresentationFailure("SDL window missing");
  if (!g_sdl_renderer) {
    g_sdl_renderer = SDL_CreateRenderer(static_cast<SDL_Window*>(g_native_window), -1,
        SDL_RENDERER_ACCELERATED |
        (GR_Main::VSyncEnabled ? SDL_RENDERER_PRESENTVSYNC : 0));
    if (!g_sdl_renderer) return PresentationFailure("SDL_CreateRenderer");
  }
  g_presentation_diagnostics.renderer_ready = g_sdl_renderer != nullptr;
  if (!g_sdl_texture || g_texture_width != width || g_texture_height != height) {
    if (g_sdl_texture) SDL_DestroyTexture(g_sdl_texture);
    g_sdl_texture = SDL_CreateTexture(g_sdl_renderer, SDL_PIXELFORMAT_RGB565,
                                      SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!g_sdl_texture) return PresentationFailure("SDL_CreateTexture");
    g_texture_width = width;
    g_texture_height = height;
  }
  g_presentation_diagnostics.texture_ready = g_sdl_texture != nullptr;
  const auto upload_start_us = e2e_clock::Microseconds();
  if (SDL_UpdateTexture(g_sdl_texture, nullptr, pixels, pitch) != 0)
    return PresentationFailure("SDL_UpdateTexture");
  presentation_profile.Measure(41, e2e_clock::Microseconds() - upload_start_us);
  const auto copy_start_us = e2e_clock::Microseconds();
  int output_width{};
  int output_height{};
  if (SDL_GetRendererOutputSize(g_sdl_renderer, &output_width, &output_height) != 0 ||
      output_width <= 0 || output_height <= 0)
    return PresentationFailure("SDL_GetRendererOutputSize");
  const float scale_x = static_cast<float>(output_width) / static_cast<float>(width);
  const float scale_y = static_cast<float>(output_height) / static_cast<float>(height);
  const float scale = scale_x < scale_y ? scale_x : scale_y;
  const int destination_width = static_cast<int>(static_cast<float>(width) * scale);
  const int destination_height = static_cast<int>(static_cast<float>(height) * scale);
  const SDL_Rect destination{
      (output_width - destination_width) / 2,
      (output_height - destination_height) / 2,
      destination_width,
      destination_height,
  };
  g_presentation_diagnostics.output_width = output_width;
  g_presentation_diagnostics.output_height = output_height;
  g_presentation_diagnostics.destination_x = destination.x;
  g_presentation_diagnostics.destination_y = destination.y;
  g_presentation_diagnostics.destination_width = destination.w;
  g_presentation_diagnostics.destination_height = destination.h;
  if (SDL_SetRenderDrawColor(g_sdl_renderer, 0, 0, 0, 255) != 0 ||
      SDL_RenderClear(g_sdl_renderer) != 0)
    return PresentationFailure("SDL_RenderClear");
  if (SDL_RenderCopy(g_sdl_renderer, g_sdl_texture, nullptr, &destination) != 0)
    return PresentationFailure("SDL_RenderCopy");
  presentation_profile.Measure(42, e2e_clock::Microseconds() - copy_start_us);
#if defined(E2E_DEBUG_CURSOR_OVERLAY) && E2E_DEBUG_CURSOR_OVERLAY
  if (!DrawInputCursor(destination, width, height))
    return PresentationFailure("controller cursor overlay");
#endif
  const auto swap_start_us = e2e_clock::Microseconds();
  SDL_RenderPresent(g_sdl_renderer);
  presentation_profile.Measure(43, e2e_clock::Microseconds() - swap_start_us);
  g_presentation_diagnostics.present_succeeded = true;
  const auto present_us = e2e_clock::Microseconds() - present_start_us;
  g_present_us_sum += present_us;
  g_present_us_max = std::max(g_present_us_max, present_us);
#else
  g_presentation_diagnostics.present_succeeded = true;
#endif
  ++g_presentation_count;
#if defined(E2E515_PERF_DIAGNOSTICS) && E2E515_PERF_DIAGNOSTICS
  perf515::Frame(GlobalsV::CurrentScreenId);
#endif
#if defined(__SWITCH__) && defined(E2E3_TRACE) && E2E3_TRACE
  CaptureSceneSample(pixels, pitch, width, height);
#endif
  e2e5_stage_profiler::PresentFrame(width, height);
  const auto completed_us = e2e_clock::Microseconds();
  presentation_profile.Measure(44, completed_us - present_start_us);
  if (last_screen == GlobalsV::CurrentScreenId && previous_present_us)
    presentation_profile.Measure(45, completed_us - previous_present_us);
  if (last_screen != GlobalsV::CurrentScreenId) {
    char detail[192]{};
    std::snprintf(detail, sizeof(detail),
        "screen first presented id=%d presentations=%llu width=%d height=%d rgb565_hash=%016llx",
        GlobalsV::CurrentScreenId, static_cast<unsigned long long>(g_presentation_count),
        width, height, static_cast<unsigned long long>(g_last_presentation_hash));
    e2e_stage::Log(detail);
    last_screen = GlobalsV::CurrentScreenId;
  }
  previous_present_us = completed_us;
#if defined(E2E5_HOST_SDL_WINDOW)
  if (g_presentation_count == 1) {
    char detail[160]{};
    std::snprintf(detail, sizeof(detail),
                  "HOST first software frame width=%d height=%d rgb565_hash=%016llx",
                  width, height,
                  static_cast<unsigned long long>(g_last_presentation_hash));
    e2e_stage::Log(detail);
  }
#endif
#if defined(__SWITCH__)
  const auto now = e2e_clock::Milliseconds();
  if (!g_rate_start_tick) {
    g_rate_start_tick = now;
    g_rate_start_count = g_presentation_count;
    g_present_us_sum = 0;
    g_present_us_max = 0;
  } else if (static_cast<std::uint32_t>(now - g_rate_start_tick) >= 5000u) {
    const auto elapsed = static_cast<std::uint32_t>(now - g_rate_start_tick);
    const auto frames = g_presentation_count - g_rate_start_count;
    const auto fps_x100 = frames * UINT64_C(100000) / elapsed;
    char detail[256]{};
    std::snprintf(detail, sizeof(detail),
                  "presentation rate frames=%llu elapsed_ms=%u fps_x100=%llu present_avg_us=%llu present_max_us=%llu screen=%d rgb565_hash=%016llx",
                  static_cast<unsigned long long>(frames), elapsed,
                  static_cast<unsigned long long>(fps_x100),
                  static_cast<unsigned long long>(frames ? g_present_us_sum / frames : 0),
                  static_cast<unsigned long long>(g_present_us_max), GlobalsV::CurrentScreenId,
                  static_cast<unsigned long long>(g_last_presentation_hash));
    e2e_stage::Log(detail);
    g_rate_start_tick = now;
    g_rate_start_count = g_presentation_count;
    g_present_us_sum = 0;
    g_present_us_max = 0;
  }
#endif
  return true;
}

void ShutdownPresentation() {
#if defined(__SWITCH__)
  if (g_sdl_texture) SDL_DestroyTexture(g_sdl_texture);
  if (g_sdl_renderer) SDL_DestroyRenderer(g_sdl_renderer);
  g_sdl_texture = nullptr;
  g_sdl_renderer = nullptr;
  g_texture_width = 0;
  g_texture_height = 0;
#if defined(E2E_DEBUG_CURSOR_OVERLAY) && E2E_DEBUG_CURSOR_OVERLAY
  g_cursor_overlay_logged = false;
#endif
  g_rate_start_tick = 0;
  g_rate_start_count = 0;
  g_present_us_sum = 0;
  g_present_us_max = 0;
  g_last_hash_tick = 0;
#endif
  g_presentation_count = 0;
  g_last_presentation_hash = 0;
  g_presentation_diagnostics = {};
  g_presentation_failure_logged = false;
}

void ShutdownSoftwareRenderer() {
  ShutdownPresentation();
  if (g_initialized) ClearSoftwareGlobals();
  g_initialized = false;
}

bool IsInitialized() { return g_initialized; }
std::uint64_t PresentationCount() { return g_presentation_count; }
std::uint64_t LastPresentationHash() { return g_last_presentation_hash; }
PresentationDiagnostics LastPresentationDiagnostics() { return g_presentation_diagnostics; }

}  // namespace srhd_awa::platform::renderer_platform
