"""Original shadow texture generation with real SDK surfaces and staging draw."""
import hashlib
import re
from pathlib import Path
from matrixgame_render_pipeline_overrides import adapt
from matrixgame_vector_object_overrides import function


def prepare_shadow_texture(output):
    source=(output/'MatrixLib/3G/VectorObject.cpp').read_text(encoding='utf-8')
    signatures=['CTextureManaged *CVectorObject::CalcShadowTexture(int cnt,',
                'CTextureManaged *CVectorObject::CalcShadowTextureWOMat(int cnt,']
    text='\n\n'.join(function(source,s) for s in signatures)
    # The shared CPU declaration uses a recorder. This SDK consumer instead
    # executes the exact original inline DrawBegin, with a real FVF operation.
    header=(Path(__file__).resolve().parents[1]/'upstream/matrixgame/MatrixLib/3G/VectorObject.hpp').read_text(encoding='utf-8')
    begin=function(header,'static void DrawBegin(void)');begin=begin[begin.index('{')+1:-1]
    text=text.replace('CVectorObject::DrawBegin();','{'+begin+'}')
    text=adapt(text)
    values={'D3DVIEWPORT9':'rp3g::VideoViewport','LPDIRECT3DTEXTURE9':'IDirect3DTexture9*',
        'D3DSURFACE_DESC':'shadow_surface::SurfaceDesc','D3DLOCKED_RECT':'shadow_texture::LockedRect',
        'D3D_OK':'0','D3DUSAGE_RENDERTARGET':'1u','D3DFMT_A8':'28u','D3DFMT_A8L8':'51u',
        'D3DFMT_A8R3G3B2':'29u','D3DFMT_A8R8G8B8':'21u','D3DPOOL_DEFAULT':'0u',
        'D3DMULTISAMPLE_NONE':'0u','D3DCLEAR_TARGET':'1u','D3DCLEAR_ZBUFFER':'2u','D3DCLEAR_STENCIL':'4u',
        'D3DRS_ZENABLE':'static_cast<srhd_awa::matrixgame::RasterRegister>(7)',
        'D3DRS_CULLMODE':'static_cast<srhd_awa::matrixgame::RasterRegister>(22)',
        'D3DRS_ALPHAREF':'static_cast<srhd_awa::matrixgame::RasterRegister>(24)',
        'D3DRS_ALPHAFUNC':'static_cast<srhd_awa::matrixgame::RasterRegister>(25)',
        'D3DZB_TRUE':'1u','D3DZB_FALSE':'0u','D3DCULL_NONE':'1u','D3DCULL_CW':'2u','D3DCULL_CCW':'3u',
        'D3DCMP_GREATEREQUAL':'7u','D3DTEXF_POINT':'1u','D3DTEXF_LINEAR':'2u',
        'D3DSAMP_MAGFILTER':'srhd_awa::matrixgame::SamplerRegister::MagFilter',
        'D3DSAMP_MINFILTER':'srhd_awa::matrixgame::SamplerRegister::MinFilter','byte':'std::uint8_t'}
    for old,new in values.items():text=re.sub(r'\b'+old+r'\b',lambda m:new,text)
    for fmt in ['28u','51u','29u','21u']:text=text.replace('CreateLock('+fmt+',','CreateLock(static_cast<TexturePixelFormat>('+fmt+'),')
    for kind in ['WORLD','VIEW','PROJECTION']:
        text=text.replace('g_D3DD->SetTransform(D3DTS_'+kind+',','shadow_device::SetTransform(shadow_device::Transform::'+kind.title()+',')
    for old,new in {'GetViewport':'GetViewport','SetViewport':'SetViewport','GetRenderTarget':'GetTarget',
        'SetRenderTarget':'SetTarget','CreateRenderTarget':'CreateTarget','GetDepthStencilSurface':'GetDepth',
        'SetDepthStencilSurface':'SetDepth','CreateDepthStencilSurface':'CreateDepth',
        'Clear':'Clear','BeginScene':'BeginScene','EndScene':'EndScene'}.items():
        text=text.replace('g_D3DD->'+old+'(','shadow_surface::'+new+'(')
    text=text.replace('D3DXCreateTexture(g_D3DD,','shadow_gpu::CreateRenderTexture(')
    text=text.replace('D3DXMatrixLookAtLH(','srhd_awa::matrixgame::geometry::LookAtLH(')
    text=text.replace('g_D3DD->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,','shadow_gpu::DrawIndexedStaging(4u,')
    text=text.replace('newTexture->GetSurfaceLevel(0, &newTarget)','shadow_unmanaged::Surface(newTexture,0,&newTarget)')
    text=text.replace('tex_to_update->DX()->GetSurfaceLevel(0, &newTarget)','shadow_unmanaged::Surface(tex_to_update->DX(),0,&newTarget)')
    text=text.replace('oldZ->GetDesc(&d)','shadow_surface::Describe(oldZ,&d)')
    text=text.replace('newTarget->LockRect(&lor, NULL, 0)','shadow_surface::Lock(newTarget,&lor,nullptr,0)')
    text=text.replace('newTarget->UnlockRect()','shadow_surface::Unlock(newTarget)')
    for name in ['newTarget','oldTarget','newZ','oldZ']:text=text.replace(name+'->Release()','shadow_surface::Release('+name+')')
    text=text.replace('CACHE_CREATE_TEXTUREMANAGED()','shadow_texture::CreateManaged()')
    prelude='''// Both original CVectorObject shadow texture methods; genuine SDK path.
#include "VectorObject.hpp"
#include "ShadowProj.hpp"
#include "Texture.hpp"
#include "view_geometry.hpp"
#include "map_device_backend.hpp"
#include "renderer_ops.hpp"
#include "MatrixSampleStateManager.hpp"
#include "unmanaged_texture_backend.hpp"
#include "shadow_texture_backend.hpp"
namespace rp3g=srhd_awa::matrixgame::platform3g;
namespace shadow_device=srhd_awa::matrixgame::map_device;
namespace shadow_surface=srhd_awa::matrixgame::offscreen;
namespace shadow_texture=srhd_awa::matrixgame::legacy_texture;
namespace shadow_unmanaged=srhd_awa::matrixgame::unmanaged_texture;
namespace shadow_gpu=srhd_awa::matrixgame::shadow_texture;
#undef FAILED_DX
#define FAILED_DX(call) rp3g::CheckDeviceResult((call), #call)
#undef ASSERT_DX
#ifdef ASSERT_OFF
#define ASSERT_DX(call) (call)
#else
#define ASSERT_DX(call) FAILED_DX(call)
#endif
'''
    name='MatrixLib/3G/OriginalShadowTexture.cpp';path=output/name;path.write_bytes((prelude+text+'\n').encode())
    return {name:hashlib.sha256(path.read_bytes()).hexdigest()}
