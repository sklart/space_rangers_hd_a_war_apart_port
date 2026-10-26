"""Original visibility TU and Windows message/game loop, without success stubs."""
import hashlib
from matrixgame_render_pipeline_overrides import adapt
from matrixgame_vector_object_overrides import function


def prepare_full_game(output):
    changes = {}
    def write(name, text):
        path = output / name
        path.write_bytes(text.encode())
        changes[name] = hashlib.sha256(path.read_bytes()).hexdigest()
    # Portable CacheTexture is deliberately not a CTextureManaged. Every
    # original consumer in these complete map modules needs the typed registry.
    lookup='(CTextureManaged *)g_Cache->Get(CacheClass::TextureManaged, '
    for name,count in [('MatrixGame/src/MatrixMap.cpp',1),
                       ('MatrixGame/src/OriginalMapPrepare.cpp',3),
                       ('MatrixGame/src/MatrixTerSurface.cpp',4)]:
        text=(output/name).read_text(encoding='utf-8')
        if text.count(lookup)!=count:raise ValueError('Original map texture lookup changed: '+name)
        text=text.replace(lookup,'srhd_awa::matrixgame::legacy_texture::GetManaged(')
        if name.endswith('OriginalMapPrepare.cpp'):
            if text.count('CACHE_CREATE_TEXTURE()')!=2:raise ValueError('Original side texture factory changed')
            text=text.replace('CACHE_CREATE_TEXTURE()','srhd_awa::matrixgame::legacy_texture::CreateUnmanaged()')
        if name.endswith('MatrixMap.cpp'):
            for method in ('BeforeDraw','DrawObjects','DrawLandscape','DrawLandscapeSurfaces','DrawWater','DrawShadows','DrawEffects','Draw'):
                signature='void CMatrixMap::'+method+'('
                body=function(text,signature)
                observed=body.replace('{','{\n    std::clog << "original frame component: CMatrixMap::'+method+'\\n";',1)
                text=text.replace(body,observed,1)
            text='#include <iostream>\n'+text
        write(name,'#include "Texture.hpp"\n#include "legacy_texture_backend.hpp"\n#include "unmanaged_texture_backend.hpp"\n'+text)
    name = 'MatrixGame/src/MatrixVisiCalc.cpp'
    text = adapt((output / name).read_text(encoding='utf-8'))
    text = text.replace('D3DRS_ZENABLE', 'static_cast<srhd_awa::matrixgame::RasterRegister>(7)')
    text = text.replace('D3DZB_FALSE', '0u')
    text = text.replace('LPDWORD', 'std::uint32_t*')
    text = text.replace('ASSERT_DX(', 'VIS_ASSERT_DX(')
    write(name, '''#include "platform_3g.hpp"
#include "renderer_ops.hpp"
#define VIS_ASSERT_DX(call) srhd_awa::matrixgame::platform3g::CheckDeviceResult((call), #call)
namespace rp3g=srhd_awa::matrixgame::platform3g;
''' + text)
    source = (output / 'MatrixLib/3G/3g.cpp').read_text(encoding='utf-8')
    loop = function(source, 'int L3GRun()')
    loop = loop.replace('g_D3DD->GetAvailableTextureMem()', 'srhd_awa::matrixgame::platform3g::ReadyDevice().AvailableTextureBytes()')
    # MSVC chrono::rep is int64_t; preserve the original 100ms upper limit.
    loop = loop.replace('std::min(100LL, to_milliseconds(delta_time).count())',
                        'std::min<std::int64_t>(100, to_milliseconds(delta_time).count())')
    write('MatrixLib/3G/OriginalWindowsLoop.cpp', '''// Original 3g.cpp L3GRun; GPL-2.0-or-later.
#include <windows.h>
#include "3g.hpp"
#include "Form.hpp"
#include "CReminder.hpp"
#include "platform_3g.hpp"
#include "fps_counter.hpp"
#include "stupid_logger.hpp"
#include <chrono>
#include <thread>
#include <algorithm>
int g_DrawFPS;
''' + loop)
    # The first-frame driver calls the complete original form Draw method.
    # Only platform calls and the before-Present readback observer differ.
    source=(output/'MatrixGame/src/MatrixFormGame.cpp').read_text(encoding='utf-8')
    # Retain the entire original simulation step, including recovery and camera
    # input. The body has no platform API calls; only its exported name differs.
    # This does not fabricate a CForm/vtable or replace its input handlers.
    takt=function(source,'void CFormMatrixGame::Takt(int step)')
    takt=takt.replace('void CFormMatrixGame::Takt(int step)',
                      'void TaktOriginalNativeGame(int step)',1)
    write('MatrixGame/src/OriginalNativeTakt.cpp', '''// Complete original CFormMatrixGame::Takt; GPL-2.0-or-later.
#include "MatrixFormGame.hpp"
#include "MatrixMap.hpp"
#include "MatrixConfig.hpp"
#include "input.hpp"
using Input::isKeyPressed;
''' + takt)
    frame=function(source,'void CFormMatrixGame::Draw(void)')
    frame=frame.replace('void CFormMatrixGame::Draw(void)','void DrawOriginalWindowsFrame(void (*before_present)())')
    frame=frame.replace('g_D3DD->Clear(','frame_surface::Clear(')
    frame=frame.replace('g_D3DD->EndScene());','g_D3DD->EndScene());\n    if(before_present)before_present();',1)
    frame=frame.replace('g_D3DD->BeginScene()','frame_surface::BeginScene()').replace('g_D3DD->EndScene()','frame_surface::EndScene()')
    frame=frame.replace('g_D3DD->Present(NULL, NULL, NULL, NULL)','srhd_awa::matrixgame::transition::Present()')
    frame=frame.replace('D3DCLEAR_ZBUFFER','2u').replace('D3DCLEAR_STENCIL','4u').replace('D3DCLEAR_TARGET','1u')
    frame=frame.replace('D3DCOLOR_XRGB(255, 0, 255)','0xffff00ffu')
    write('MatrixGame/src/OriginalWindowsFrame.cpp', '''// Complete original CFormMatrixGame::Draw, GPL-2.0-or-later.
#include "MatrixMap.hpp"
#include "MatrixConfig.hpp"
#include "MatrixInstantDraw.hpp"
#include "Interface/CInterface.h"
#include "platform_win32.hpp"
#include "offscreen_backend.hpp"
#include "transition_backend.hpp"
#include "DeviceState.hpp"
#include "3g.hpp"
#include "utils.hpp"
namespace frame_surface=srhd_awa::matrixgame::offscreen;
#undef ASSERT_DX
#define ASSERT_DX(call) srhd_awa::matrixgame::platform3g::CheckDeviceResult((call),#call)
''' + frame)
    from matrixgame_form_overrides import prepare_native_form
    changes.update(prepare_native_form(output))
    return changes
