"""Retain the complete original ApplyVideoParams behind typed native providers."""
import hashlib
import re
from matrixgame_vector_object_overrides import function

def prepare_video(output):
    name='MatrixGame/src/MatrixGame.cpp'
    path=output/name
    source=path.read_text(encoding='utf-8')
    original=function(source,'void CGame::ApplyVideoParams(SRobotsSettings &set)')
    body=original
    body=body.replace('::srhd_awa::matrixgame::ZeroTrivialBytes', 'ZeroMemory')
    body=body.replace('::srhd_awa::matrixgame::Viewport', 'p::VideoViewport')
    replacements={
        'D3DDISPLAYMODE':'p::DisplayMode','D3DDEVICE_CREATION_PARAMETERS':'p::CreationParameters',
        '_D3DMULTISAMPLE_TYPE':'std::uint32_t','D3DVIEWPORT9':'p::VideoViewport',
        'D3DMATERIAL9':'p::VideoMaterial','D3DLIGHT9':'p::VideoLight','RECT':'p::WindowRect',
        'D3DFMT_X8R8G8B8':'p::PixelFormat::Xrgb8','D3DFMT_A8R8G8B8':'p::PixelFormat::Argb8',
        'D3DFMT_R5G6B5':'p::PixelFormat::Rgb565','D3DFMT_D24S8':'p::PixelFormat::Depth24Stencil8',
        'D3DFMT_D16':'p::PixelFormat::Depth16','D3DFMT_D24X8':'p::PixelFormat::Depth24',
        'D3DSWAPEFFECT_DISCARD':'1u','D3DPRESENT_INTERVAL_ONE':'1u',
        'D3DPRESENT_INTERVAL_IMMEDIATE':'0x80000000u','D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL':'2u',
        'D3DADAPTER_DEFAULT':'0u','D3DDEVTYPE_HAL':'1u','D3DMULTISAMPLE_NONE':'0u',
        'D3DRS_MULTISAMPLEANTIALIAS':'static_cast<srhd_awa::matrixgame::RasterRegister>(161)',
        'D3DRS_SPECULARMATERIALSOURCE':'static_cast<srhd_awa::matrixgame::RasterRegister>(146)',
        'D3DMCS_MATERIAL':'0u','D3DLIGHT_DIRECTIONAL':'3u','D3D_OK':'p::DeviceSuccess',
        'g_D3Dpp':'p::Current().presentation','g_Flags':'p::Current().flags','g_Wnd':'p::Current().window_identity',
        'TRUE':'true','FALSE':'false',
    }
    for old,new in replacements.items():body=re.sub(r'\b'+old+r'\b',new,body)
    calls={
        'g_D3DD->GetCreationParameters(&params);':'params = p::ReadyDevice().ReadCreationParameters();',
        'ASSERT_DX(g_D3D->GetAdapterDisplayMode(params.AdapterOrdinal, &d3ddm));':'d3ddm = p::ReadyDevice().ReadAdapterDisplayMode(params.AdapterOrdinal);',
        'ZeroMemory(&p::Current().presentation, sizeof(p::Current().presentation));':'p::Current().presentation = {};',
        'GetClientRect(p::Current().window_identity, &rect);':'rect = p::ReadyWindow().ReadClientRect();',
        'GetWindowRect(p::Current().window_identity, &r1);':'r1 = p::ReadyWindow().ReadWindowRect();',
        'GetClientRect(p::Current().window_identity, &r2);':'r2 = p::ReadyWindow().ReadClientRect();',
        '''SetWindowPos(p::Current().window_identity, NULL, 0, 0, set.m_ResolutionX + (r1.right - r1.left - r2.right),
                     set.m_ResolutionY + (r1.bottom - r1.top - r2.bottom),
                     SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOACTIVATE);''':'''p::ReadyWindow().ResizeOuter(set.m_ResolutionX + (r1.right - r1.left - r2.right),
                     set.m_ResolutionY + (r1.bottom - r1.top - r2.bottom));''',
        'SetWindowLong(p::Current().window_identity, GWL_STYLE, WS_POPUP | WS_VISIBLE);':'p::ReadyWindow().SetFullscreenStyle();',
        'MoveWindow(p::Current().window_identity, 0, 0, set.m_ResolutionX, set.m_ResolutionY, false);':'p::ReadyWindow().MoveFullscreen(set.m_ResolutionX, set.m_ResolutionY);',
        '''SUCCEEDED(g_D3D->CheckDeviceMultiSampleType(0u, 1u, p::Current().presentation.BackBufferFormat,
                                                    p::Current().presentation.Windowed, expectedMultiSampleType, NULL))''':'''(p::ReadyDevice().CheckMultisample(0u, 1u, p::Current().presentation.BackBufferFormat,
                                                    p::Current().presentation.Windowed, expectedMultiSampleType) >= 0)''',
        'g_D3DD->Reset(&p::Current().presentation)':'p::ReadyDevice().ResetPresentation(p::Current().presentation)',
        'ZeroMemory(&ViewPort, sizeof(p::VideoViewport));':'ViewPort = {};',
        'ASSERT_DX(g_D3DD->SetViewport(&ViewPort));':'p::CheckDeviceResult(p::ReadyDevice().SetVideoViewport(ViewPort), "SetViewport");',
        'g_D3DD->SetRenderState(':'p::ReadyDevice().SetRenderState(',
        'ZeroMemory(&mtrl, sizeof(p::VideoMaterial));':'mtrl = {};',
        'g_D3DD->SetMaterial(&mtrl);':'p::ReadyDevice().SetVideoMaterial(mtrl);',
        'ZeroMemory(&light, sizeof(p::VideoLight));':'light = {};',
        'light.Direction = g_MatrixMap->m_LightMain;':'light.Direction = {g_MatrixMap->m_LightMain.x, g_MatrixMap->m_LightMain.y, g_MatrixMap->m_LightMain.z};',
        'ASSERT_DX(g_D3DD->SetLight(0, &light));':'p::CheckDeviceResult(p::ReadyDevice().SetVideoLight(0, light), "SetLight");',
        'ASSERT_DX(g_D3DD->LightEnable(0, true));':'p::CheckDeviceResult(p::ReadyDevice().EnableVideoLight(0, true), "LightEnable");',
        'lgr.debug("Apply window mode");':'std::clog << "Apply window mode\\n";',
        'lgr.debug("Apply fullscreen mode");':'std::clog << "Apply fullscreen mode\\n";',
    }
    for old,new in calls.items():
        if old not in body:raise ValueError('Original video call changed: '+old)
        body=body.replace(old,new)
    # The same complete adapted method is used by the full-TU compile audit.
    source=source.replace(original,body)
    if source.count('timeBeginPeriod(1);')!=1 or source.count('timeEndPeriod(1);')!=1:
        raise ValueError('Original loop timer call sites changed')
    source=source.replace('timeBeginPeriod(1);','p::BeginLoopTimerPeriod(1);')
    source=source.replace('timeEndPeriod(1);','p::EndLoopTimerPeriod(1);')
    # The integrated owner provides these real engine globals once. The full
    # source view must not define a second heap/map when its closure is linked.
    source=source.replace('CHeap *g_MatrixHeap;', 'extern CHeap *g_MatrixHeap;')
    source=source.replace('CMatrixMapLogic *g_MatrixMap;', 'extern CMatrixMapLogic *g_MatrixMap;')
    if source.count('logger_type lgr{"test.log"};')!=1:raise ValueError('Original game logger changed')
    source=source.replace('logger_type lgr{"test.log"};','logger_type lgr{std::clog};')
    prelude='#include "platform_3g.hpp"\n#include <iostream>\nnamespace p=srhd_awa::matrixgame::platform3g;\n'
    path.write_bytes((prelude+source).encode())
    # The full original TU now supplies the called method. Retire only this
    # previously managed generated file, never a user source or upstream file.
    selected=output/'MatrixGame/src/GameVideoNative.cpp'
    if selected.exists():selected.unlink()
    return {name:hashlib.sha256(path.read_bytes()).hexdigest()}
