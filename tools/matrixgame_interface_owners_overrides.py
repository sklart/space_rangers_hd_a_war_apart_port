"""Complete original interface owners and their concrete element classes."""
import hashlib
import re


def prepare_interface_owners(output):
    changes = {}
    names = ('CInterface.cpp', 'CIFaceMenu.cpp', 'CIFaceStatic.cpp',
             'CIFaceImage.cpp', 'CAnimation.cpp', 'CCounter.cpp', 'CHistory.cpp')
    for leaf in names:
        name = 'MatrixGame/src/Interface/' + leaf
        path = output / name
        text = path.read_text(encoding='utf-8')
        text = re.sub(r'\bTRUE\b', 'true', text)
        text = re.sub(r'\bFALSE\b', 'false', text)
        text = text.replace('D3DLOCKED_RECT', 'srhd_awa::matrixgame::legacy_texture::LockedRect')
        text = text.replace('D3DFMT_A8R8G8B8', 'static_cast<TexturePixelFormat>(21)')
        text = re.sub(r'\bD3D_OK\b', '0', text)
        text = text.replace('(CTextureManaged *)g_Cache->Get(CacheClass::TextureManaged, ',
                            'srhd_awa::matrixgame::legacy_texture::GetManaged(')
        prelude = '#include "Texture.hpp"\n#include "legacy_texture_backend.hpp"\n'
        path.write_bytes((prelude + text).encode())
        changes[name] = hashlib.sha256(path.read_bytes()).hexdigest()
    return changes
