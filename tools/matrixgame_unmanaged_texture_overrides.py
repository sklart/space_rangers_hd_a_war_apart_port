"""Original CTexture methods and device-loss lifecycle through actual SDK APIs."""
import hashlib
import re
from matrixgame_vector_object_overrides import function


def prepare_unmanaged_texture(output):
    changes={};name='MatrixLib/3G/Texture.cpp';path=output/name
    source=path.read_text(encoding='utf-8');chunks=[]
    signatures=['void CTexture::OnLostDevice(void)','void CTexture::OnResetDevice(void)',
        'void CTexture::LoadFromBitmap(CBitmap &bm)','void CTexture::Unload(void)',
        'void CTexture::Load(void)','void CTexture::MakeSolidTexture(int sx, int sy, std::uint32_t color)']
    for signature in signatures:
        original=function(source,signature);body=original
        if signature=='void CTexture::LoadFromBitmap(CBitmap &bm)':
            if original.count('D3DXCreateTextureFromFileInMemoryEx(')!=1 or 'D3DX_FILTER_TRIANGLE | D3DX_FILTER_DITHER' not in original:
                raise ValueError('Original unmanaged bitmap decode contract changed')
            start=body.index('D3DXCreateTextureFromFileInMemoryEx(');end=body.index('&m_Tex)',start)+len('&m_Tex)')
            body=body[:start]+'ut::DecodeBitmap(buf.Get(),buf.Len(),FLAG(m_Flags, TF_NOMIPMAP) ? 1u : 0u,&m_Tex)'+body[end:]
        body=body.replace('D3DSURFACE_DESC','ut::ResourceDesc').replace('D3DLOCKED_RECT','lt::LockedRect')
        body=re.sub(r'\bHRESULT\b','std::int32_t',body)
        for token,value in {'D3D_OK':'0','D3DPOOL_DEFAULT':'0u','D3DPOOL_SYSTEMMEM':'2u','D3DFMT_A8R8G8B8':'21u'}.items():body=re.sub(r'\b'+token+r'\b',value,body)
        body=body.replace('g_D3DD->GetAvailableTextureMem()','utp::ReadyDevice().AvailableTextureBytes()')
        body=body.replace('g_D3DD->CreateTexture(','lt::Create(').replace('g_D3DD->UpdateTexture(','ut::Update(')
        body=body.replace('m_Tex->GetLevelDesc(', 'ut::Describe(m_Tex, ').replace('m_Tex->GetLevelCount()', 'lt::LevelCount(m_Tex)')
        for texture in ['m_Tex','sm_tex','tex']:
            body=body.replace(texture+'->GetSurfaceLevel(', 'ut::Surface('+texture+', ')
            body=body.replace(texture+'->LockRect(', 'lt::Lock('+texture+', ').replace(texture+'->UnlockRect(', 'lt::Unlock('+texture+', ')
            body=body.replace(texture+'->AddDirtyRect(NULL)','ut::Dirty('+texture+', nullptr)')
            body=body.replace(texture+'->Release()', 'lt::Release('+texture+')')
        for surface in ['from','to']:body=body.replace(surface+'->Release()', 'os::Release('+surface+')')
        body=body.replace('D3DXLoadSurfaceFromSurface(to, NULL, NULL, from, NULL, NULL, D3DX_FILTER_NONE, 0)','ut::CopySurface(to, from)')
        chunks.append(body);source=source.replace(original,body,1)
    prelude='''#include "Texture.hpp"
#include "CStorage.hpp"
#include "unmanaged_texture_backend.hpp"
namespace lt=srhd_awa::matrixgame::legacy_texture;
namespace ut=srhd_awa::matrixgame::unmanaged_texture;
namespace os=srhd_awa::matrixgame::offscreen;
namespace utp=srhd_awa::matrixgame::platform3g;
#undef FAILED_DX
#define FAILED_DX(call) utp::CheckDeviceResult((call), #call)
#undef ASSERT_DX
#ifdef ASSERT_OFF
#define ASSERT_DX(call) (call)
#else
#define ASSERT_DX(call) FAILED_DX(call)
#endif
'''
    generated='MatrixLib/3G/OriginalUnmanagedTexture.cpp';gp=output/generated
    gp.write_bytes((prelude+'\n\n'.join(chunks)+'\n').encode());changes[generated]=hashlib.sha256(gp.read_bytes()).hexdigest()
    # Keep the same method bodies in the full future Texture TU as well.
    source='#include "unmanaged_texture_backend.hpp"\nnamespace ut=srhd_awa::matrixgame::unmanaged_texture;\nnamespace os=srhd_awa::matrixgame::offscreen;\nnamespace utp=srhd_awa::matrixgame::platform3g;\n'+source
    path.write_bytes(source.encode());changes[name]=hashlib.sha256(path.read_bytes()).hexdigest()
    # The real registry lives in this existing TU. Its original dispatch visits
    # unmanaged textures only; do not synthesize a second registry.
    full=(output/name).read_text(encoding='utf-8')
    managed='MatrixLib/3G/OriginalManagedTexture.cpp';mp=output/managed;m=mp.read_text(encoding='utf-8')
    marker='CTextureManaged* CreateManaged(){return HNew(g_CacheHeap) CTextureManaged;}'
    if m.count(marker)!=1:raise ValueError('Original managed factory entry changed')
    m=m.replace(marker,marker+'\nCTexture* CreateUnmanaged(){return HNew(g_CacheHeap) CTexture;}')
    for signature in ['void CBaseTexture::OnLostDevice(void)','void CBaseTexture::OnResetDevice(void)']:
        m+='\n'+function(full,signature)+'\n'
    mp.write_bytes(m.encode());changes[managed]=hashlib.sha256(mp.read_bytes()).hexdigest()
    return changes
