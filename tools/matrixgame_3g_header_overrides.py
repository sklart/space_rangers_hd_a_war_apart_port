"""Keep complete 3G/texture declarations without importing a concrete SDK.

SDK-dependent original bodies move to their existing implementation TU. No
texture methods or device implementation are supplied by this declaration split.
"""
import hashlib
import re
from matrixgame_vector_object_overrides import function

def patch_3g_headers(output):
    changed={}
    def read(name):return (output/name).read_text(encoding='utf-8')
    def write(name,text):
        path=output/name;path.write_bytes(text.encode())
        changed[name]=hashlib.sha256(path.read_bytes()).hexdigest()
    base='MatrixLib/3G/'
    h=read(base+'3g.hpp');c=read(base+'3g.cpp')
    for include in ('#include "d3d9.h"','#include "d3dx9tex.h"'):h=h.replace(include,'')
    h=h.replace('#include "Form.hpp"','#include "Form.hpp"\n#include "../Base/Tracer.hpp"')
    h=h.replace('#include "BaseDef.hpp"','''#include "BaseDef.hpp"
#include <cstdint>
// Genuine SDK interfaces/value tags remain opaque. No methods or fake SDK types.
struct IDirect3D9;
struct IDirect3DDevice9;
struct _D3DCAPS9;
struct _D3DPRESENT_PARAMETERS;''')
    h=h.replace('extern HINSTANCE g_HInst;','extern void* g_HInst;')
    h=h.replace('extern HWND g_Wnd;','extern void* g_Wnd;')
    h=h.replace('extern D3DCAPS9 g_D3DDCaps;','extern _D3DCAPS9 g_D3DDCaps;')
    h=h.replace('extern D3DPRESENT_PARAMETERS g_D3Dpp;','extern _D3DPRESENT_PARAMETERS g_D3Dpp;')
    start=h.index('#ifdef DO_SMART_COLOROPS')
    end=h.index('\nclass CExceptionD3D',start)
    ops=h[start:end]
    # Both smart and ordinary implementations survive in the original TU.
    struct_end=ops.index('extern STextureStageOp g_AlphaOp[];')+len('extern STextureStageOp g_AlphaOp[];')
    impl=ops[:ops.index('struct STextureStageOp')]+ops[struct_end:]
    impl=impl.replace('inline void ','void ').replace('D3DTEXTUREOP op','std::uint32_t op')
    declarations='''#ifdef DO_SMART_COLOROPS
struct STextureStageOp {std::uint32_t op,p1,p2;};
extern STextureStageOp g_ColorOp[];
extern STextureStageOp g_AlphaOp[];
#endif
void SetColorOp(int stage,std::uint32_t op,std::uint32_t p1,std::uint32_t p2);
void SetAlphaOp(int stage,std::uint32_t op,std::uint32_t p1,std::uint32_t p2);
void SetColorOpAnyOrder(int stage,std::uint32_t op,std::uint32_t p1,std::uint32_t p2);
#ifndef DO_SMART_COLOROPS
void SetAlphaOpAnyOrder(int stage,std::uint32_t op,std::uint32_t p1,std::uint32_t p2);
#endif
void SetColorOpSelect(int stage,std::uint32_t p);
void SetAlphaOpSelect(int stage,std::uint32_t p);
void SetColorOpDisable(int stage);
void SetAlphaOpDisable(int stage);
'''
    h=h[:start]+declarations+h[end:]
    for old,new in [('DWORD','std::uint32_t'),('HRESULT','std::int32_t'),('HINSTANCE','void*'),('HWND','void*')]:
        h=re.sub(r'\b'+old+r'\b',new,h)
    h=h.replace('hr != D3D_OK','hr != 0')
    # SDK DWORD may be unsigned long on Windows, unlike uint32_t. Definitions
    # use the same fixed-width public API instead of changing their algorithms.
    impl=re.sub(r'\bDWORD\b','std::uint32_t',impl)
    c='#include "d3d9.h"\n#include "d3dx9tex.h"\n'+c+'\n// Moved intact from the original inline 3g.hpp implementations.\n'+impl
    write(base+'3g.hpp',h);write(base+'3g.cpp',c)

    h=read(base+'Texture.hpp');c=read(base+'Texture.cpp')
    moved=[]
    signatures=[
        ('virtual ~CTextureManaged()', 'CTextureManaged::~CTextureManaged()'),
        ('void Preload(void)', 'void CTextureManaged::Preload(void)'),
        ('HRESULT CreateLock(D3DFORMAT fmt, int sx, int sy, int levels, D3DLOCKED_RECT &lr)',
         'std::int32_t CTextureManaged::CreateLock(TexturePixelFormat fmt, int sx, int sy, int levels, _D3DLOCKED_RECT &lr)'),
        ('void LockRect(D3DLOCKED_RECT &lr, DWORD Flags)',
         'void CTextureManaged::LockRect(_D3DLOCKED_RECT &lr, std::uint32_t Flags)'),
        ('void UnlockRect(void)','void CTextureManaged::UnlockRect(void)'),
    ]
    # Preload exists in both texture classes; only the managed body calls SDK.
    managed=h.index('class CTextureManaged :')
    prefix,h_managed=h[:managed],h[managed:]
    for signature,definition in signatures:
        body=function(h_managed,signature)
        moved.append(body.replace(signature,definition,1))
        h_managed=h_managed.replace(body,signature+';',1)
    h=prefix+h_managed
    for include in ('#include "d3d9.h"','#include "d3dx9tex.h"'):h=h.replace(include,'')
    h=h.replace('#include "Cache.hpp"','''#include "Cache.hpp"
#include <cassert>
#include <type_traits>
// Real resource interface and locked-rectangle tag, deliberately incomplete.
struct IDirect3DTexture9;
struct _D3DLOCKED_RECT;
// Native value identifiers. They implement no SDK/backend operations.
enum class TextureMemoryPool:std::uint32_t {Device=0,Managed=1,SystemMemory=2,Scratch=3};
enum class TexturePixelFormat:std::uint32_t {};''')
    h=h.replace('D3DPOOL pool = D3DPOOL_DEFAULT','TextureMemoryPool pool = TextureMemoryPool::Device')
    h=h.replace('DTRACE();','').replace('ASSERT(','assert(')
    names={'LPDIRECT3DTEXTURE9':'IDirect3DTexture9*','DWORD':'std::uint32_t',
        'HRESULT':'std::int32_t','D3DFORMAT':'TexturePixelFormat','D3DLOCKED_RECT':'_D3DLOCKED_RECT'}
    for old,new in names.items():h=re.sub(r'\b'+old+r'\b',lambda m:new,h)
    # An old SDK enum must not silently bind to the bool conversion overload
    # after introducing the native format identifier. Require an explicit port.
    h=h.replace('    // void LoadFromBitmapAsIs', '''    template<class LegacyFormat> requires std::is_enum_v<LegacyFormat>
    void LoadFromBitmap(const CBitmap&, LegacyFormat, int = 0) = delete;
    // void LoadFromBitmapAsIs''')
    # The original SDK implementation keeps its algorithms and receives an
    # explicit conversion at the native parameter boundary only.
    signature='LPDIRECT3DTEXTURE9 CBaseTexture::LoadTextureFromFile(bool to16, D3DPOOL pool)'
    body=function(c,signature)
    adapted=body.replace(signature,'LPDIRECT3DTEXTURE9 CBaseTexture::LoadTextureFromFile(bool to16, TextureMemoryPool native_pool)',1)
    adapted=adapted.replace('{','{\n    const auto pool=static_cast<D3DPOOL>(native_pool);',1)
    c=c.replace(body,adapted)
    signature='void CTextureManaged::LoadFromBitmap(const CBitmap &bm, D3DFORMAT fmt, int levels)'
    body=function(c,signature)
    adapted=body.replace(signature,'void CTextureManaged::LoadFromBitmap(const CBitmap &bm, TexturePixelFormat native_format, int levels)',1)
    adapted=adapted.replace('{','{\n    const auto fmt=static_cast<D3DFORMAT>(native_format);',1)
    c=c.replace(body,adapted)
    c=c.replace('LoadTextureFromFile(to16, D3DPOOL_MANAGED)','LoadTextureFromFile(to16, TextureMemoryPool::Managed)')
    c=c.replace('LoadTextureFromFile(to16, D3DPOOL_SYSTEMMEM)','LoadTextureFromFile(to16, TextureMemoryPool::SystemMemory)')
    # Non-managed configuration retains its format value and conversion too.
    c=c.replace('m_Usage, m_Format,','m_Usage, static_cast<D3DFORMAT>(m_Format),')
    c=c.replace('m_Levelcnt, 0, m_Format,','m_Levelcnt, 0, static_cast<D3DFORMAT>(m_Format),')
    c=c.replace('m_Format = desc.Format','m_Format = static_cast<TexturePixelFormat>(desc.Format)')
    for body in moved:
        if 'CTextureManaged::CreateLock' in body:
            body=body.replace('TexturePixelFormat fmt','TexturePixelFormat native_format')
            body=body.replace('{','{\n    const auto fmt=static_cast<D3DFORMAT>(native_format);',1)
            body=body.replace('m_Format = desc.Format','m_Format = static_cast<TexturePixelFormat>(desc.Format)')
        c+='\n// Moved from original Texture.hpp, including both pool branches.\n'+body+'\n'
    c='#include "d3d9.h"\n#include "d3dx9tex.h"\n'+c
    write(base+'Texture.hpp',h);write(base+'Texture.cpp',c)
    # Debug text declarations need a genuine opaque font interface only.
    name='MatrixGame/src/MatrixDebugInfo.hpp'
    h=read(name).replace('#include <d3dx9core.h>','struct ID3DXFont;')
    write(name,h)
    name='MatrixGame/src/MatrixDebugInfo.cpp'
    c=read(name)
    write(name,'#include <d3dx9core.h>\n'+c)
    # Preserve the original release tracing definitions; its Windows/x86 clock
    # helpers are only needed by the original debug tracing configuration.
    name='MatrixLib/Base/Tracer.hpp';h=read(name)
    h=h.replace('#include <windows.h>','#if defined(_DEBUG) || defined(_TRACE)\n#include <windows.h>\n#endif')
    h=h.replace('#ifdef __GNUC__','#if defined(__GNUC__) && (defined(_DEBUG) || defined(_TRACE))')
    body=function(h,'inline unsigned __int64 GetCPUTakt(void)')
    h=h.replace(body,'#if defined(_DEBUG) || defined(_TRACE)\n'+body+'\n#endif')
    write(name,h)
    name='MatrixGame/src/Effects/MatrixEffect.hpp';h=read(name)
    h=h.replace('#include "VectorObject.hpp"','#include "VectorObject.hpp"\n#include "../../../MatrixLib/Base/Tracer.hpp"')
    write(name,h)
    name='MatrixGame/src/MatrixMapStatic.hpp';h=read(name)
    h=h.replace('#include "VectorObject.hpp"','#include "VectorObject.hpp"\n#include "Texture.hpp"')
    write(name,h)
    return changed
