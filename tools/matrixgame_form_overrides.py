"""Complete original form/vtable with explicit native platform boundaries."""
import hashlib,re
from matrixgame_vector_object_overrides import function
from matrixgame_render_pipeline_overrides import adapt

KEYS={**{'VK_'+chr(c):c for c in range(65,91)},**{'VK_'+str(c):48+c for c in range(10)},
      **{'VK_F'+str(c):111+c for c in range(1,25)},
      'VK_LBUTTON':1,'VK_RBUTTON':2,'VK_MBUTTON':4,'VK_RETURN':13,'VK_SHIFT':16,
      'VK_CONTROL':17,'VK_PAUSE':19,'VK_ESCAPE':27,'VK_SPACE':32,'VK_DELETE':46,
      'VK_LSHIFT':160,'VK_RSHIFT':161,'VK_LBRACKET':219,'VK_RBRACKET':221}

def prepare_native_form(output):
 changes={}
 def write(name,text):
  path=output/name;path.write_bytes(text.encode());changes[name]=hashlib.sha256(path.read_bytes()).hexdigest()
 source=(output/'MatrixGame/src/MatrixFormGame.cpp').read_text(encoding='utf-8')
 screenshot=function(source,'void make_screenshot()')
 # Keep the full screenshot algorithm in the original TU. This native-only TU
 # explicitly rejects the still-unported F9 path; it never reports a saved file.
 text=source.replace(screenshot,'void make_screenshot(){native_form::Screenshot();}',1)
 text=text.replace('#include <sys/timeb.h>','').replace('#include <format>','')
 # The extracted native draw uses all original branches, routed to typed APIs.
 frame=(output/'MatrixGame/src/OriginalWindowsFrame.cpp').read_text(encoding='utf-8')
 frame=function(frame,'void DrawOriginalWindowsFrame(void (*before_present)())')
 frame=frame.replace('void DrawOriginalWindowsFrame(void (*before_present)())','void CFormMatrixGame::Draw(void)',1)
 frame=frame.replace('\n    if(before_present)before_present();','')
 original_draw=function(text,'void CFormMatrixGame::Draw(void)');text=text.replace(original_draw,frame,1)
 text=adapt(text)
 replacements={
  'D3DMATERIAL9':'form_platform::VideoMaterial','D3DLIGHT9':'form_platform::VideoLight',
  'D3DLIGHT_DIRECTIONAL':'3u','D3DMCS_MATERIAL':'0u',
  'D3DRS_SPECULARMATERIALSOURCE':'static_cast<srhd_awa::matrixgame::RasterRegister>(146)',
  'g_D3DD->SetMaterial(&mtrl);':'form_platform::CheckDeviceResult(form_platform::ReadyDevice().SetVideoMaterial(mtrl),"form SetMaterial");',
  'g_D3DD->SetLight(0, &light)':'form_platform::ReadyDevice().SetVideoLight(0,light)',
  'g_D3DD->LightEnable(0, true)':'form_platform::ReadyDevice().EnableVideoLight(0,true)',
  'light.Direction = g_MatrixMap->m_LightMain;':'light.Direction = {g_MatrixMap->m_LightMain.x,g_MatrixMap->m_LightMain.y,g_MatrixMap->m_LightMain.z};',
  'ClientToScreen(g_Wnd, (LPPOINT)&p);':'native_form::ClientPointToScreen(p);',
  'RECT r;\n            GetWindowRect(g_Wnd, &r);\n            ClipCursor(&r);':'native_form::ClipCursorToWindow();',
 }
 for old,new in replacements.items():
  if old not in text:raise ValueError('Original native form boundary changed: '+old)
  text=text.replace(old,new)
 # Source preparation already turned ZeroMemory into checked trivial clears;
 # value initialization is the matching typed material/light operation.
 for var,kind in [('mtrl','VideoMaterial'),('light','VideoLight')]:
  clear=f'::srhd_awa::matrixgame::ZeroTrivialBytes(&{var}, sizeof(form_platform::{kind}));'
  if text.count(clear)!=1:raise ValueError('Original form zero boundary changed: '+clear)
  text=text.replace(clear,var+' = {};')
 for name in set(re.findall(r'\bVK_\w+\b',text)):
  if name not in KEYS:raise ValueError('Unmapped original key '+name)
  text=re.sub(r'\b'+name+r'\b',str(KEYS[name]),text)
 marker='namespace {';index=text.index(marker)
 text=text[:index]+'''#undef ASSERT_DX
#define ASSERT_DX(call) form_platform::CheckDeviceResult((call),#call)
'''+text[index:]
 prelude='''// Complete original MatrixFormGame.cpp; native-only platform adaptation.
#include "native_form_backend.hpp"
#include "offscreen_backend.hpp"
#include "transition_backend.hpp"
#include "renderer_ops.hpp"
namespace form_platform=srhd_awa::matrixgame::platform3g;
namespace rp3g=form_platform;
namespace native_form=srhd_awa::matrixgame::native_form;
namespace frame_surface=srhd_awa::matrixgame::offscreen;
'''
 write('MatrixGame/src/OriginalNativeForm.cpp',prelude+text)
 loop_source=(output/'MatrixLib/3G/3g.cpp').read_text(encoding='utf-8')
 loop=function(loop_source,'int L3GRun()')
 message_loop='''        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            DispatchMessage(&msg); // Effectively calls L3GWndProc() handler
        }'''
 if loop.count(message_loop)!=1:raise ValueError('Original loop message boundary changed')
 loop=loop.replace(message_loop,'        native_loop::Pump();',1)
 for old,new in [('int L3GRun()','int RunOriginalNativeLoop()'),
                 ('using clock = std::chrono::high_resolution_clock;','using clock = native_loop::Clock;'),
                 ('    MSG msg;',''),('clock::now()','native_loop::Now()'),
                 ('std::this_thread::yield();','native_loop::Idle();'),
                 ('g_FormCur->Draw();','native_loop::DrawCurrentForm();'),
                 ('g_D3DD->GetAvailableTextureMem()','srhd_awa::matrixgame::platform3g::ReadyDevice().AvailableTextureBytes()'),
                 ('std::min(100LL, to_milliseconds(delta_time).count())','std::min<std::int64_t>(100, to_milliseconds(delta_time).count())')]:
  if old not in loop:raise ValueError('Original native loop boundary changed: '+old)
  loop=loop.replace(old,new)
 write('MatrixLib/3G/OriginalNativeLoop.cpp','''// Complete original L3GRun; GPL-2.0-or-later.
#include "native_loop.hpp"
#include "3g.hpp"
#include "CReminder.hpp"
#include "platform_3g.hpp"
#include "fps_counter.hpp"
#include "stupid_logger.hpp"
#include <algorithm>
namespace native_loop=srhd_awa::matrixgame::native_loop;
'''+loop)
 return changes
