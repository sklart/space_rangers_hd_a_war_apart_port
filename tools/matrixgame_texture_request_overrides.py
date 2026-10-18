"""Extract pinned original texture flags without compiling a fake D3D class."""
import re
INPUTS={
 'Texture.hpp':'2eaf5916cc9a2b406e886617a156a33ab39682ed6196011bb0ec873295f575a1',
 'Texture.cpp':'11535b4750ea94de8b97831636b044a781e785be8ecb9bffbe16358df72b0901',
 'Cache.cpp':'568f108f4578e8fb53a5b08bf2561189d4d3426402275deabeccd17a1e82fa9a',
}
SUBDIRS={name:'../3G' for name in INPUTS}


def function(text,signature):
    if text.count(signature)!=1:raise ValueError('Unexpected texture function '+signature)
    start=text.index(signature);opening=text.index('{',start);at=opening+1;depth=1
    while depth:
        depth+=(text[at]=='{')-(text[at]=='}');at+=1
    return text[start:at]


def patch_texture_requests(files):
    source=files['Texture.cpp'].decode();header=files['Texture.hpp'].decode()
    flags=[]
    for name in ('TF_NOMIPMAP','TF_ALPHATEST','TF_ALPHABLEND','TF_LOST','TF_COMPRESSED'):
        bit=re.search(r'^#define '+name+r'\s+SETBIT\((\d+)\)',header,re.M)
        if not bit:raise ValueError('Unexpected texture flag '+name)
        flags.append((name,int(bit[1])))
    files['TextureFlags.hpp']=('#pragma once\n#include <cstdint>\nnamespace srhd_awa::matrixgame {\n'+
        ''.join('inline constexpr std::uint32_t '+name+'=1u<<'+str(bit)+';\n' for name,bit in flags)+'}\n').encode()
    body=function(source,'void CBaseTexture::ParseFlags(const ParamParser& name)')
    body=body.replace('void CBaseTexture::ParseFlags','std::uint32_t ParseOriginalTextureFlags')
    body=body.replace('    m_Flags = 0;','    std::uint32_t m_Flags = 0;',1)
    body=body.replace('    DTRACE();','')
    body=body[:-1]+'    return m_Flags;\n}'
    # Only this CPU function is extracted; the original GPU methods stay unported.
    files['TextureFlags.cpp']=('''// MatrixGame - SR2 Planetary battles engine
// Copyright (C) 2012, Elemental Games, Katauri Interactive, CHK-Games
// GPL-2.0-or-later; original CBaseTexture::ParseFlags body.
#include "TextureFlags.hpp"
#include "texture_request.hpp"
#include "CBlockPar.hpp"
#include "CFile.hpp"
#include "utils.hpp"
namespace srhd_awa::matrixgame {
using namespace Base;
static void CacheReplaceFileExt(std::wstring& out,std::wstring_view name,std::wstring_view ext) {
    out=ReplaceTextureFileExt(name,ext);
}
'''+body+'\n}\n').encode()
