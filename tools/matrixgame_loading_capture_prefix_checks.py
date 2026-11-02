"""Native entry snapshot failure/retry with real host; CPU-only caps refusal."""
import matrixgame_hosted_windows_native_checks as check
from matrixgame_standard_nro_audit import ROOT
if __name__=='__main__':
 check.BASE=ROOT/'build/matrixgame/e2e580-loading-capture/native-build'
 generated=check.BASE/'loading_capture_prefix.cpp'
 text=(ROOT/'tests/probe_matrixgame_hosted_native_prefix.cpp').read_text()
 replacements={
  '#include "init_runtime.hpp"':'#include "init_runtime.hpp"\n#include "transition_backend.hpp"',
  'struct Device final:p::Device {':'''struct Device final:p::Device,native_form::ScreenshotProvider {
 unsigned captures{};bool capture_fails{true};
 native_form::ScreenshotPixels CaptureScreenshot()override{
  ++captures;if(capture_fails)throw p::Unsupported("LOADING_CAPTURE_REFUSED");
  return {1,1,4,native_form::ScreenshotFormat::Rgba8,std::vector<std::byte>(4)};
 }''',
  '   bool stopped=false;try{runtime.InitializeNativeGame(options);}':'''   device.capture_fails=true;const auto caps_before=device.caps;
   bool capture_stopped=false;try{runtime.InitializeNativeGame(options);}catch(const p::Unsupported& error){capture_stopped=std::string(error.what())=="LOADING_CAPTURE_REFUSED";}
   assert(capture_stopped && device.caps==caps_before && !runtime.CpuReady() && !runtime.OriginalGameReady() && !window.attachments);NativeClean();
   assert(EngineCallbacks::Bound() && GR_Main::MainDataConfig==config && GR_Main::CacheDataRoot==cache);
   device.capture_fails=false;
   bool stopped=false;try{runtime.InitializeNativeGame(options);}''',
  ' assert(device.caps==2 && device.raster_calls==2);':''' assert(device.caps==2 && device.raster_calls==2 && device.captures==4);
 {transition::LoadingCaptureScope released(device);}assert(device.captures==5);
 std::puts("LOADING_CAPTURE_NATIVE_ENTRY refusal_before_Init=2 retry_caps_rollback=2 exclusive_scope_released=PASS real_host_preserved=PASS SYNTHETIC_CPU_PIXELS NO_GPU");'''
 }
 for old,new in replacements.items():assert text.count(old)==1;text=text.replace(old,new,1)
 if generated.exists():assert generated.read_text(encoding='utf-8')==text
 else:
  with generated.open('x',encoding='utf-8') as stream:stream.write(text)
 check.SOURCE=generated
 check.main()
