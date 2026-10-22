"""Compile the pinned cache algorithms against explicit portable resource factories."""
from matrixgame_texture_request_overrides import function


def patch_cache(files):
    # Cache.cpp and Cache.hpp hashes are already checked by the core preparer.
    text=files['Cache.cpp'].decode()
    text=text.replace('#include "3g.hpp"','#include "cache_resources.hpp"')
    text=text.replace('#include "VectorObject.hpp"','#include "VectorObjectCpu.hpp"')
    for old,new in (
        ('HNew(g_CacheHeap) CTextureManaged','srhd_awa::matrixgame::CreateCacheTexture(CacheClass::TextureManaged)'),
        ('HNew(g_CacheHeap) CTexture','srhd_awa::matrixgame::CreateCacheTexture(CacheClass::Texture)'),
    ):
        if text.count(old)!=1:raise ValueError('Pinned cache factory changed: '+old)
        text=text.replace(old,new)
    text=text.replace('DTRACE();','')
    # std::filesystem::path implicitly converts to wstring only on Windows.
    # Use the already ported texture path boundary, keeping Windows separators
    # and query suffixes without a host locale conversion.
    signature='void CacheReplaceFileExt('
    old=function(text,signature)
    text=text.replace(old,'''void CacheReplaceFileExt(std::wstring& outname, std::wstring_view mname, std::wstring_view ext) {
    outname=srhd_awa::matrixgame::ReplaceTextureFileExt(mname,ext);
}''')
    old=function(text,'void CacheReplaceFileNameAndExt(')
    text=text.replace(old,'''void CacheReplaceFileNameAndExt(std::wstring& outname, std::wstring_view mname, std::wstring_view replacement) {
    const auto query=mname.find(L'?');
    const auto file=mname.substr(0,query);
    const auto slash=file.find_last_of(L"/\\\\");
    outname=(slash==file.npos?std::wstring{}:std::wstring(file.substr(0,slash+1)));
    outname+=replacement;
    if(query!=mname.npos)outname+=mname.substr(query);
}''')
    # Reject a live double entry before publishing another heap/cache. Allocation
    # failure must not leave the module appearing partly initialized.
    old='''    g_CacheHeap = HNew(NULL) CHeap();
    g_Cache = HNew(g_CacheHeap) CCache;'''
    new='''    if (g_Cache || g_CacheHeap) throw std::logic_error("CacheInit already active");
    CHeap* heap = HNew(NULL) CHeap();
    try { g_Cache = HNew(heap) CCache; }
    catch (...) { HDelete(CHeap, heap, NULL); throw; }
    g_CacheHeap = heap;'''
    if text.count(old)!=1:raise ValueError('Pinned CacheInit changed')
    text=text.replace(old,new)
    old='''    cd->m_Name = name;
    Add(cd);'''
    new='''    try { cd->m_Name = name; Add(cd); }
    catch (...) { Destroy(cd); throw; }'''
    if text.count(old)!=1:raise ValueError('Pinned Cache Get publication changed')
    text=text.replace(old,new)
    # This remains a separately named effective TU. No GPU Texture/D3D header
    # is provided or hidden; full-engine casts to CTexture remain unported.
    files['CacheNative.cpp']=text.encode()
