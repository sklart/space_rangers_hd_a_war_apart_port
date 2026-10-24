"""Original managed texture methods, actual registry and explicit SDK boundary.

This is a selected-method integration, not a full Texture.cpp port. The
production USE_DX_MANAGED_TEXTURES branch is retained; no CPU cache cast.
"""
import hashlib,re
from matrixgame_vector_object_overrides import function

BITMAP='void CTextureManaged::LoadFromBitmap(const CBitmap &bm, bool convert_to_16bit, int levelcnt)'
FORMATTED='void CTextureManaged::LoadFromBitmap(const CBitmap &bm, TexturePixelFormat native_format, int levels)'
def extract(source,signature):
    if signature!=BITMAP:return function(source,signature)
    # Alternative preprocessor branches have intentionally unmatched raw braces.
    # Preserve both branches using the pinned next top-level method boundary.
    if source.count(signature)!=1:raise ValueError('Original bitmap overload changed')
    start=source.index(signature);end=source.index('\nvoid CTextureManaged::Ramka(',start)
    return source[start:end].rstrip()

def prepare_legacy_texture(output):
    changes={};name='MatrixLib/3G/Texture.cpp';path=output/name
    source=path.read_text(encoding='utf-8')
    signatures=['CBaseTexture::CBaseTexture(void)','CBaseTexture::~CBaseTexture()',
        'void CBaseTexture::ParseFlags(const ParamParser& name)',
        'IDirect3DTexture9* CBaseTexture::LoadTextureFromFile(bool to16, TextureMemoryPool native_pool)',
        'void CTextureManaged::Init(bool to16)','void CTextureManaged::Load(void)',
        'void CTextureManaged::Unload(void)','CTextureManaged::~CTextureManaged()',
        'void CTextureManaged::Preload(void)',
        'std::int32_t CTextureManaged::CreateLock(TexturePixelFormat native_format, int sx, int sy, int levels, _D3DLOCKED_RECT &lr)',
        'void CTextureManaged::LockRect(_D3DLOCKED_RECT &lr, std::uint32_t Flags)',
        'void CTextureManaged::UnlockRect(void)',
        'std::uint32_t CTextureManaged::GetPixelColor(int x, int y)',
        'void CTextureManaged::LoadFromBitmap(int level, const CBitmap &bm, bool convert_to_16bit)',
        'void CTextureManaged::LoadFromBitmap(const CBitmap &bm, bool convert_to_16bit, int levelcnt)',FORMATTED]
    chunks=[]
    for signature in signatures:
        before=extract(source,signature);body=before
        if signature=='std::uint32_t CTextureManaged::GetPixelColor(int x, int y)':
            body=body.replace('RECT r;', 'lt::TextureRect r;')
            for texture in ('m_Tex','m_TexFrom'):
                body=body.replace(texture+'->LockRect(0, &lr, &r, D3DLOCK_READONLY)',
                                  'lt::LockRegion('+texture+', 0, &lr, r, 16u)')
        if signature==FORMATTED:
            if before.count('bm.SaveInDDSUncompressed(buf);')!=1 or before.count('D3DXCreateTextureFromFileInMemoryEx(')!=2:
                raise ValueError('Original formatted bitmap path changed')
            # Keep original serialization/format/levels/pool/filter semantics.
            # Publish replacement only after SDK creation and description succeed.
            body=FORMATTED+''' {
    CBuf buf;
    bm.SaveInDDSUncompressed(buf);
    IDirect3DTexture9* candidate=nullptr;
    struct Owner {IDirect3DTexture9*& value;~Owner(){lt::Release(value);}} owner{candidate};
    tp::CheckDeviceResult(lt::DecodeFormatted(buf.Get(),buf.Len(),levels,
        static_cast<std::uint32_t>(native_format),1u,&candidate), "original formatted bitmap decode");
    if(!candidate)throw std::runtime_error("original formatted bitmap returned no texture");
    lt::LevelDesc desc{};
    tp::CheckDeviceResult(lt::Describe(candidate,0,&desc), "formatted bitmap GetLevelDesc");
    lt::Release(m_Tex);m_Tex=candidate;candidate=nullptr;
    m_Size.x=desc.Width;m_Size.y=desc.Height;
}'''
        body=body.replace('const auto pool=static_cast<D3DPOOL>(native_pool);','const auto pool=static_cast<std::uint32_t>(native_pool);')
        body=body.replace('D3DPOOL_MANAGED','1u').replace('D3DPOOL_SYSTEMMEM','2u')
        body=body.replace('LoadTextureFromFile(to16, 1u)','LoadTextureFromFile(to16, TextureMemoryPool::Managed)')
        body=body.replace('LoadTextureFromFile(to16, 2u)','LoadTextureFromFile(to16, TextureMemoryPool::SystemMemory)')
        body=body.replace('D3DSURFACE_DESC','lt::LevelDesc').replace('_D3DLOCKED_RECT','lt::LockedRect').replace('D3DLOCKED_RECT','lt::LockedRect')
        body=body.replace('static_cast<D3DFORMAT>(native_format)','static_cast<std::uint32_t>(native_format)')
        body=re.sub(r'\bHRESULT\b','std::int32_t',body)
        for key,value in {'D3DFMT_A4R4G4B4':26,'D3DFMT_A8R8G8B8':21,'D3DFMT_X1R5G5B5':24,'D3DFMT_L8':50,'D3DFMT_A8L8':51,'D3DFMT_X8R8G8B8':22,'D3D_OK':0}.items():
            body=re.sub(r'\b'+key+r'\b',str(value)+'u',body)
        body=body.replace('g_D3DD->CreateTexture(', 'lt::Create(')
        body=re.sub(r'\b(m_Tex|m_TexFrom)->Release\(\)',r'lt::Release(\1)',body)
        body=re.sub(r'\b(m_Tex|m_TexFrom)->GetLevelDesc\(',r'lt::Describe(\1, ',body)
        body=re.sub(r'\b(m_Tex|m_TexFrom)->GetLevelCount\(\)',r'lt::LevelCount(\1)',body)
        body=re.sub(r'\b(m_Tex|m_TexFrom)->LockRect\(',r'lt::Lock(\1, ',body)
        body=re.sub(r'\b(m_Tex|m_TexFrom)->UnlockRect\(',r'lt::Unlock(\1, ',body)
        body=body.replace('m_Tex->PreLoad()', 'lt::Preload(m_Tex)')
        body=body.replace('TexFrom()->LockRect(', 'lt::Lock(TexFrom(), ').replace('Tex()->LockRect(', 'lt::Lock(Tex(), ')
        body=body.replace('TexFrom()->UnlockRect(', 'lt::Unlock(TexFrom(), ').replace('Tex()->UnlockRect(', 'lt::Unlock(Tex(), ')
        if 'LoadTextureFromFile' in signature:
            old='''D3DXCreateTextureFromFileInMemoryEx(g_D3DD, buf.Get(), buf.Len(), 0, 0,
                                                   FLAG(m_Flags, TF_NOMIPMAP) ? 1 : 0, 0, D3DFMT_UNKNOWN, pool,
                                                   D3DX_FILTER_NONE, D3DX_FILTER_BOX, 0, NULL, NULL, &ret)'''
            if old not in body:raise ValueError('Original encoded loader changed')
            body=body.replace('FAILED('+old+')','(lt::Decode(buf.Get(), buf.Len(), FLAG(m_Flags, TF_NOMIPMAP) ? 1 : 0, pool, &ret) < 0)')
            body=body.replace('CTextureManaged *tex = CACHE_CREATE_TEXTUREMANAGED();',
                '''CTextureManaged *tex = lt::CreateManaged();
        struct TemporaryOwner {CTextureManaged* texture;~TemporaryOwner(){CCache::Destroy(texture);}} temporary{tex};''')
            body=body.replace('CCache::Destroy(tex);','/* TemporaryOwner destroys the actual original managed object. */',1)
        if signature=='void CTextureManaged::Init(bool to16)':
            needle='''    lt::LevelDesc desc;
    lt::Describe(m_Tex, 0, &desc);'''
            replacement='''    if(!m_Tex)throw tp::DeviceError("original managed texture decode", -1);
    lt::LevelDesc desc;
    try {tp::CheckDeviceResult(lt::Describe(m_Tex, 0, &desc), "GetLevelDesc");}
    catch(...) {lt::Release(m_Tex);m_Tex=nullptr;throw;}'''
            if needle not in body:raise ValueError('Original managed Init description changed')
            body=body.replace(needle,replacement)
        chunks.append(body)
    prelude='''// Selected original Texture.cpp/Texture.hpp managed methods; not a full TU.
#include "Texture.hpp"
#include "CFile.hpp"
#include "CStorage.hpp"
#include "legacy_texture_backend.hpp"
#include <list>
namespace lt=srhd_awa::matrixgame::legacy_texture;
namespace tp=srhd_awa::matrixgame::platform3g;
#ifndef USE_DX_MANAGED_TEXTURES
#error Native original textures currently preserve the production managed branch only.
#endif
#undef ASSERT_DX
#ifdef ASSERT_OFF
#define ASSERT_DX(call) (call)
#else
#define ASSERT_DX(call) tp::CheckDeviceResult((call), #call)
#endif
static std::list<CBaseTexture*> _textures;
namespace srhd_awa::matrixgame::legacy_texture {
std::size_t RegistrySize()noexcept{return _textures.size();}
CTextureManaged* CreateManaged(){return HNew(g_CacheHeap) CTextureManaged;}
CTextureManaged* GetManaged(std::wstring_view name) {
    if(!g_Cache)throw std::logic_error("original named texture requires initialized cache");
    // Original Get: Find, create, assign the exact name, Add. Keep lazy load.
    if(auto* existing=g_Cache->Find(CacheClass::TextureManaged,name)) {
        if(auto* managed=dynamic_cast<CTextureManaged*>(existing))return managed;
        throw tp::Unsupported("original named texture conflicts with portable cache entry");
    }
    auto* candidate=CreateManaged();
    struct Owner {CTextureManaged*& value;~Owner(){if(value)CCache::Destroy(value);}} owner{candidate};
    candidate->m_Name=name;g_Cache->Add(candidate);
    auto* result=candidate;candidate=nullptr;return result;
}
}
'''
    generated='MatrixLib/3G/OriginalManagedTexture.cpp'
    data=(prelude+'\n\n'.join(chunks)+'\n').encode();(output/generated).write_bytes(data)
    changes[generated]=hashlib.sha256(data).hexdigest()
    # Same exact adapted definitions stay in full Texture.cpp, rather than a
    # second ABI/body when the remaining full TU is eventually ported.
    for signature,body in zip(signatures,chunks):source=source.replace(extract(source,signature),body,1)
    source='#include "legacy_texture_backend.hpp"\nnamespace lt=srhd_awa::matrixgame::legacy_texture;\nnamespace tp=srhd_awa::matrixgame::platform3g;\n'+source
    path.write_bytes(source.encode());changes[name]=hashlib.sha256(path.read_bytes()).hexdigest()
    hname='MatrixLib/3G/Texture.hpp';header=(output/hname).read_text(encoding='utf-8')
    header=header.replace('struct _D3DLOCKED_RECT;','#include "legacy_texture_backend.hpp"')
    header=header.replace('_D3DLOCKED_RECT &lr','srhd_awa::matrixgame::legacy_texture::LockedRect &lr')
    (output/hname).write_bytes(header.encode());changes[hname]=hashlib.sha256(header.encode()).hexdigest()
    return changes
