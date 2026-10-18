"""Port original CBitmap CPU methods and FilePNG handle/lifetime boundary."""
import re
INPUTS={
 'CBitmap.hpp':'7638f74743d26c427ec5f0f17626aaca6f1dd6879877370db8c3d4c7fe71d6c5',
 'CBitmap.cpp':'ee8e9f8da05306c7b33a3bc706b4953daf0cdf4181121f7819a3a003912a3437',
 'FilePNG.hpp':'390d96acc19fbc1522751bbf3b4e909ecbb96a81fdb829ad031b25811b330cd3',
 'FilePNG.cpp':'8af367f028492df51f3089aafa5eab5ea732cfea0b45f5de9318dfb5cb543397',
}
SUBDIRS={name:'../Bitmap' for name in INPUTS}


def patch_bitmap(files,root,body):
    native=root/'port/matrixgame/native-core'
    h=files['CBitmap.hpp'].decode().replace('#include <windows.h>', '#include <utility>')
    h=h[:h.index('class WinBitmap :')]+'''// GDI capture has no native implementation yet.
class WinBitmap;
'''
    h=h.replace('    ~CBitmap();','''    ~CBitmap();
    CBitmap(const CBitmap&)=delete;
    CBitmap& operator=(const CBitmap&)=delete;
    CBitmap(CBitmap&&) noexcept;
    CBitmap& operator=(CBitmap&&) noexcept;
    void Swap(CBitmap&) noexcept;
    bool LoadRGBAFromPNG(void*,int);
    bool LoadRGBAFromPNG(std::wstring_view);
''')
    h=h.replace('    void Clear();','''    bool LoadNativePNG(void*,int,bool);
    bool LoadNativePNGFile(std::wstring_view,bool);
    void Clear();''')
    files['CBitmap.hpp']=h.encode()
    png=files['FilePNG.hpp'].decode()
    png=png.replace('uint32_t Read(uintptr_t id,', '''void ReadCancel(uintptr_t id) noexcept;
uintptr_t ReadStart_RGBA(void*,uint32_t,uint32_t*,uint32_t*,uint32_t*,uint32_t*);
uint32_t Read(uintptr_t id,''')
    files['FilePNG.hpp']=png.encode()
    files['FilePNG.cpp']=(native/'png_native.cpp').read_bytes()
    c=files['CBitmap.cpp'].decode();c=c[:c.index('void WinBitmap::Clear()')]
    c=c.replace('#include <ddraw.h>','''#include "bitmap_limits.hpp"
#include <array>
#include <vector>
#include <utility>
using namespace matrixgame::native;''')
    # Shapes are checked before old image ownership changes.
    for signature,bpp in [('void CBitmap::CreateRGB(int lenx, int leny)',3),
                          ('void CBitmap::CreateGrayscale(int lenx, int leny)',1),
                          ('void CBitmap::Create16(int lenx, int leny)',2)]:
        c=c.replace(signature+'\n{',signature+'\n{\n    BitmapPitch(lenx,leny,'+str(bpp)+');',1)
        c=c.replace(signature+' {',signature+' {\n    BitmapPitch(lenx,leny,'+str(bpp)+');',1)
    c=c.replace('void CBitmap::CreateRGBA(int lenx, int leny, int pitch, void* data)\n{','''void CBitmap::CreateRGBA(int lenx, int leny, int pitch, void* data)
{
    const int checked=BitmapPitch(lenx,leny,4,pitch);
    if(data&&m_Data&&!m_DataExt) {
        const auto a=reinterpret_cast<uintptr_t>(data),b=reinterpret_cast<uintptr_t>(m_Data);
        if(a>=b&&a-b<std::size_t(m_Pitch)*m_Size.y)throw std::invalid_argument("bitmap borrows its released allocation");
    }''')
    c=c.replace('m_Pitch = std::max(pitch, lenx * 4);','m_Pitch = checked;')
    c=c.replace('void CBitmap::CreatePalate(int lenx, int leny, int palcnt) {','''void CBitmap::CreatePalate(int lenx, int leny, int palcnt) {
    BitmapPitch(lenx,leny,1);if(palcnt<1||palcnt>256)throw std::invalid_argument("bitmap palette");''')
    c=body(c,'void CBitmap::Make2xSmaller()', '''void CBitmap::Make2xSmaller() {
    CBitmap result;Make2xSmaller(CPoint(0,0),m_Size,result);Swap(result);
}''')
    c=body(c,'void CBitmap::Make2xSmaller(const Base::CPoint &lu, const Base::CPoint &size, CBitmap &des_bitmap) const', '''void CBitmap::Make2xSmaller(const Base::CPoint &lu,const Base::CPoint &size,CBitmap& destination) const {
    if(!m_Data||size.x<=0||size.y<=0||!BitmapRect(lu.x,lu.y,size.x,size.y,m_Size.x,m_Size.y)||
       (m_BytePP!=1&&m_BytePP!=3&&m_BytePP!=4))throw std::invalid_argument("bitmap downsample region");
    const int w=std::max(1,size.x/2),h=std::max(1,size.y/2);CBitmap result;
    if(m_Format==BMF_PALATE){result.CreatePalate(w,h,m_AddDataVal[0]);std::memcpy(result.m_AddData[0],m_AddData[0],m_AddDataVal[0]*4);}
    else if(m_BytePP==1)result.CreateGrayscale(w,h);
    else if(m_BytePP==3)result.CreateRGB(w,h);else result.CreateRGBA(w,h);
    for(int y=0;y<h;++y)for(int x=0;x<w;++x)for(int c=0;c<m_BytePP;++c) {
        const auto x0=lu.x+x*2,x1=std::min(x0+1,lu.x+size.x-1),y0=lu.y+y*2,y1=std::min(y0+1,lu.y+size.y-1);
        const unsigned sum=Data()[y0*m_Pitch+x0*m_BytePP+c]+Data()[y0*m_Pitch+x1*m_BytePP+c]+
            Data()[y1*m_Pitch+x0*m_BytePP+c]+Data()[y1*m_Pitch+x1*m_BytePP+c];
        result.Data()[y*result.m_Pitch+x*m_BytePP+c]=uint8_t(sum/4);
    }
    destination.Swap(result);
}''')
    c=body(c,'void CBitmap::Copy(const Base::CPoint &pdes, const Base::CPoint &size, const CBitmap &bmsou,\n                   const Base::CPoint &spsou)', '''void CBitmap::Copy(const Base::CPoint &pdes,const Base::CPoint &size,const CBitmap& source,const Base::CPoint &sp) {
    if(!m_Data||!source.m_Data||size.x<=0||size.y<=0||m_Format!=source.m_Format||
       !BitmapRect(pdes.x,pdes.y,size.x,size.y,m_Size.x,m_Size.y)||!BitmapRect(sp.x,sp.y,size.x,size.y,source.m_Size.x,source.m_Size.y))return;
    if(m_BytePP!=source.m_BytePP&&!((m_BytePP==3||m_BytePP==4)&&(source.m_BytePP==3||source.m_BytePP==4)))throw std::invalid_argument("bitmap copy conversion");
    const auto db=reinterpret_cast<uintptr_t>(m_Data),sb=reinterpret_cast<uintptr_t>(source.m_Data);
    const bool overlap=db<=sb?sb-db<std::size_t(m_Pitch)*m_Size.y:db-sb<std::size_t(source.m_Pitch)*source.m_Size.y;
    const auto row=std::size_t(size.x)*source.m_BytePP;std::vector<uint8_t> snapshot;
    if(overlap){snapshot.resize(row*size.y);for(int y=0;y<size.y;++y)std::memcpy(snapshot.data()+row*y,source.Data()+std::size_t(sp.y+y)*source.m_Pitch+sp.x*source.m_BytePP,row);}
    for(int y=0;y<size.y;++y) {
        const auto* s=overlap?snapshot.data()+row*y:source.Data()+std::size_t(sp.y+y)*source.m_Pitch+sp.x*source.m_BytePP;
        auto* d=Data()+std::size_t(pdes.y+y)*m_Pitch+pdes.x*m_BytePP;
        if(m_BytePP==source.m_BytePP)std::memcpy(d,s,row);
        else for(int x=0;x<size.x;++x){std::memcpy(d+x*m_BytePP,s+x*source.m_BytePP,3);if(m_BytePP==4)d[x*4+3]=255;}
    }
}''')
    c=c.replace('    Copy(pdes, szsou, bmsou, spsou);','''    if(desize.x<=0||desize.y<=0||szsou.x<=0||szsou.y<=0||szsou.x>desize.x||szsou.y>desize.y||
       !BitmapRect(pdes.x,pdes.y,desize.x,desize.y,m_Size.x,m_Size.y)||
       !BitmapRect(spsou.x,spsou.y,szsou.x,szsou.y,bmsou.m_Size.x,bmsou.m_Size.y))return;
    Copy(pdes, szsou, bmsou, spsou);''',1)
    guards={
      'void CBitmap::Fill': '!m_Data||size.x<=0||size.y<=0||!BitmapRect(pdes.x,pdes.y,size.x,size.y,m_Size.x,m_Size.y)',
      'void CBitmap::SwapByte': '!m_Data||size.x<=0||size.y<=0||!BitmapRect(pos.x,pos.y,size.x,size.y,m_Size.x,m_Size.y)',
      'void CBitmap::MergeWithAlpha': '!m_Data||!bmsou.m_Data||size.x<=0||size.y<=0||!BitmapRect(pdes.x,pdes.y,size.x,size.y,m_Size.x,m_Size.y)||!BitmapRect(spsou.x,spsou.y,size.x,size.y,bmsou.m_Size.x,bmsou.m_Size.y)',
      'void CBitmap::MergeByMask': '!m_Data||!bm1.m_Data||!bm2.m_Data||!mask.m_Data||size.x<=0||size.y<=0||!BitmapRect(pdes.x,pdes.y,size.x,size.y,m_Size.x,m_Size.y)||!BitmapRect(sp1.x,sp1.y,size.x,size.y,bm1.m_Size.x,bm1.m_Size.y)||!BitmapRect(sp2.x,sp2.y,size.x,size.y,bm2.m_Size.x,bm2.m_Size.y)||!BitmapRect(spm.x,spm.y,size.x,size.y,mask.m_Size.x,mask.m_Size.y)',
    }
    for signature,guard in guards.items():
        at=c.index('{',c.index(signature))+1;c=c[:at]+'\n    if('+guard+')return;'+c[at:]
    c=c.replace('void CBitmap::FlipY()\n{','void CBitmap::FlipY()\n{\n    if(!m_Data||m_Size.y<2)return;',1)
    c=re.sub(r'\*\(uint(16|32)_t \*\)des = ([^;]+);',lambda m:'BitmapWrite<uint'+m[1]+'_t>(des,'+m[2]+');',c)
    c=re.sub(r'\*\(uint(16|32)_t \*\)(des|sou)',lambda m:'BitmapRead<uint'+m[1]+'_t>('+m[2]+')',c)
    c=body(c,'void CBitmap::SaveInBMP(Base::CBuf &buf) const','')
    c=body(c,'void CBitmap::SaveInDDSUncompressed(Base::CBuf &buf) const','')
    c=body(c,'bool CBitmap::LoadFromPNG(void *buf, int buflen)','bool CBitmap::LoadFromPNG(void* bytes,int length){return LoadNativePNG(bytes,length,false);}')
    c=body(c,'bool CBitmap::LoadFromPNG(const std::wstring_view filename)','bool CBitmap::LoadFromPNG(std::wstring_view filename){return LoadNativePNGFile(filename,false);}')
    c=body(c,'bool CBitmap::SaveInPNG(CBuf &buf)', '''bool CBitmap::SaveInPNG(CBuf& buf) {
    const int needed=SaveInPNG(nullptr,0);if(needed<=0){buf.Clear();return false;}
    buf.Len(std::size_t(needed));const int written=SaveInPNG(buf.Get(),needed);
    if(written!=needed){buf.Clear();return false;}return true;
}''')
    c=c.replace('if (m_Format != BMF_FLAT)\n        return 0;','if (m_Format != BMF_FLAT || m_BytePP==2)\n        return 0;',1)
    c=c.replace('CFile file(filename.data());','CFile file{std::wstring(filename)};')
    c+='\n'+(native/'bitmap_native.inc').read_text()
    files['CBitmap.cpp']=c.encode()
