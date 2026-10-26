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
    if text.count('fs::path filepath{filename};')!=1 or text.count('std::filesystem::exists(filename.c_str(), ec)')!=1:raise ValueError('Minimap cache filesystem boundaries changed')
    text=text.replace('fs::path filepath{filename};','fs::path filepath{mm_output::FilesystemPath(filename)};')
    text=text.replace('std::filesystem::exists(filename.c_str(), ec)','std::filesystem::exists(mm_output::FilesystemPath(filename), ec)')
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

    # Same original calls and draw algorithms; own every temporary across native
    # exception exits, and keep staging data until successful resource adoption.
    from matrixgame_vector_object_overrides import function
    body=function(text,'void CMinimap::RestoreTexture(void)')
    old='''    mm_texture::Release(m_TextureStore);
    m_TextureStore = NULL;

    m_Texture = mm_texture::CreateUnmanaged();
    m_Texture->Set(newTexture);'''
    if body.count(old)!=1:raise ValueError('Original minimap restore ownership boundary changed')
    body=body.replace(old,'''    std::unique_ptr<CTexture, MinimapCacheDeleter> candidate{mm_texture::CreateUnmanaged()};
    candidate->Set(newTexture); newTexture.detach();
    m_Texture = candidate.release();
    mm_texture::Release(m_TextureStore);
    m_TextureStore = NULL;''')
    text=text.replace(function(text,'void CMinimap::RestoreTexture(void)'),body,1)
    pattern=r'IDirect3DSurface9\s*\*\s*(\w+)(?:,\s*\*\s*(\w+)(?:\s*=\s*NULL)?)?\s*;'
    text,count=re.subn(pattern,lambda m:'mm_offscreen::SurfaceOwner '+m[1]+(', '+m[2] if m[2] else '')+';',text)
    if count!=9:raise ValueError('Minimap surface declaration count changed')
    for name in ('surf_to','surf_from','oldZ','newZ','oldTarget','newTarget','outtgt'):
        text=re.sub(r'&'+name+r'\b',name+'.put()',text)
        text=text.replace('mm_offscreen::Release('+name+')',name+'.reset()')
    text,count=re.subn(r'IDirect3DTexture9\s*\*\s*(newTexture|loaded_tex|out)\s*;',r'mm_lifetime::TextureOwner \1;',text)
    if count!=4:raise ValueError('Minimap temporary texture declaration count changed')
    for name in ('newTexture','loaded_tex','out'):
        if name=='out':
            original=function(text,'bool CMinimap::StoreMinimapToCache(')
            body=re.sub(r'&out\b','out.put()',original)
            text=text.replace(original,body,1)
        else:text=re.sub(r'&'+name+r'\b',name+'.put()',text)
        text=text.replace('mm_texture::Release('+name+')',name+'.reset()')
    if text.count('    m_Texture->Set(newTexture);')!=1:raise ValueError('Minimap render result adoption changed')
    text=text.replace('    m_Texture->Set(newTexture);','    m_Texture->Set(newTexture); newTexture.detach();')
    if text.count('StoreMinimapToCache(mmname, newTexture);')!=1:raise ValueError('Minimap adopted cache save source changed')
    text=text.replace('StoreMinimapToCache(mmname, newTexture);','StoreMinimapToCache(mmname, m_Texture->DX());')
    early='''    if (LoadMinimapFromCache(mmname, newTexture))
    {
        return;
    }'''
    if text.count(early)!=1:raise ValueError('Minimap warm-cache adoption changed')
    text=text.replace(early,early.replace('        return;','        newTexture.detach();\n        return;'))
    # Any failure while creating/copying the system backup clears that incomplete
    # slot. Success keeps it owned by the original CMinimap until restore/clear.
    body=function(text,'void CMinimap::StoreTexture(void)')
    marker='    mm_offscreen::SurfaceOwner surf_to, surf_from;'
    if body.count(marker)!=1:raise ValueError('Minimap store staging owner changed')
    body=body.replace(marker,'    mm_lifetime::TextureSlotRollback store_backup{m_TextureStore};\n'+marker)
    body=body.replace('\nreally_lost:','\n    store_backup.Commit();\n\nreally_lost:')
    text=text.replace(function(text,'void CMinimap::StoreTexture(void)'),body,1)
    for signature in ('void CMinimap::RenderBackground(', 'void CMinimap::RenderObjectToBackground('):
        original=function(text,signature);body=original
        flag='    SETFLAG(g_MatrixMap->m_Flags, MMFLAG_DISABLE_DRAW_OBJECT_LIGHTS);'
        if body.count(flag)!=1:raise ValueError('Minimap render flag boundary changed')
        body=body.replace(flag,'''    mm_lifetime::FlagRollback flag_restore{g_MatrixMap->m_Flags, MMFLAG_DISABLE_DRAW_OBJECT_LIGHTS};
    mm_offscreen::TargetScope minimap_targets;
    mm_offscreen::SceneScope minimap_scene;
'''+flag)
        body=body.replace('ASSERT_DX(mm_offscreen::BeginScene());','minimap_scene.Begin();')
        body=body.replace('ASSERT_DX(mm_offscreen::EndScene());','minimap_scene.End();')
        viewport='    ASSERT_DX(mm_offscreen::SetViewport(&oldViewport));'
        if body.count(viewport)!=1:raise ValueError('Minimap viewport restoration changed')
        body=body.replace(viewport,viewport+'\n    FAILED_DX(minimap_targets.Restore());')
        reset='    RESETFLAG(g_MatrixMap->m_Flags, MMFLAG_DISABLE_DRAW_OBJECT_LIGHTS);'
        if body.count(reset)!=1:raise ValueError('Minimap normal flag reset changed')
        body=body.replace(reset,reset+'\n    flag_restore.Commit();')
        if 'RenderObjectToBackground' in signature:
            body=body.replace('    bool f1 = g_Config.m_ShowStencilShadows;','    mm_lifetime::ValueRollback stencil_restore{g_Config.m_ShowStencilShadows};\n    mm_lifetime::ValueRollback projected_restore{g_Config.m_ShowProjShadows};\n    bool f1 = g_Config.m_ShowStencilShadows;')
            body=body.replace('    g_Config.m_ShowProjShadows = f2;','    g_Config.m_ShowProjShadows = f2;\n    stencil_restore.Commit(); projected_restore.Commit();')
        text=text.replace(original,body,1)
    # SaveInPNG can throw after LockRect. The bitmap views die before the lock,
    # then the system texture and surfaces are released in the original order.
    lock='    mm_offscreen::Lock(outtgt, &lr, NULL, 0);'
    if text.count(lock)!=1:raise ValueError('Minimap cache lock changed')
    text=text.replace(lock,'    mm_offscreen::SurfaceLockScope cache_lock{outtgt,lr};')
    text=text.replace('    mm_offscreen::Unlock(outtgt);','    cache_lock.Unlock();')
    for prefix,expected in [('mm_offscreen::CreateDepth(',2),('mm_offscreen::GetDepth(',2),('mm_offscreen::Describe(',2),('mm_offscreen::SetDepth(',4),('mm_unmanaged::Update(',2)]:
        text,count=re.subn(r'(?m)^(\s*)('+re.escape(prefix)+r'[^;]+\));',lambda m:m[1]+'FAILED_DX('+m[2]+');',text)
        if count!=expected:raise ValueError('Required minimap call count changed: '+prefix+' '+str(count))

    prelude='''#include "Texture.hpp"
#include "unmanaged_texture_backend.hpp"
#include "output_paths_backend.hpp"
#include "offscreen_scope.hpp"
#include "minimap_lifetime.hpp"
#include <memory>
namespace {struct MinimapCacheDeleter {void operator()(CTexture* p)const{if(p)CCache::Destroy(p);}};}
#include "map_device_backend.hpp"
#include "renderer_ops.hpp"
#include "minimap_geometry.hpp"
#include "view_geometry.hpp"
namespace rp3g=srhd_awa::matrixgame::platform3g;
namespace mm_output=srhd_awa::matrixgame::output_paths;
namespace mm_texture=srhd_awa::matrixgame::legacy_texture;
namespace mm_unmanaged=srhd_awa::matrixgame::unmanaged_texture;
namespace mm_offscreen=srhd_awa::matrixgame::offscreen;
namespace mm_device=srhd_awa::matrixgame::map_device;
namespace mm_geometry=srhd_awa::matrixgame::minimap_geometry;
namespace mm_lifetime=srhd_awa::matrixgame::minimap_lifetime;
'''
    marker='#define MINIMAP_Z'
    text=text.replace(marker,'''#undef FAILED_DX
#define FAILED_DX(call) rp3g::CheckDeviceResult((call), #call)
#undef ASSERT_DX
#define ASSERT_DX(call) FAILED_DX(call)
'''+marker,1)
    path.write_bytes((prelude+text).encode())
    return {'MatrixGame/src/MatrixMinimap.cpp':hashlib.sha256(path.read_bytes()).hexdigest()}
