"""Original bottom texture union with an explicit DXT1 overload."""
import hashlib
from matrixgame_vector_object_overrides import function


def prepare_bottom_texture(output):
    name='MatrixGame/src/MatrixMapTexture.cpp';path=output/name
    text=path.read_text(encoding='utf-8')
    call='m_Texture->LoadFromBitmap(bm, D3DFMT_DXT1, 6);'
    if text.count(call)!=2:raise ValueError('Original bottom texture request changed')
    text=text.replace(call,'m_Texture->LoadFromBitmap(bm, static_cast<TexturePixelFormat>(0x31545844u), 6);')
    create='m_Texture = CACHE_CREATE_TEXTUREMANAGED();'
    if text.count(create)!=1:raise ValueError('Original bottom texture factory changed')
    text=text.replace(create,'m_Texture = srhd_awa::matrixgame::legacy_texture::CreateManaged();')
    signature='void CBottomTextureUnion::MakeFromBitmap(const CBitmap &bm)'
    original=function(text,signature)
    replacement=signature+''' {
    auto* candidate=srhd_awa::matrixgame::legacy_texture::CreateManaged();
    struct Owner {CTextureManaged*& value;~Owner(){if(value)CCache::Destroy(value);}} owner{candidate};
    candidate->LoadFromBitmap(bm,static_cast<TexturePixelFormat>(0x31545844u),6);
    if(m_Texture)CCache::Destroy(m_Texture);
    m_Texture=candidate;candidate=nullptr;
}'''
    text=text.replace(original,replacement)
    path.write_bytes(text.encode())
    return {name:hashlib.sha256(path.read_bytes()).hexdigest()}
