"""Complete original static-object TU, with real offscreen SDK boundaries."""
import hashlib
import re
from matrixgame_render_pipeline_overrides import adapt


def prepare_map_static(output):
    name='MatrixGame/src/MatrixMapStatic.cpp';path=output/name
    text=path.read_text(encoding='utf-8')
    if text.count('CACHE_CREATE_TEXTUREMANAGED()')!=1:raise ValueError('Original portrait texture factory changed')
    text=text.replace('CACHE_CREATE_TEXTUREMANAGED()','static_texture::CreateManaged()')
    text=adapt(text)
    values={'D3DFMT_A8R8G8B8':'static_cast<TexturePixelFormat>(21u)',
        'D3DMULTISAMPLE_NONE':'0u','D3D_OK':'0','D3DLIGHT9':'rp3g::VideoLight',
        'D3DLIGHT_DIRECTIONAL':'3u','D3DSURFACE_DESC':'static_offscreen::SurfaceDesc',
        'D3DLOCKED_RECT':'static_texture::LockedRect','D3DCLEAR_TARGET':'1u',
        'D3DCLEAR_ZBUFFER':'2u','D3DCLEAR_STENCIL':'4u','D3DX_PI':'DirectX::XM_PI',
        'D3DRS_ZENABLE':'static_cast<srhd_awa::matrixgame::RasterRegister>(7)',
        'D3DRS_NORMALIZENORMALS':'static_cast<srhd_awa::matrixgame::RasterRegister>(143)'}
    for token,value in values.items():text=re.sub(r'\b'+token+r'\b',lambda m:value,text)
    # CreateTarget takes the serialized format; original texture CreateLock
    # keeps its stronger enum overload to avoid accidental bool conversion.
    text=text.replace('CreateRenderTarget(RENDSZ, RENDSZ, static_cast<TexturePixelFormat>(21u),',
                      'CreateRenderTarget(RENDSZ, RENDSZ, 21u,')
    calls={'CreateRenderTarget':'CreateTarget','CreateDepthStencilSurface':'CreateDepth',
        'GetDepthStencilSurface':'GetDepth','SetDepthStencilSurface':'SetDepth',
        'GetRenderTarget':'GetTarget','SetRenderTarget':'SetTarget','SetViewport':'SetViewport',
        'BeginScene':'BeginScene','EndScene':'EndScene','Clear':'Clear'}
    for old,new in calls.items():text=text.replace('g_D3DD->'+old+'(','static_offscreen::'+new+'(')
    text=text.replace('g_D3DD->SetTransform(D3DTS_VIEW,','static_device::SetTransform(static_device::Transform::View,')
    text=text.replace('g_D3DD->SetTransform(D3DTS_PROJECTION,','static_device::SetTransform(static_device::Transform::Projection,')
    text=text.replace('g_D3DD->SetLight(0, &light)','rp3g::ReadyDevice().SetVideoLight(0, light)')
    text=text.replace('g_D3DD->LightEnable(0, true)','rp3g::ReadyDevice().EnableVideoLight(0, true)')
    text=text.replace('light.Direction = D3DXVECTOR3(-0.82242596f, 0.56887215f, 0);',
                      'light.Direction = {-0.82242596f, 0.56887215f, 0};')
    text=text.replace('light.Direction = g_MatrixMap->m_LightMain;',
                      'light.Direction = {g_MatrixMap->m_LightMain.x,g_MatrixMap->m_LightMain.y,g_MatrixMap->m_LightMain.z};')
    text=text.replace('oldZ->GetDesc(&d)','static_offscreen::Describe(oldZ,&d)')
    text=text.replace('D3DXMatrixLookAtLH(','srhd_awa::matrixgame::geometry::LookAtLH(')
    text=text.replace('newTarget->LockRect(&lor, NULL, 0)','static_offscreen::Lock(newTarget,&lor,nullptr,0)')
    text=text.replace('newTarget->UnlockRect()','static_offscreen::Unlock(newTarget)')
    for handle in ['oldZ','newZ','oldTarget','newTarget']:
        text=text.replace(handle+'->Release()','static_offscreen::Release('+handle+')')
    marker='bool FreeObjResources('
    if text.count(marker)!=1:raise ValueError('Original static-object resource callback changed')
    check='''#undef ASSERT_DX
#ifdef ASSERT_OFF
#define ASSERT_DX(call) (call)
#else
#define ASSERT_DX(call) rp3g::CheckDeviceResult((call), #call)
#endif
'''
    text=text.replace(marker,check+'\n'+marker)
    prelude='''#include "Texture.hpp"
#include "offscreen_backend.hpp"
#include "map_device_backend.hpp"
#include "renderer_ops.hpp"
#include "view_geometry.hpp"
namespace rp3g=srhd_awa::matrixgame::platform3g;
namespace static_device=srhd_awa::matrixgame::map_device;
namespace static_offscreen=srhd_awa::matrixgame::offscreen;
namespace static_texture=srhd_awa::matrixgame::legacy_texture;
'''
    path.write_bytes((prelude+text).encode())
    return {name:hashlib.sha256(path.read_bytes()).hexdigest()}
