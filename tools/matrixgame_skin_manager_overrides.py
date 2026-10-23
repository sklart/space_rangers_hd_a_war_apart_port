"""Complete original skin manager through existing typed texture/device APIs."""
import hashlib
from matrixgame_render_pipeline_overrides import adapt
from matrixgame_vector_object_overrides import function


def prepare_skin_manager(output):
    name='MatrixGame/src/MatrixSkinManager.cpp';path=output/name
    text=path.read_text(encoding='utf-8')
    lookup='(CTextureManaged *)g_Cache->Get(CacheClass::TextureManaged, '
    if text.count(lookup)!=5:
        raise ValueError('Original skin texture lookup sites changed')
    text=text.replace(lookup,'skin_texture::GetManaged(')
    text=adapt(text)
    text=text.replace('(LPDIRECT3DTEXTURE9)user_param','reinterpret_cast<void*>(user_param)')
    for stage in range(8):
        text=text.replace(f'g_D3DD->SetTransform(D3DTS_TEXTURE{stage},',
                          f'srhd_awa::matrixgame::render_pipeline::TextureTransform({stage},')
    marker='static void SkinPreload(const SSkin *s)'
    if text.count(marker)!=1:raise ValueError('Original skin preload entry changed')
    check='''#undef ASSERT_DX
#ifdef ASSERT_OFF
#define ASSERT_DX(call) (call)
#else
#define ASSERT_DX(call) rp3g::CheckDeviceResult((call), #call)
#endif
'''
    text=text.replace(marker,check+'\n'+marker)
    clear=function(text,'void CSkinManager::Clear(void)')
    old='''        if (m_Skins[gsp])
            HFree(m_Skins[gsp], g_MatrixHeap);'''
    if clear.count(old)!=1:raise ValueError('Original skin ownership cleanup changed')
    fixed=clear.replace(old,old+'''
        m_Skins[gsp] = NULL;
        m_SkinsCount[gsp] = 0;''')
    text=text.replace(clear,fixed)
    prelude='''#include "Texture.hpp"
#include "render_pipeline_backend.hpp"
#include "renderer_ops.hpp"
namespace rp3g=srhd_awa::matrixgame::platform3g;
namespace skin_texture=srhd_awa::matrixgame::legacy_texture;
'''
    path.write_bytes((prelude+text).encode())
    return {name:hashlib.sha256(path.read_bytes()).hexdigest()}
