"""Complete original text parsing/layout/font/render TUs with typed SDK calls."""
import hashlib
import re
from matrixgame_vector_object_overrides import function


def prepare_text(output):
    changed={}
    def write(name,text):
        path=output/name;path.write_bytes(text.encode());changed[name]=hashlib.sha256(path.read_bytes()).hexdigest()
    name='MatrixGame/src/Text/Font.hpp';text=(output/name).read_text(encoding='utf-8')
    text=text.replace('#include <d3dx9core.h>','#include "text_backend.hpp"').replace('LPD3DXFONT','ID3DXFont*');write(name,text)
    name='MatrixGame/src/Text/Parser.hpp';text=(output/name).read_text(encoding='utf-8').replace('RECT','srhd_awa::matrixgame::text_backend::Rect');write(name,text)
    for leaf in ['Font.cpp','Parser.cpp','Render.cpp']:
        name='MatrixGame/src/Text/'+leaf;text=(output/name).read_text(encoding='utf-8')
        text=text.replace('#include <d3dx9core.h>','').replace('#include <d3dx9tex.h>','').replace('#include <Resource.h>','')
        text=text.replace('LPD3DXFONT','ID3DXFont*').replace('D3DCOLOR','std::uint32_t')
        text=re.sub(r'\bRECT\b','text_native::Rect',text)
        for key,value in {'NOCLIP':0x100,'LEFT':0,'CENTER':1,'RIGHT':2,'TOP':0,'VCENTER':4,'BOTTOM':8,'WORDBREAK':0x10,'CALCRECT':0x400,'SINGLELINE':0x20}.items():
            text=re.sub(r'\bDT_'+key+r'\b',hex(value)+'u',text)
        if leaf=='Font.cpp':
            prepare=function(text,'void prepareRangersFont()');text=text.replace(prepare,'void prepareRangersFont(){text_native::PrepareRangersFont();}')
            old='''D3DXCreateFontW(device, size, 0, FW_NORMAL, 1, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                DEFAULT_PITCH, name.data(), &font)'''
            if text.count(old)!=1:raise ValueError('Original text font request changed')
            text=text.replace('S_OK != '+old,'0 != text_native::CreateTextFont(device, name, size, &font)')
            text=text.replace('lgr.error("Failed to load font: {}")(utils::from_wstring(name));','text_native::LogError("Failed to load font: "+utils::from_wstring(name));')
            text=text.replace('m_font->Release();','text_native::Release(m_font);')
            text=text.replace('D3DXFONT_DESCW desc;','text_native::FontDesc desc;').replace('m_font->GetDescW(&desc);','text_native::DescribeFont(m_font, &desc);')
            text=text.replace('SIZE size{};','text_native::Size size{};').replace('GetTextExtentPoint32W(m_font->GetDC(), L" ", 1, &size);','text_native::MeasureSpace(m_font, L" ", 1, &size);')
            text=text.replace('m_font->DrawTextW(NULL, ','text_native::DrawFontText(m_font, ')
        if leaf=='Parser.cpp':
            text=text.replace('lgr.error("Failed to parse color from text: {}")(utils::from_wstring(std::wstring{text}));','text_native::LogError("Failed to parse color from text: "+utils::from_wstring(std::wstring{text}));')
        if leaf=='Render.cpp':
            text=text.replace('m_obj->Release();','text_native::Release(m_obj);')
            text=text.replace('font->DrawTextW(NULL, ','text_native::DrawFontText(font.operator->(), ')
            text=text.replace('Text::GetFont(g_D3DD, font_name)','Text::GetFont(text_native::DeviceIdentity(), font_name)')
            text=text.replace('g_D3DD->CreateTexture(sizex, sizey, 0, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, texture, NULL)','text_native::CreateTexture(sizex, sizey, texture)')
            text=text.replace('texture->GetSurfaceLevel(0, surface)','text_native::GetSurface(texture, 0, surface)')
            text=text.replace('D3DLOCKED_RECT rect{};','srhd_awa::matrixgame::legacy_texture::LockedRect rect{};')
            text=text.replace('texture->LockRect(0, &rect, NULL, 0)','text_native::Lock(texture, 0, &rect)')
            text=text.replace('D3DXCreateRenderToSurface(g_D3DD, sizex, sizey, D3DFMT_A8R8G8B8, 0, D3DFMT_UNKNOWN, pSurfaceRender)','text_native::CreateRenderSurface(sizex, sizey, pSurfaceRender)')
            text=text.replace('pSurfaceRender->BeginScene(surface, NULL)','text_native::BeginScene(pSurfaceRender, surface)')
            text=text.replace('g_D3DD->Clear(0, 0, D3DCLEAR_TARGET, 0, 0, 0)','text_native::Clear()')
            text=text.replace('texture->UnlockRect(0)','text_native::Unlock(texture, 0)').replace('pSurfaceRender->EndScene(0)','text_native::EndScene(pSurfaceRender, 0)')
            text=text.replace('FAILED(res)','(res < 0)')
        prelude='#include "text_backend.hpp"\nnamespace text_native=srhd_awa::matrixgame::text_backend;\n'
        write(name,prelude+text)
    name='MatrixGame/src/Text/Font.hpp';text=(output/name).read_text(encoding='utf-8')
    text=text.replace('Font& GetFont(', 'void ClearFontCache() noexcept;\n\nFont& GetFont(',1);write(name,text)
    name='MatrixGame/src/Text/Font.cpp';text=(output/name).read_text(encoding='utf-8')
    original=function(text,'Font& GetFont(IDirect3DDevice9* device, std::wstring_view font_name)')
    begin=original.index('        prepareRangersFont();');end=original.index('        return fonts;',begin)+len('        return fonts;')
    build='\n'.join(line[4:] for line in original[begin:end].splitlines())
    replacement='''namespace {
struct FontCache {IDirect3DDevice9* device{};std::map<std::wstring_view, Font> fonts;};
FontCache& Cache(){static FontCache cache;return cache;}
std::map<std::wstring_view, Font> BuildFonts(IDirect3DDevice9* device) {
'''+build+'''
}
}
void ClearFontCache() noexcept {
    auto& cache=Cache();cache.fonts.clear();cache.device=nullptr;
    text_native::ReleaseRangersFont();
}
Font& GetFont(IDirect3DDevice9* device, std::wstring_view font_name) {
    auto& cache=Cache();
    if(cache.device!=device) {
        ClearFontCache();
        try {auto fonts=BuildFonts(device);cache.fonts.swap(fonts);cache.device=device;}
        catch(...) {text_native::ReleaseRangersFont();throw;}
    }
    if(!cache.fonts.contains(font_name))throw std::runtime_error("Unknown font: "+utils::from_wstring(font_name));
    return cache.fonts.at(font_name);
}'''
    text=text.replace(original,replacement,1);write(name,text)
    name='MatrixGame/src/Text/Render.cpp';text=(output/name).read_text(encoding='utf-8')
    lock='    text_native::Lock(texture, 0, &rect);'
    if text.count(lock)!=1:raise ValueError('Original text lock changed')
    text=text.replace(lock,'',1)
    old='''    text_native::Unlock(texture, 0);
    text_native::EndScene(pSurfaceRender, 0);'''
    new='''    srhd_awa::matrixgame::platform3g::CheckDeviceResult(text_native::EndScene(pSurfaceRender, 0), "text surface EndScene");
    srhd_awa::matrixgame::platform3g::CheckDeviceResult(text_native::Lock(texture, 0, &rect), "text bitmap LockRect");
    struct UnlockOwner {IDirect3DTexture9* texture;~UnlockOwner(){text_native::Unlock(texture, 0);}} unlock_owner{texture};'''
    if text.count(old)!=1:raise ValueError('Original text unlock/end changed')
    text=text.replace(old,new,1);write(name,text)
    return changed
