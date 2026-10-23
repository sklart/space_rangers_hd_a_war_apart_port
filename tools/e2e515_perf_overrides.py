"""Observation only: applied exclusively to the separate performance build."""
import re

def apply(name: str, text: str) -> str:
    def once(old,new):
        nonlocal text
        if text.count(old)!=1:raise RuntimeError(f'perf hook {name}: {old!r}')
        text=text.replace(old,new,1)
    scopes={
        'ab_MainForm.cpp': [('TfAB::TimerTakt','timer',False),('TfAB::DrawFrame','draw',False),
            ('TfAB::OnOpen','battle_open',False),('TfAB::OnClose','battle_close',False),('TfAB::ShowVictory','victory',False),
            ('TfAB::BeginBattleExit','battle_exit',False),('TfAB::TogglePause','pause_toggle',False),('TfAB::BattleKeyDown','input_key_call',True)],
        'ab_Object.cpp': [('TabObject::Advance','physics',True),('TabObject::FindCollision','collision',True)],
        'ab_ShipAI.cpp':[('TabShipAI::Advance','ai',True)],
        'ab_Ship.cpp':[('TabShip::FirePrimary','fire_primary_call',True),('TabShip::FireSecondary','fire_secondary_call',True),
            ('ab_Ship_RepelOverlaps','repel',True),('TabShip::StartThrust','movement_call',True)],
        'ab_Hit.cpp':[('TabHit::ApplyDamage','damage_call',True),('TabHit::ExplosionComplete','explosion_complete',True)],
    }
    if name in scopes:
        for function,label,detail in scopes[name]:
            pattern=r'^    [^\n{};]*\b'+re.escape(function)+r'\([^\n]*\) \{'
            matches=list(re.finditer(pattern,text,re.M))
            if len(matches)!=1:raise RuntimeError(f'perf signature {name}:{function}: {len(matches)}')
            marker=matches[0].group()
            once(marker,marker+f'\n        srhd_awa::platform::perf515::Scope perf515_scope("{label}",{str(detail).lower()});')
        if name=='ab_MainForm.cpp':
            once('                ++ab_Global::ArcadeTickCount;',
                 '                ++ab_Global::ArcadeTickCount;\n                srhd_awa::platform::perf515::Record("logical_tick");')
            once('                    Obj->Advance();',
                 '                    { srhd_awa::platform::perf515::Scope perf515_advance("advance",true); Obj->Advance(); }')
        if name=='ab_Hit.cpp':
            once('                if (Health <= 0) {',
                 '                if (Health <= 0) {\n                    srhd_awa::platform::perf515::Record("explosion_start",0,-1,this==ab_Ship::PlayerArcadeShip,Health);\n                    srhd_awa::platform::perf515::Scope perf515_explosion("explosion_setup");')
        text='#include "e2e515_perf.hpp"\n'+text
    if name=='GI_MessageLoop.cpp':
        text='#if defined(E2E5_HOST_SDL_WINDOW)\n#include "e2e516_map_probe.hpp"\n#endif\n'+text
        once('                            Draw(DrawRect);', '''#if defined(E2E5_HOST_SDL_WINDOW)
                            srhd_awa::platform::map516::Draw map_draw(this,{DrawRect.Left,DrawRect.Top,DrawRect.Right,DrawRect.Bottom});
#endif
                            Draw(DrawRect);''')
        marker='                    if (e2e_arcade && MMSystem::timeGetTime() - e2e_debt_log_tick >= 5000u) {'
        once(marker,'                    if (e2e_arcade) srhd_awa::platform::perf515::TickBatch(e2e_timer_debt.Pending(), e2e_delivered_ms);\n'+marker)
        start=text.index('    void TMessageLoopGI::DrawQueuedUpdateRects() {')
        end=text.index('    void TMessageLoopGI::DrawQueuedControlRects()',start)
        part=text[start:end]
        part=part.replace('    void TMessageLoopGI::DrawQueuedUpdateRects() {',
            '    void TMessageLoopGI::DrawQueuedUpdateRects() {\n        srhd_awa::platform::renderer_platform::SceneFrameBegin(this);',1)
        at=part.rfind('    }')
        part=part[:at]+'        srhd_awa::platform::renderer_platform::SceneFrameEnd(this);\n'+part[at:]
        text=text[:start]+part+text[end:]
    if name=='fStarMap.cpp':
        text='#if defined(E2E5_HOST_SDL_WINDOW)\n#include "e2e516_map_probe.hpp"\n#include "types/GI_RotateImage5.hpp"\n#endif\n'+text
        def draw_once(old,new):
            nonlocal text
            start=text.index('    void TfStarMap::DrawFrame() {')
            end=text.index('\n    void TfStarMap::RunTalkDialogs()',start)
            part=text[start:end]
            if part.count(old)!=1:raise RuntimeError('map draw hook: '+old)
            text=text[:start]+part.replace(old,new,1)+text[end:]
        draw_once('            Stage = 5;', '''            Stage = 5;
#if defined(E2E5_HOST_SDL_WINDOW)
            using namespace srhd_awa::platform;
            map516::Begin(aGalaxy::Galaxy->CurrentTurn,FilmStepIndex,int(Mode),GR_Main::ScreenRenderBuffer->GetPixels(),GR_Main::ScreenRenderBuffer->PitchBytes,GR_Main::GameScreenWidth,GR_Main::GameScreenHeight);
            if(map516::enabled && map516::sample && aPlayer::GetPlayer()->CurrentStar) {
                auto* ships=aPlayer::GetPlayer()->CurrentStar->Ships;
                for(int i=0;i<pas::list_count(ships);++i) {
                    auto* ship=pas::list_at<aShip::TShip>(ships,i);
                    auto* graphic=pas::class_cast_if<SE_Ship2::TShip2SE*>(ship->Graphic);
                    if(!graphic || !graphic->Image || !graphic->Image->Active)continue;
                    const auto r=graphic->Image->HitTestBounds;int dirty=0;
                    for(auto* n=UpdateRects->FirstRect;n;n=n->Next) {
                        const auto d=n->Bounds;
                        if(r.Left<d.Right && r.Right>d.Left && r.Top<d.Bottom && r.Bottom>d.Top)++dirty;
                    }
                    map516::Add(ship->Id,double(ship->Position.X),double(ship->Position.Y),graphic->Image,{r.Left,r.Top,r.Right,r.Bottom},dirty);
                }
            }
#endif''')
        draw_once('            Stage = 9;', '''#if defined(E2E5_HOST_SDL_WINDOW)
            srhd_awa::platform::map516::Phase("background");
#endif
            Stage = 9;''')
        draw_once('            DrawQueuedControlRects();', '''            DrawQueuedControlRects();
#if defined(E2E5_HOST_SDL_WINDOW)
            srhd_awa::platform::map516::Phase("world-ui-composed");
#endif''')
        text='#include "renderer_platform.hpp"\n#include "e2e_clock.hpp"\n#include "e2e515_perf.hpp"\n#include <cstdio>\n'+text
        once('    void TfStarMap::DrawFrame() {',
             '''    void TfStarMap::DrawFrame() {
        srhd_awa::platform::renderer_platform::SceneFrameBegin(this, aGalaxy::Galaxy ? aGalaxy::Galaxy->CurrentTurn : -1, FilmStepIndex, aPlayer::GetPlayer() ? aPlayer::GetPlayer()->Order : -1);
        if (srhd_awa::platform::perf515::SceneEnabled()) {
            static std::uint64_t last_observed_us{};
            const auto now=srhd_awa::platform::e2e_clock::Microseconds();
            if (now-last_observed_us>=500000 && aGalaxy::Galaxy && aPlayer::GetPlayer()) {
                const auto* p=aPlayer::GetPlayer();char detail[384]{};
                std::snprintf(detail,sizeof(detail),"event=map-state screen=16 owner=%p turn=%d film=%d mode=%d order=%d x=%.6f y=%.6f star=%d",this,aGalaxy::Galaxy->CurrentTurn,FilmStepIndex,int(Mode),int(p->Order),double(p->Position.X),double(p->Position.Y),p->CurrentStar?int(p->CurrentStar->Id):-1);
                srhd_awa::platform::perf515::AsyncLog("[SCENE] ",detail);
                last_observed_us=now;
            }
        }''')
        once('                CommitFrameDraw();','                CommitFrameDraw();\n                srhd_awa::platform::renderer_platform::SceneFrameEnd(this);')
        once('                aPlayer::GetPlayer()->virtual_TShip_NextDay();',
             '                aPlayer::GetPlayer()->virtual_TShip_NextDay();\n                srhd_awa::platform::renderer_platform::SceneAction("day-accepted",this,aGalaxy::Galaxy->CurrentTurn,FilmStepIndex);')
        once('    void TfStarMap::StartTurnFilm() {',
             '    void TfStarMap::StartTurnFilm() {\n        srhd_awa::platform::renderer_platform::SceneAction("turn-film-start",this,aGalaxy::Galaxy->CurrentTurn,FilmStepIndex);')
        once('    void TfStarMap::StopTurnFilm(std::uint8_t StopTurnProcessing) {',
             '    void TfStarMap::StopTurnFilm(std::uint8_t StopTurnProcessing) {\n        srhd_awa::platform::renderer_platform::SceneAction("turn-film-stop",this,aGalaxy::Galaxy->CurrentTurn,FilmStepIndex);')
    if name=='GR_GraphBuf.cpp':
        text='#include "e2e515_perf.hpp"\n'+text
        start=text.index('    void TGraphBufGR::LoadFromScreen(')
        end=text.index('\n    }',start)+len('\n    }')
        part=text[start:end]
        marker=r'^        \} catch \(\.\.\.\) \{'
        if len(re.findall(marker,part,re.M))!=1:raise RuntimeError('LoadFromScreen catch boundary changed')
        part=re.sub(marker,lambda m:m[0]+'\n            if(srhd_awa::platform::perf515::SceneEnabled()) srhd_awa::platform::perf515::AsyncLog("[SCENE] ","event=load-from-screen-failed");',part,flags=re.M)
        text=text[:start]+part+text[end:]
    if name=='GI_GraphBuf.cpp':
        text='#include "renderer_platform.hpp"\n'+text
        for marker,event,buffer,borrowed in [
            ('    void TGraphBufGI::BindExternalGraphBuf(GR_GraphBuf::TGraphBufGR* Buffer) {','bind','Buffer','true'),
            ('    void TGraphBufGI::Draw(WindowsSdk::TRect ClipRect) {','draw','GraphBuf','UsesExternalGraphBuf')]:
            once(marker,marker+f'\n        srhd_awa::platform::renderer_platform::SceneCache("{event}",MessageLoop,this,{buffer},SourceHasPerPixelAlpha,{borrowed});')
    if name=='ThreadCalc.cpp':
        text='#include "e2e515_perf.hpp"\n'+text
        once('    void TThreadCalc_Execute(TThreadCalc* Self) {',
             '    void TThreadCalc_Execute(TThreadCalc* Self) {\n        srhd_awa::platform::perf515::ThreadScope watchdog_turn(srhd_awa::platform::perf515::ThreadRole::Turn,"turn-execute",static_cast<unsigned>(Self->Job));')
        once('    void WaitForTurnCalculation() {',
             '    void WaitForTurnCalculation() {\n        srhd_awa::platform::perf515::ThreadScope watchdog_wait(srhd_awa::platform::perf515::ThreadRole::Main,"wait-turn-idle");')
    if name=='aSaveLoad.cpp':
        text='#include "e2e515_perf.hpp"\n'+text
        once('    void TSaver_Execute(TSaver* Self) {',
             '    void TSaver_Execute(TSaver* Self) {\n        srhd_awa::platform::perf515::ThreadScope watchdog_save(srhd_awa::platform::perf515::ThreadRole::Saver,"save-lock-wait");')
        start=text.index('    void TSaver_Execute(TSaver* Self) {')
        end=text.index('    void TSaver::QueueSave(',start)
        part=text[start:end]
        marker='        pas::critical_enter(SaveLoadLock);'
        if part.count(marker)!=1:raise RuntimeError('Saver lock watchdog boundary changed')
        part=part.replace(marker,marker+'\n        srhd_awa::platform::perf515::MarkThread(srhd_awa::platform::perf515::ThreadRole::Saver,"save-serialize");',1)
        text=text[:start]+part+text[end:]
    if name=='program.cpp':
        once('    namespace platform = srhd_awa::platform;',
             '    namespace platform = srhd_awa::platform;\n    platform::perf515::Session perf515_session(platform::e2e_stage::kRoot);')
        text=text.replace('        std::_Exit(1);','        platform::perf515::Shutdown();\n        std::_Exit(1);')
        once('    platform::win32_compat::LogRuntimeStats();\n    if (!platform::e2e_stage::FirstFailure().empty()) {',
             '    platform::perf515::MarkThread(platform::perf515::ThreadRole::Main,"post-ProgramMain");\n    platform::perf515::AsyncLog("[WATCHDOG] ","ProgramMain returned; checking exit gates");\n    platform::win32_compat::LogRuntimeStats();\n    if (!platform::e2e_stage::FirstFailure().empty()) {')
        once('    return 0;\n}',
             '    platform::perf515::MarkThread(platform::perf515::ThreadRole::Main,"main-return");\n    platform::perf515::AsyncLog("[WATCHDOG] ","main returning; perf session destructor next");\n    return 0;\n}')
    if name=='GR_Main.cpp':
        for function in ['AppendLogLineThreadSafe','AppendLogTextThreadSafe','AppendDebugLogLine']:
            marker=f'    void {function}(const pas::AnsiString& Text) {{'
            once(marker,marker+'\n        srhd_awa::platform::perf515::AsyncLog("[SESSION] ",Text.c_str());\n        return;')
    return text
