// Real original Text front end; SDK inspection is isolated in another TU.
#include "Text/Font.hpp"
#include "Text/Parser.hpp"
#include "Text/Render.hpp"
#include "text_backend.hpp"
#include <cassert>
#include <cstdio>
int ProbeOriginalTextFontCache(ID3DXFont*(*)(IDirect3DDevice9*),void(*)());
ID3DXFont* OriginalTextFontForDevice(IDirect3DDevice9* device){return Text::GetFont(device,L"Font.2Normal").operator->();}
#ifdef MATRIXGAME_TEXT_CACHE_FIXED
void ClearOriginalTextFonts(){Text::ClearFontCache();}
#endif
int VerifyOriginalTextDevice(const wchar_t* output) {
    namespace tb=srhd_awa::matrixgame::text_backend;
    auto& font=Text::GetFont(tb::DeviceIdentity(),L"Font.2Normal");
    assert(font && font.GetHeight()==14 && font.GetSpaceWidth()>0 && font.CalcTextWidth(L"MatrixGame")>0);
    auto tokens=Text::parse_tokens(L"A <CoLoR=255,0,0>B</color>\r\nC",font);
    bool red=false;for(const auto& token:tokens)if(token.text==L"B"){red=true;assert(token.color==0xffff0000u && token.width==font.CalcTextWidth(L"B"));}
    assert(red && Text::calc_lines(tokens,font,{0,0,200,80})==2);
    const auto cache=ProbeOriginalTextFontCache(OriginalTextFontForDevice,
#ifdef MATRIXGAME_TEXT_CACHE_FIXED
                                             ClearOriginalTextFonts
#else
                                             nullptr
#endif
                                             );
    CBitmap bitmap;
    Text::Render(L"MatrixGame 123",L"Font.2Normal",0xffffffffu,128,32,0,0,1,0,0,Base::CRect(0,0,128,32),bitmap);
    assert(bitmap.SizeX()==128 && bitmap.SizeY()==32);
    unsigned ink=0;for(int y=0;y<bitmap.SizeY();++y)for(int x=0;x<bitmap.SizeX();++x){const auto* p=bitmap.Data()+y*bitmap.Pitch()+x*4;if(p[0]||p[1]||p[2])++ink;}
    assert(bitmap.SaveInPNG(output));std::printf("original Text::Render bitmap=128x32 nonzero_rgb_pixels=%u device_cache_match=%d\n",ink,cache==0);
#ifdef MATRIXGAME_TEXT_CACHE_FIXED
    Text::ClearFontCache();Text::ClearFontCache();
#endif
    return cache?cache:(ink?0:2);
}
