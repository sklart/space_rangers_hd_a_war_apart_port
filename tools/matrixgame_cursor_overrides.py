"""Complete original MatrixCursor through typed system/device/GDI operations."""
import hashlib
import re
from matrixgame_vector_object_overrides import function
from matrixgame_render_pipeline_overrides import adapt

def prepare_cursor(output):
    name='MatrixGame/src/MatrixCursor.hpp';path=output/name
    h=path.read_text(encoding='utf-8')
    cpath=output/'MatrixGame/src/MatrixCursor.cpp';c=cpath.read_text(encoding='utf-8')
    ctor=function(h,'    CMatrixCursor(void)')
    # Zero scalars explicitly, preserving the original values without corrupting
    # a live wstring. The complete original member layout is retained, plus the
    # borrowed typed Window needed after the platform state is detached.
    h=h.replace('#include "MatrixMap.hpp"','''#pragma once
#include "CMain.hpp"
#include "platform_3g.hpp"
#include <string>
class CTextureManaged;''')
    h=re.sub(r'\bHICON\b','srhd_awa::matrixgame::platform3g::IconHandle',h)
    h=re.sub(r'\bHCURSOR\b','srhd_awa::matrixgame::platform3g::CursorHandle',h)
    h=h.replace(ctor,'    CMatrixCursor(void);')
    h=h.replace('    int m_Time;','    int m_Time;\n    srhd_awa::matrixgame::platform3g::Window* m_CursorWindow;')
    ctor='''CMatrixCursor::CMatrixCursor(void)
    :m_CursorFlags(0),m_Pos(0,0),m_HotSpot(0,0),m_CursorTexture(nullptr),m_CursorSize(0),
     m_CursorIcons(nullptr),m_OldCursor{},m_FramesCnt(0),m_FrameInc(0),m_Frame(0),m_NextCursorTime(0),
     m_CursorTimePeriod(0),m_CursorInTexLine(0),m_TexSizeXInversed(0),m_TexSizeYInversed(0),
     m_u0(0),m_v0(0),m_u1(0),m_v1(0),m_Time(0),m_CursorWindow(&srhd_awa::matrixgame::platform3g::ReadyWindow()) {
    m_OldCursor=m_CursorWindow->ExchangeCursor({});
}
'''
    clear=function(c,'void CMatrixCursor::Clear(void)')
    native_clear=clear.replace('SetCursor(m_OldCursor);','m_CursorWindow->ExchangeCursor(m_OldCursor);')
    native_clear=native_clear.replace('DestroyIcon(m_CursorIcons[i]);','m_CursorWindow->DestroyCursorIcon(m_CursorIcons[i]);')
    # Keep the genuine virtual Unload contract while the concrete legacy texture
    # backend is still unported. No dummy Unload definition is introduced.
    native_clear=native_clear.replace('m_CursorTexture->Unload();','static_cast<CCacheData*>(m_CursorTexture)->Unload();')
    c=adapt(c.replace(clear,native_clear))
    c=c.replace('(CTextureManaged *)g_Cache->Get(CacheClass::TextureManaged, ',
                'srhd_awa::matrixgame::legacy_texture::GetManaged(')
    c=c.replace('g_D3DD->ShowCursor(', 'cursor_native::ShowDeviceCursor(')
    c=re.sub(r'\bShowCursor\(', 'cursor_native::ShowSystemCursor(',c)
    c=c.replace('SetCursor(NULL);','m_CursorWindow->ExchangeCursor({});')
    for index in ('m_Frame','0'):
        c=c.replace('SetCursor(m_CursorIcons['+index+']);',
                    'm_CursorWindow->ExchangeCursor({m_CursorIcons['+index+'].value});')
    c=re.sub(r'\bHICON\b','srhd_awa::matrixgame::platform3g::IconHandle',c)
    c=c.replace('WinBitmap frame;', 'cursor_native::BitmapFrame frame;')
    c=c.replace('ICONINFO ii;', 'cursor_native::IconInfo ii;')
    c=c.replace('CreateIconIndirect(&ii)', 'cursor_native::CreateIcon(ii)')
    c=c.replace('D3DRS_ZENABLE','static_cast<srhd_awa::matrixgame::RasterRegister>(7)')
    c=c.replace('D3DZB_FALSE','0u').replace('D3DZB_TRUE','1u')
    c='''#include "MatrixCursor.hpp"
#include "Texture.hpp"
#include "renderer_ops.hpp"
#include "cursor_backend.hpp"
namespace rp3g=srhd_awa::matrixgame::platform3g;
namespace cursor_native=srhd_awa::matrixgame::cursor;
#undef ASSERT_DX
#ifdef ASSERT_OFF
#define ASSERT_DX(call) (call)
#else
#define ASSERT_DX(call) rp3g::CheckDeviceResult((call), #call)
#endif
'''+ctor+'\n'+c
    path.write_bytes(h.encode());cpath.write_bytes(c.encode())
    return {name:hashlib.sha256(h.encode()).hexdigest(),'MatrixGame/src/MatrixCursor.cpp':hashlib.sha256(c.encode()).hexdigest()}
