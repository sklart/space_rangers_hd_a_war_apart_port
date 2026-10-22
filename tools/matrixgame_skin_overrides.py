"""Pinned skin resource selection and UV lifecycle; no fake D3D skin callbacks."""
from matrixgame_vector_object_overrides import function

INPUTS={'MatrixSkinManager.cpp':'d9d565af29522397011903bf6e3d730575092d0eb8ad540506740ff8f03f4778',
        'MatrixSkinManager.hpp':'734500a6c2aa6d75665ed18c52243222b1842bcc514941c7c2cad7bdf8297fef'}
SUBDIRS={name:'../../MatrixGame/src' for name in INPUTS}

def patch_skin_resources(files):
    c=files['MatrixSkinManager.cpp'].decode()
    original=function(c,'const SSkin *CSkinManager::GetSkin(')
    # Deliberate boundary before heap publication/Prepare and callback selection.
    # Native publication uses RAII, not memcpy/HAlloc of shared_ptr fields.
    prefix=original[:original.index('    for (int i = 0; i < m_SkinsCount[gsp];')]
    prefix=prefix.replace('const SSkin *CSkinManager::GetSkin(',
                          'std::shared_ptr<const NativeSkin> NativeSkinLibrary::Acquire(')
    prefix=prefix.replace('DWORD','std::uint32_t').replace('SMatrixSkin sk;','NativeSkin sk;')
    prefix=prefix.replace('    ParamParser t(textures), temp, temp_prev;',
        '    Check();\n    if(!textures)throw std::invalid_argument("null skin key");\n    ParamParser t(textures), temp, temp_prev;')
    prefix=prefix.replace('    ASSERT(gsp < GSP_COUNT);',
        '    if(gsp>=GSP_COUNT)throw std::invalid_argument("skin group");')
    call='(CTextureManaged *)g_Cache->Get(CacheClass::TextureManaged, '
    if prefix.count(call)!=5:raise ValueError('Pinned skin cache selection changed')
    prefix=prefix.replace(call,'GetTexture(')
    prefix=prefix.replace('g_Config.m_ObjTexturesGloss','gloss_enabled_')
    prefix=prefix.replace('NULL','nullptr')
    tail='''    if(!sk.m_Tex)throw std::invalid_argument("skin requires base texture");
    for(const auto& current:m_Skins[gsp])if(*current==sk)return current;
    sk.group=static_cast<GSParam>(gsp);
    if(!epoch_)epoch_=std::make_shared<int>(0);
    sk.lifetime=epoch_;
    auto skin=std::make_shared<NativeSkin>(std::move(sk));
    m_Skins[gsp].push_back(skin);
    return skin;
}
'''
    takt=function(c,'void CSkinManager::Takt(').replace('CSkinManager::','NativeSkinLibrary::')
    takt=takt.replace('    for (int gsp', '    Check();\n    for (int gsp',1)
    takt=takt.replace('m_SkinsCount[gsp]','int(m_Skins[gsp].size())')
    takt=takt.replace('SMatrixSkin *ms = m_Skins[gsp][i];','NativeSkin *ms = m_Skins[gsp][i].get();')
    files['SkinResourcesNative.cpp']=('''// Original MatrixSkinManager resource prefix/Takt; GPL-2.0-or-later.
#include "native_skin.hpp"
#include "CFile.hpp"
#include "TextureFlags.hpp"
namespace srhd_awa::matrixgame {
'''+prefix+tail+takt+'\n}\n').encode()
