"""Adapt the full original minimap; retain world and GPU algorithms."""
import hashlib
import re
from matrixgame_render_pipeline_overrides import adapt


def prepare_minimap(output):
    name='MatrixGame/src/MatrixMinimap.cpp';path=output/name
    text=path.read_text(encoding='utf-8')
    if text.count('CACHE_CREATE_TEXTURE()')!=2:raise ValueError('Minimap factories changed')
    text=text.replace('CACHE_CREATE_TEXTURE()', 'mm_texture::CreateUnmanaged()')
    old='(CTextureManaged *)g_Cache->Get(CacheClass::TextureManaged, n.GetStrPar(0, L",").c_str())'
    if text.count(old)!=1:raise ValueError('Minimap managed lookup changed')
    text=text.replace(old,'mm_texture::GetManaged(n.GetStrPar(0, L",").c_str())')
    text=adapt(text)
    values={'D3D_OK':'0','D3DPOOL_SYSTEMMEM':'2u','D3DPOOL_DEFAULT':'0u',
        'D3DUSAGE_RENDERTARGET':'1u','rp3g::PixelFormat::Xrgb8':'22u',
        'D3DSURFACE_DESC':'mm_offscreen::SurfaceDesc','D3DLOCKED_RECT':'mm_texture::LockedRect',
        'D3DLIGHT9':'rp3g::VideoLight','D3DMATERIAL9':'rp3g::VideoMaterial',
        'D3DLIGHT_DIRECTIONAL':'3u','D3DMCS_MATERIAL':'0u','D3DZB_TRUE':'1u',
        'D3DCLEAR_TARGET':'1u','D3DCLEAR_ZBUFFER':'2u','D3DCLEAR_STENCIL':'4u',
        'D3DTEXF_POINT':'1u','D3DTEXF_NONE':'0u','D3DTEXF_LINEAR':'2u','D3DPT_LINESTRIP':'3u',
        'D3DSAMP_MAGFILTER':'srhd_awa::matrixgame::SamplerRegister::MagFilter',
        'D3DSAMP_MINFILTER':'srhd_awa::matrixgame::SamplerRegister::MinFilter',
        'D3DSAMP_MIPFILTER':'srhd_awa::matrixgame::SamplerRegister::MipFilter',
        'D3DRS_ZENABLE':'static_cast<srhd_awa::matrixgame::RasterRegister>(7)',
        'D3DRS_SPECULARMATERIALSOURCE':'static_cast<srhd_awa::matrixgame::RasterRegister>(146)'}
    for old,new in values.items():text=text.replace(old,new)
    for old,new in {'CreateTexture':'mm_texture::Create','UpdateTexture':'mm_unmanaged::Update',
        'GetRenderTargetData':'mm_offscreen::Readback','GetDepthStencilSurface':'mm_offscreen::GetDepth',
        'SetDepthStencilSurface':'mm_offscreen::SetDepth','CreateDepthStencilSurface':'mm_offscreen::CreateDepth',
        'GetViewport':'mm_offscreen::GetViewport','SetViewport':'mm_offscreen::SetViewport',
        'GetRenderTarget':'mm_offscreen::GetTarget','SetRenderTarget':'mm_offscreen::SetTarget',
        'BeginScene':'mm_offscreen::BeginScene','EndScene':'mm_offscreen::EndScene','Clear':'mm_offscreen::Clear'}.items():
        text=text.replace('g_D3DD->'+old+'(',new+'(')
    for handle in ['m_TextureStore','loaded_tex','texture','out','newTexture']:
        text=text.replace(handle+'->Release()','mm_texture::Release('+handle+')')
        text=text.replace(handle+'->GetSurfaceLevel(', 'mm_unmanaged::Surface('+handle+', ')
        text=text.replace(handle+'->LockRect(', 'mm_texture::Lock('+handle+', ')
        text=text.replace(handle+'->UnlockRect(', 'mm_texture::Unlock('+handle+', ')
    text=text.replace('m_Texture->Tex()->GetSurfaceLevel(', 'mm_unmanaged::Surface(m_Texture->Tex(), ')
    text=text.replace('m_TextureStore->AddDirtyRect(NULL)','mm_unmanaged::Dirty(m_TextureStore,nullptr)')
    text=text.replace('D3DXLoadSurfaceFromSurface(surf_to, NULL, NULL, surf_from, NULL, NULL, D3DX_FILTER_NONE, 0)',
        'mm_unmanaged::CopySurface(surf_to,surf_from)')
    for handle in ['surf_to','surf_from','oldZ','newZ','oldTarget','newTarget','outtgt']:
        text=text.replace(handle+'->Release()','mm_offscreen::Release('+handle+')')
        text=text.replace(handle+'->GetDesc(', 'mm_offscreen::Describe('+handle+', ')
        text=text.replace(handle+'->LockRect(', 'mm_offscreen::Lock('+handle+', ')
        text=text.replace(handle+'->UnlockRect()', 'mm_offscreen::Unlock('+handle+')')
    for kind in ['VIEW','WORLD','PROJECTION']:
        text=text.replace('g_D3DD->SetTransform(D3DTS_'+kind+',','mm_device::SetTransform(mm_device::Transform::'+kind.title()+',')
    text=text.replace('g_D3DD->SetMaterial(&mtrl)','rp3g::ReadyDevice().SetVideoMaterial(mtrl)')
    text=text.replace('g_D3DD->DrawPrimitiveUP(', 'mm_device::DrawPrimitiveUP(')
    text=text.replace('g_D3DD->SetLight(0, &light)','rp3g::ReadyDevice().SetVideoLight(0, light)')
    text=text.replace('g_D3DD->LightEnable(0, true)','rp3g::ReadyDevice().EnableVideoLight(0, true)')
    text=text.replace('D3DXMatrixTranslation(', 'mm_geometry::Translation(')
    text=text.replace('D3DXMatrixOrthoLH(', 'mm_geometry::OrthoLH(')
    text=text.replace('D3DXMatrixLookAtLH(', 'srhd_awa::matrixgame::geometry::LookAtLH(')
    text=text.replace('D3DXVec2TransformCoordArray(', 'mm_geometry::TransformCoordArray(')
    text=text.replace('D3DXVec2TransformCoord(', 'mm_geometry::TransformCoord(')
    text=text.replace('D3DCOLOR_XRGB(0, 0, 0)','0xff000000u')
    text=text.replace('light.Direction = g_MatrixMap->m_LightMain;',
        'light.Direction = {g_MatrixMap->m_LightMain.x,g_MatrixMap->m_LightMain.y,g_MatrixMap->m_LightMain.z};')
    prelude='''#include "Texture.hpp"
#include "unmanaged_texture_backend.hpp"
#include "map_device_backend.hpp"
#include "renderer_ops.hpp"
#include "minimap_geometry.hpp"
#include "view_geometry.hpp"
namespace rp3g=srhd_awa::matrixgame::platform3g;
namespace mm_texture=srhd_awa::matrixgame::legacy_texture;
namespace mm_unmanaged=srhd_awa::matrixgame::unmanaged_texture;
namespace mm_offscreen=srhd_awa::matrixgame::offscreen;
namespace mm_device=srhd_awa::matrixgame::map_device;
namespace mm_geometry=srhd_awa::matrixgame::minimap_geometry;
'''
    marker='#define MINIMAP_Z'
    text=text.replace(marker,'''#undef FAILED_DX
#define FAILED_DX(call) rp3g::CheckDeviceResult((call), #call)
#undef ASSERT_DX
#ifdef ASSERT_OFF
#define ASSERT_DX(call) (call)
#else
#define ASSERT_DX(call) FAILED_DX(call)
#endif
'''+marker,1)
    path.write_bytes((prelude+text).encode())
    return {name:hashlib.sha256(path.read_bytes()).hexdigest()}
