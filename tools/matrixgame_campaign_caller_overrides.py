"""Route native FRun caller restoration through the real scoped presentation lease."""
def apply(name,text):
 if name!='Robot.cpp':return text
 start=text.index('    std::int32_t FRun(');end=text.index('    void PAS_STDCALL RobotPlaySound(',start);body=text[start:end]
 changes=[
  ('        SavedDirectory = SysUtilsImports::GetCurrentDir();','        SavedDirectory = SysUtilsImports::GetCurrentDir();\n        auto native_caller = srhd_awa::platform::planet_battle::Capture();'),
  ('                        Result = RobotInterface->Run(', '                        if (native_caller) native_caller->EnterBattle();\n                        Result = RobotInterface->Run('),
  ('''                GR_Main::GR_DXReset();
                WindowsSdk::SetWindowTextA(GR_Main::MainWindowHandle, pas::literal_pointer("Rangers"));
                GR_Main::ApplyGammaRamp(GR_Main::DisplayBrightness, GR_Main::DisplayContrast);''', '''                if (native_caller) {
                    native_caller->Restore();
                } else {
                    GR_Main::GR_DXReset();
                    GR_Main::ApplyGammaRamp(GR_Main::DisplayBrightness, GR_Main::DisplayContrast);
                }
                if (!native_caller || !native_caller->UsesNativeWindow())
                    WindowsSdk::SetWindowTextA(GR_Main::MainWindowHandle, pas::literal_pointer("Rangers"));'''),
  ('                if (GR_Main::ShowSystemMouse) {','                if (!native_caller || !native_caller->UsesNativeWindow()) {\n                if (GR_Main::ShowSystemMouse) {'),
  ('                reinterpret_cast<GI_MessageLoop::TMessageLoopGI*>(GlobalsV::RegisteredScreens[GlobalsV::CurrentScreenId])->InvalidateViewport();','                }\n                reinterpret_cast<GI_MessageLoop::TMessageLoopGI*>(GlobalsV::RegisteredScreens[GlobalsV::CurrentScreenId])->InvalidateViewport();')]
 for before,after in changes:
  assert body.count(before)==1,(name,before,body.count(before));body=body.replace(before,after)
 return '#include "planet_battle_presentation.hpp"\n'+text[:start]+body+text[end:]
