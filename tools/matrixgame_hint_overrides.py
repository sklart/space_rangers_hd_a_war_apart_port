"""Full original hint module, typed texture ownership and device contracts."""
import hashlib
import re
from matrixgame_render_pipeline_overrides import adapt


def prepare_hint(output):
    name='MatrixGame/src/Interface/MatrixHint.cpp';path=output/name
    text=path.read_text(encoding='utf-8')
    if text.count('CACHE_CREATE_TEXTUREMANAGED()')!=1:raise ValueError('Hint texture factory changed')
    text=text.replace('CACHE_CREATE_TEXTUREMANAGED()', 'hint_texture::CreateManaged()')
    factory='    CTextureManaged *tex = hint_texture::CreateManaged();'
    text=text.replace(factory,factory+'''
    struct TextureOwner {
        CTextureManaged*& value;
        ~TextureOwner(){if(value)CCache::Destroy(value);}
    } texture_owner{tex};''',1)
    publish='    hint->m_CopyPos = copypos;\n\n    return hint;'
    if text.count(publish)!=1:raise ValueError('Hint ownership publication changed')
    text=text.replace(publish,'''    // Constructor completed: the actual hint now owns its texture.
    tex=nullptr;
    struct HintOwner {
        CMatrixHint*& value;
        ~HintOwner(){if(value)value->Release();}
    } hint_owner{hint};
    hint->m_CopyPos = copypos;
    auto* result=hint;hint=nullptr;
    return result;''',1)
    text=adapt(text)
    values={'D3DLOCKED_RECT':'hint_texture::LockedRect',
        'D3DFMT_A8R8G8B8':'static_cast<TexturePixelFormat>(21u)',
        'D3DRS_ALPHAREF':'static_cast<srhd_awa::matrixgame::RasterRegister>(24)',
        'D3DRS_ZENABLE':'static_cast<srhd_awa::matrixgame::RasterRegister>(7)',
        'D3DZB_FALSE':'0u','D3DTEXF_POINT':'1u','D3DTEXF_NONE':'0u',
        'D3DSAMP_MAGFILTER':'srhd_awa::matrixgame::SamplerRegister::MagFilter',
        'D3DSAMP_MINFILTER':'srhd_awa::matrixgame::SamplerRegister::MinFilter',
        'D3DSAMP_MIPFILTER':'srhd_awa::matrixgame::SamplerRegister::MipFilter'}
    for old,new in values.items():text=re.sub(r'\b'+old+r'\b',lambda m:new,text)
    marker='static std::list<CMatrixHint*> m_hints;'
    if text.count(marker)!=1:raise ValueError('Hint registry changed')
    text=text.replace(marker,'''#undef FAILED_DX
#define FAILED_DX(call) rp3g::CheckDeviceResult((call), #call)
#undef ASSERT_DX
#ifdef ASSERT_OFF
#define ASSERT_DX(call) (call)
#else
#define ASSERT_DX(call) FAILED_DX(call)
#endif
'''+marker)
    prelude='''#include "Texture.hpp"
#include "renderer_ops.hpp"
namespace rp3g=srhd_awa::matrixgame::platform3g;
namespace hint_texture=srhd_awa::matrixgame::legacy_texture;
'''
    path.write_bytes((prelude+text).encode())
    return {name:hashlib.sha256(path.read_bytes()).hexdigest()}
