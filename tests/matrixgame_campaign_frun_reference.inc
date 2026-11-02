// Genuine Gov OnOpen -> memory snapshot -> Robot::FRun -> MatrixGame ->
// original result processing. The UI StartPlanetBattle* auto-save handler is
// outside this fixture's entry boundary. Transition/map/difficulty are inputs;
// the result, statistics, rewards and history are never supplied by the test.
#include "rangers_legacy_presentation.hpp"
#include "units/aRanger.hpp"
#include "units/GI_MessageLoop.hpp"
#include "units/fGov.hpp"
#include "renderer_platform.hpp"
void QueryCallerDesktop(std::uintptr_t,void*);
void CaptureCallerGammaBeforeBattle();
void VerifyCallerGammaAfterBattle();
std::uint32_t AttachRangersReferenceWindow(void*);
void DetachRangersReferenceWindow();
#ifndef MATRIXGAME_GOV_DIFFICULTY
#define MATRIXGAME_GOV_DIFFICULTY 1
#endif
#ifndef MATRIXGAME_CAMPAIGN_DIFFICULTY
#define MATRIXGAME_CAMPAIGN_DIFFICULTY 2
#endif

int RunOriginalWindowsGame(void* module,void* hwnd,std::uintptr_t api,std::uintptr_t raw,
 platform3g::Window& window,platform3g::Device& device)try{
 HostClean();MatrixClean();Empty();const auto cwd=std::filesystem::current_path();
 diagnostic::Journal journal(std::filesystem::path(output_root)/"journals","E602-Gov-original-FRun",hosted_map_source_sha);diagnostic::Binding journal_binding(journal);
 {
  RangersHostSession host(RangersHostOptions{},RangersDeferredCallbacksTag{});
  output_paths::NativeScope outputs(output_root.c_str());
  GR_Main::MainWindowHandle=AttachRangersReferenceWindow(hwnd);
  struct BorrowedCaller {
   ~BorrowedCaller(){GR_Main::Direct3DDevice=nullptr;GR_Main::Direct3D=nullptr;DetachRangersReferenceWindow();}
  } borrowed;
  RangersLegacyWindowBinding real_window(GR_Main::MainWindowHandle,hwnd);
  GR_Main::Direct3D=pas::ComView<Direct3D9::IDirect3D9_Tag>(reinterpret_cast<void*>(api));
  GR_Main::Direct3DDevice=pas::ComView<Direct3D9::IDirect3DDevice9_Tag>(reinterpret_cast<void*>(raw));
  auto settings=SRobotsSettings::generate_default_settings();settings.FDirect3D=reinterpret_cast<void*>(api);settings.FD3DDevice=reinterpret_cast<void*>(raw);settings.m_ResolutionX=1024;settings.m_ResolutionY=768;
  OriginalGameOptions options;options.module=module;options.window=hwnd;options.seed=37;options.window_provider=&window;options.device_provider=&device;options.resource_root=matrix_root.c_str();options.settings=&settings;
  NativeRangersInterface binding(options,RangersLegacyReferenceTag{});binding.Install(MakeRangersHostCallbacks());
  m::RangersScriptSession scripts;
  struct SoftwareUi {
   GR_Main::TBlendPixel16 blend=GR_Main::BlendPixel16;
   GR_Main::TTriangleRasterizer16 triangle=GR_Main::TriangleRasterizer16;
   GR_Main::TLineRasterizer16 line=GR_Main::LineRasterizer16;
   SoftwareUi(){
    namespace renderer=srhd_awa::platform::renderer_platform;
    assert(!renderer::IsInitialized() && !GR_Main::CurrentPixelFormat && !GR_Main::ScreenRenderBuffer && !GR_Main::RenderScratchBuffer && !GR_Main::AuxRenderBuffer);
    renderer::RendererConfig config;config.game_width=config.presentation_width=1024;config.game_height=config.presentation_height=768;config.frame_limit=false;
    auto* data=GR_Main::MainDataConfig->GetBlockByPath(u"Data"_wref.get());
    if(data->CountParams(u"MiniMapBufSize"_wref.get()))config.minimap_buffer_size=EC_Str::ExtractDigitsToIntW(pas::view(data->GetParam(u"MiniMapBufSize"sv)));
    std::string error;if(!renderer::InitializeSoftwareRenderer(config,&error))throw std::runtime_error(error);
    assert(GR_Main::ScreenRenderBuffer->GetPixels() && GR_Main::ScreenRenderBuffer->PitchBytes==2048);
   }
   ~SoftwareUi(){srhd_awa::platform::renderer_platform::ShutdownSoftwareRenderer();GR_Main::BlendPixel16=blend;GR_Main::TriangleRasterizer16=triangle;GR_Main::LineRasterizer16=line;}
  } software_ui;
  Caller caller;
  GR_Main::PresentationWidth=1024;GR_Main::PresentationHeight=768;GR_Main::WindowedModeRequested=true;GR_Main::DisableTripleBuffer=true;GR_Main::AlternateViewportEnabled=false;
  QueryCallerDesktop(api,&GR_Main::DesktopDisplayMode);GR_Main::Direct3DPresentParameters.Windowed=true;
  const auto old_cursor=GR_Main::CustomCursorEnabled;GR_Main::CustomCursorEnabled=false;
  struct CursorRestore{std::uint8_t value;~CursorRestore(){GR_Main::CustomCursorEnabled=value;}} cursor{old_cursor};
  m::RangersGlobalUiSession ui;
  const auto previous_screen=GlobalsV::CurrentScreenId;GlobalsV::CurrentScreenId=GlobalsV::screenIntroduction;
  {
   m::RangersCampaignOptions campaign_options;campaign_options.difficulty.fill(MATRIXGAME_CAMPAIGN_DIFFICULTY);
   m::RangersCampaignSession campaign(campaign_options);ExerciseOriginalLanding();
   auto* gov=Globals::GovernmentScreen;gov->InitializeLayout();assert(gov->MainPanel->Screen==gov);
   struct GovernmentLoop {
    fGov::TfGov* gov;GlobalsV::TGameScreenId previous;pas::List* previous_stack;
    explicit GovernmentLoop(fGov::TfGov* p):gov(p),previous(GlobalsV::CurrentScreenId),previous_stack(GI_MessageLoop::MessageLoopStack){GlobalsV::CurrentScreenId=GlobalsV::screenGovernment;GI_MessageLoop::PushMessageLoop(gov);}
    ~GovernmentLoop(){GI_MessageLoop::PopMessageLoop(gov);if(!previous_stack){assert(pas::list_count(GI_MessageLoop::MessageLoopStack)==0);pas::free(GI_MessageLoop::MessageLoopStack);GI_MessageLoop::MessageLoopStack=nullptr;}GlobalsV::CurrentScreenId=previous;}
   } active(gov);
   gov->OnOpen();gov->OnClose();
   int index=-1;
   for(int i=0;i<Globals::RobotMapDefinitions.length();++i){
    std::u16string normalized(pas::view(Globals::RobotMapDefinitions[i].Map));for(auto& ch:normalized)if(ch>=u'A' && ch<=u'Z')ch+=u'a'-u'A';
    std::u16string_view name(normalized);const auto slash=name.find_last_of(u"/\\");if(slash!=name.npos)name.remove_prefix(slash+1);
    if(name==u"desert1_3e.cmap"){assert(index<0);index=i;}
   }
   if(index<0){
    for(int i=0;i<Globals::RobotMapDefinitions.length();++i){const auto value=static_cast<pas::AnsiString>(Globals::RobotMapDefinitions[i].Map);std::printf("GOV_MAP id=%d path=%.*s\n",Globals::RobotMapDefinitions[i].Id,int(value.length()),reinterpret_cast<const char*>(value.data()));}
    throw std::runtime_error("real Gov definition for desert1_3e.cmap absent");
   }
   auto* player=aPlayer::GetPlayer();const auto player_id=player->Id;const auto planet_id=player->CurrentPlanet->Id;
   const auto turn=campaign.Galaxy().CurrentTurn,money=player->Money,battles=player->PlanetBattles,history=player->PlanetBattleHistory.length();
   const auto map_id=Globals::RobotMapDefinitions[index].Id;
   constexpr int difficulty=MATRIXGAME_GOV_DIFFICULTY;static_assert(difficulty>=1 && difficulty<=3);
   gov->PlanetBattleMapId=map_id;fGov::GovernmentBattleDifficulty=difficulty;gov->PendingTransition=1;
   GR_Main::ApplyGammaRamp(GR_Main::DisplayBrightness,GR_Main::DisplayContrast);CaptureCallerGammaBeforeBattle();
   Driver backend;
   std::printf("GOV_FRUN entry_boundary=OnOpen map_id=%d difficulty=%d real_campaign_planet=PASS ui_autosave_handler=NOT_RUN\n",map_id,difficulty);
   {native_loop::Scope loop(backend);gov->OnOpen();}
   VerifyCallerGammaAfterBattle();MatrixClean();assert(!GR_Main::RobotBattleActive && !GlobalsV::MemorySnapshotActive && !aSaveLoad::MemorySnapshotBuffer);
   assert(srhd_awa::platform::renderer_platform::PresentationCount()>0 && srhd_awa::platform::renderer_platform::LastPresentationHash()!=0);
   assert(backend.natural_outcome==2 || backend.natural_outcome==3);assert(gov->PendingTransition==backend.natural_outcome && GlobalsV::RequestedScreenId==GlobalsV::screenGovernment);
   player=aPlayer::GetPlayer();assert(player && player->Id==player_id && player->CurrentPlanet && player->CurrentPlanet->Id==planet_id && campaign.Galaxy().CurrentTurn==turn);
   const auto& actual=Robot::RobotBattleStatistics;const auto& expected=backend.expected;
   assert(actual.SignedTimeMs==expected.m_Time && actual.RobotsBuilt==expected.m_BuildRobot && actual.RobotsDestroyed==expected.m_KillRobot && actual.TurretsBuilt==expected.m_BuildTurret && actual.TurretsDestroyed==expected.m_KillTurret && actual.BuildingsDestroyed==expected.m_KillBuilding);
   gov->OnClose();gov->OnOpen();assert(gov->PendingTransition==0 && !GR_Main::ExitScreenLoop);
   assert(player==aPlayer::GetPlayer() && player->PlanetBattleHistory.length()==history+1 && player->LastPlanetBattleTurn==turn && player->Money>=money);
   const auto& record=player->PlanetBattleHistory[history];assert(record.MapId==map_id && record.CompletionMode==backend.natural_outcome && record.ResultCode==difficulty && record.DateTurn==turn);
   assert(record.Statistics.SignedTimeMs==actual.SignedTimeMs && record.Statistics.RobotsBuilt==actual.RobotsBuilt && record.Statistics.RobotsDestroyed==actual.RobotsDestroyed && record.Statistics.TurretsBuilt==actual.TurretsBuilt && record.Statistics.TurretsDestroyed==actual.TurretsDestroyed && record.Statistics.BuildingsDestroyed==actual.BuildingsDestroyed);
   assert(player->PlanetBattles==battles+(backend.natural_outcome==3?1:0) && gov->DialogText.length()>0);
   std::printf("GOV_FRUN actual_outcome=%d original_Gov_history=PASS actual_statistics=PASS reward_delta=%d restored_player_planet=PASS result_dialog_units=%zu requested_Gov_return=PASS no_save_file_API=PASS\n",backend.natural_outcome,player->Money-money,std::size_t(gov->DialogText.length()));
   gov->OnClose();
  }
  GlobalsV::CurrentScreenId=previous_screen;
  Robot::FinalizeRobotRuntime();assert(!Robot::RobotInterface && !g_RangersInterface && !EngineCallbacks::Bound());
  assert(std::filesystem::current_path()==cwd && EC_HsFile::LooseFileRoot==pas::AnsiString());
 }
 HostClean();MatrixClean();Empty();journal.RequireHealthy();std::puts("GOV_FRUN original_campaign_UI_script_host_cleanup=PASS");return 0;
}catch(const pas::Raised& e){const auto* error=pas::class_cast_if<pas::Exception*>(e.object.get());if(error)std::printf("GOV_FRUN_FAILURE Pascal=%.*s\n",int(error->message.length()),reinterpret_cast<const char*>(error->message.data()));return 91;}
catch(const std::exception& e){std::printf("GOV_FRUN_FAILURE CXX=%s\n",e.what());return 92;}
