#include "renderer_platform.hpp"
#include "e2e515_perf.hpp"
#include "units/GR_Main.hpp"
#include "units/GR_GraphBuf.hpp"
#include "units/GI_MessageLoop.hpp"
#include "units/GlobalsV.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc,char** argv) {
  namespace renderer=srhd_awa::platform::renderer_platform;
  namespace perf=srhd_awa::platform::perf515;
  namespace fs=std::filesystem;
  assert(argc==2 && !fs::exists(argv[1]));
  const fs::path root=fs::absolute(argv[1]);fs::create_directories(root);
  {std::ofstream f(root/"perf-mode.txt");f<<"basic\n";}
  {std::ofstream f(root/"scene-mode.txt");f<<"on\n";}
  perf::Initialize(root.c_str());assert(perf::SceneEnabled());
  renderer::RendererConfig cfg;cfg.game_width=320;cfg.game_height=180;cfg.minimap_buffer_size=8;
  assert(renderer::InitializeSoftwareRenderer(cfg,nullptr));
  auto* original=GR_Main::ScreenRenderBuffer;
  original->FillPixels16(0xf800);
  GI_MessageLoop::TMessageLoopGI map{},dialog{};
  const auto old=GlobalsV::RegisteredScreens[16];GlobalsV::RegisteredScreens[16]=&map;
  {
    renderer::SceneCanvas canvas(false,&map);
    GI_MessageLoop::PushMessageLoop(&map);
    assert(*static_cast<std::uint16_t*>(GR_Main::ScreenRenderBuffer->GetPixels())==0);
    renderer::SceneFrameBegin(&map,301,0,0);
    GR_Main::ScreenRenderBuffer->FillPixels16(0x001f);
    renderer::SceneFrameEnd(&map);
    assert(renderer::PresentRgb565(GR_Main::ScreenRenderBuffer->GetPixels(),GR_Main::ScreenRenderBuffer->PitchBytes,320,180));
    for(int i=0;i<5;++i) {
      GR_Main::CaptureScreenBackground(false,false);
      renderer::SceneCache("bind",&map,&map,GR_Main::AuxRenderBuffer,false,true);
      auto* parent=GR_Main::ScreenRenderBuffer;
      {
        renderer::SceneCanvas modal(true,&dialog);
        GI_MessageLoop::PushMessageLoop(&dialog);
        renderer::SceneFrameBegin(&dialog);
        GR_Main::ScreenRenderBuffer->FillPixels16(0x07e0);
        renderer::SceneFrameEnd(&dialog);
        GI_MessageLoop::PopMessageLoop(&dialog);
      }
      assert(GR_Main::ScreenRenderBuffer==parent);
      assert(*static_cast<std::uint16_t*>(parent->GetPixels())==0x001f);
      renderer::SceneAction("day-accepted",&map,302+i,0);
      renderer::SceneFrameBegin(&map,302+i,i,0);
      renderer::SceneFrameEnd(&map);
    }
    GI_MessageLoop::PopMessageLoop(&map);
  }
  {
    renderer::SceneCanvas foreign(false,&dialog);
    assert(*static_cast<std::uint16_t*>(GR_Main::ScreenRenderBuffer->GetPixels())==0);
  }
  GlobalsV::RegisteredScreens[16]=old;
  renderer::ShutdownSoftwareRenderer();
  pas::free(GI_MessageLoop::MessageLoopStack);GI_MessageLoop::MessageLoopStack=nullptr;
  perf::Shutdown();
  std::ifstream f(root/"logs/perf515-stage.log");
  const std::string log((std::istreambuf_iterator<char>(f)),{});
  for(const char* marker:{"event=scene-enter","event=frame-compose","event=compose-before","event=compose-after","event=present-input","event=background-capture","event=background-restore","event=ui-cache-bind","event=day-accepted","event=scene-exit"})
    assert(log.find(marker)!=std::string::npos);
  std::cout<<"PASS instrumented SceneCanvas/compose/upload, five dialog returns, foreign-scene isolation; controlled host only\n";
}
