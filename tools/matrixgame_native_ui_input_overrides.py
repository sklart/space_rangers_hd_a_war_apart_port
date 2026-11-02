"""Use a real scoped native message source before the original Win32 pump."""
def apply(name,text):
 if name!='GR_Main.cpp':return text
 anchor='    std::int32_t GR_WinMessage(TWindowMessageCallbackGR Callback) {'
 assert text.count(anchor)==1
 return '#include "rangers_ui_input.hpp"\n'+text.replace(anchor,anchor+'''
        if (auto* source = srhd_awa::platform::rangers_ui_input::installed) {
            return source->Pump([&](std::uint32_t message, std::uint32_t wp, std::int32_t lp) {
                if (pas::assigned(Callback)) Callback(message, wp, lp);
            }) ? 1 : 0;
        }
''',1)
