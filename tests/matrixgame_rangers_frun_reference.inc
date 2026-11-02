// Appended to the preserved E592 real mission driver. No original FRun body is
// extracted/reimplemented: the linked original Robot.cpp owns the whole call.
#include "units/GI_MessageLoop.hpp"
#include "units/GlobalsV.hpp"
#include "units/EC_BlockPar.hpp"
#include "types/GR_Rect.hpp"
#include "units/GR_GraphBuf.hpp"
#include "rangers_legacy_presentation.hpp"
void QueryCallerDesktop(std::uintptr_t,void*);
void CaptureCallerGammaBeforeBattle();
void VerifyCallerGammaAfterBattle();
std::uint32_t AttachRangersReferenceWindow(void*);
void DetachRangersReferenceWindow();
int RunOriginalWindowsGame(void* module,void* hwnd,std::uintptr_t api,std::uintptr_t raw,
 platform3g::Window& window,platform3g::Device& device){
 HostClean();MatrixClean();const auto cwd=std::filesystem::current_path();
 diagnostic::Journal journal(std::filesystem::path(output_root)/"journals","E593-original-FRun",hosted_map_source_sha);diagnostic::Binding journal_binding(journal);
 {
  RangersHostSession host(RangersHostOptions{},RangersDeferredCallbacksTag{});
  output_paths::NativeScope outputs(output_root.c_str());
  auto settings=SRobotsSettings::generate_default_settings();settings.FDirect3D=reinterpret_cast<void*>(api);settings.FD3DDevice=reinterpret_cast<void*>(raw);settings.m_ResolutionX=800;settings.m_ResolutionY=600;
  OriginalGameOptions options;options.module=module;options.window=hwnd;options.seed=37;
  options.window_provider=&window;options.device_provider=&device;options.resource_root=matrix_root.c_str();options.settings=&settings;
  NativeRangersInterface binding(options,RangersLegacyReferenceTag{});binding.Install(MakeRangersHostCallbacks());
  // A real base GI receiver with original panels, cursor and update rectangles.
  // This exercises the FRun boundary; it is explicitly NOT a TfGov/Galaxy UI.
  auto* screen=pas::construct_call<GI_MessageLoop::TMessageLoopGI>(GI_MessageLoop::TMessageLoopGI_Create);
  auto* user=pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);
  const auto old_id=GlobalsV::CurrentScreenId;auto* old_screen=GlobalsV::RegisteredScreens[GlobalsV::screenNone];
  const auto old_cursor=GR_Main::CustomCursorEnabled;
  struct CallerCleanup {
   GI_MessageLoop::TMessageLoopGI* screen;EC_BlockPar::TBlockParEC* user;GlobalsV::TGameScreenId id;pas::Object* old_screen;std::uint8_t cursor;
   ~CallerCleanup(){GlobalsV::RegisteredScreens[GlobalsV::screenNone]=old_screen;GlobalsV::CurrentScreenId=id;GR_Main::CustomCursorEnabled=cursor;pas::free(screen);pas::free(GR_Main::CurrentPixelFormat);GR_Main::CurrentPixelFormat=nullptr;GR_Main::UserSettingsConfig=nullptr;pas::free(user);GR_Main::Direct3DDevice=nullptr;GR_Main::Direct3D=nullptr;DetachRangersReferenceWindow();}
  } caller{screen,user,old_id,old_screen,old_cursor};
  assert(!GR_Main::UserSettingsConfig);GR_Main::UserSettingsConfig=user;
  GR_Main::MainWindowHandle=AttachRangersReferenceWindow(hwnd);
  RangersLegacyWindowBinding real_window(GR_Main::MainWindowHandle,hwnd);
  GR_Main::Direct3D=pas::ComView<Direct3D9::IDirect3D9_Tag>(reinterpret_cast<void*>(api));
  GR_Main::Direct3DDevice=pas::ComView<Direct3D9::IDirect3DDevice9_Tag>(reinterpret_cast<void*>(raw));
  GR_Main::GameScreenWidth=800;GR_Main::GameScreenHeight=600;GR_Main::GameScreenRect={0,0,800,600};
  GR_Main::PresentationWidth=800;GR_Main::PresentationHeight=600;
  GR_Main::WindowedModeRequested=true;GR_Main::DisableTripleBuffer=true;GR_Main::AlternateViewportEnabled=false;
  QueryCallerDesktop(api,&GR_Main::DesktopDisplayMode);assert(GR_Main::DesktopDisplayMode.Height>600);
  GR_Main::Direct3DPresentParameters.Windowed=true;GR_Main::CustomCursorEnabled=false;
  // Same real 565 UI pixel-format initialization as original GR_DXInit. Labels
  // use PackNormalizedRgb during construction, before any viewport is drawn.
  assert(!GR_Main::CurrentPixelFormat);GR_Main::CurrentPixelFormat=pas::make_object<GR_GraphBuf::TPixelFormatGR>();
  GR_Main::CurrentPixelFormat->RedMask=0xf800u;GR_Main::CurrentPixelFormat->GreenMask=0x07e0u;GR_Main::CurrentPixelFormat->BlueMask=0x001fu;
  GR_Main::CurrentPixelFormat->AlphaMask=0;GR_Main::CurrentPixelFormat->BytesPerPixel=2;GR_Main::CurrentPixelFormat->RebuildChannelMetrics();
  screen->InitializeDefaults();assert(screen->RootUiObject && screen->CursorControl && screen->UpdateRects);
  screen->UpdateRects->Clear();GlobalsV::RegisteredScreens[GlobalsV::screenNone]=screen;GlobalsV::CurrentScreenId=GlobalsV::screenNone;
  const auto saved_sound=GlobalsV::SoundVolume,saved_music=GlobalsV::MusicVolume;
  GR_Main::ApplyGammaRamp(GR_Main::DisplayBrightness,GR_Main::DisplayContrast);CaptureCallerGammaBeforeBattle();
  std::puts("RANGERS_FRUN real_base_GI_receiver=READY real_borrowed_D3D9=READY government_Galaxy=NOT_CREATED");
  Driver backend;native_loop::Scope loop(backend);
  const int code=Robot::FRun(u"desert1_3e.cmap"_w,pas::WideString(),pas::WideString(),pas::WideString(),pas::WideString());
  VerifyCallerGammaAfterBattle();
  MatrixClean();assert(code==2 && backend.natural_outcome==2 && !GR_Main::RobotBattleActive);
  const auto& actual=Robot::RobotBattleStatistics;const auto& expected=backend.expected;
  assert(actual.SignedTimeMs==expected.m_Time && actual.RobotsBuilt==expected.m_BuildRobot && actual.RobotsDestroyed==expected.m_KillRobot && actual.TurretsBuilt==expected.m_BuildTurret && actual.TurretsDestroyed==expected.m_KillTurret && actual.BuildingsDestroyed==expected.m_KillBuilding);
  assert(GlobalsV::SoundVolume==saved_sound && GlobalsV::MusicVolume==saved_music && GlobalsV::MusicVolumeScale==1.f);
  assert(screen->UpdateRects->FirstRect && screen->UpdateRects->FirstRect->Bounds.Right==800 && screen->UpdateRects->FirstRect->Bounds.Bottom==600);
  assert(std::filesystem::current_path()==cwd && EC_HsFile::LooseFileRoot==pas::AnsiString());
  std::printf("RANGERS_FRUN code=%d signed_time=%d kills=%d turrets=%d ticks=%u original_caller_stats=PASS sound_music_restore=PASS original_GI_invalidation=PASS cwd_loose_root_cleanup=PASS Galaxy_return=NOT_RUN\n",code,actual.SignedTimeMs,actual.RobotsDestroyed,actual.TurretsDestroyed,backend.extra_ticks);
  Robot::FinalizeRobotRuntime();assert(!Robot::RobotInterface && !g_RangersInterface && !EngineCallbacks::Bound());
 }
 HostClean();MatrixClean();journal.RequireHealthy();return 0;
}
