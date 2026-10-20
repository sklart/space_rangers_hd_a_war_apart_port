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
        marker='                    if (e2e_arcade && MMSystem::timeGetTime() - e2e_debt_log_tick >= 5000u) {'
        once(marker,'                    if (e2e_arcade) srhd_awa::platform::perf515::TickBatch(e2e_timer_debt.Pending(), e2e_delivered_ms);\n'+marker)
    if name=='program.cpp':
        once('    namespace platform = srhd_awa::platform;',
             '    namespace platform = srhd_awa::platform;\n    platform::perf515::Session perf515_session(platform::e2e_stage::kRoot);')
        text=text.replace('        std::_Exit(1);','        platform::perf515::Shutdown();\n        std::_Exit(1);')
    if name=='GR_Main.cpp':
        for function in ['AppendLogLineThreadSafe','AppendLogTextThreadSafe','AppendDebugLogLine']:
            marker=f'    void {function}(const pas::AnsiString& Text) {{'
            once(marker,marker+'\n        srhd_awa::platform::perf515::AsyncLog("[SESSION] ",Text.c_str());\n        return;')
    return text
