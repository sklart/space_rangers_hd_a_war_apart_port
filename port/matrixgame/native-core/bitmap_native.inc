// Real native extensions and portable file serialization for CBitmap.
void CBitmap::Swap(CBitmap& other) noexcept {
    using std::swap;
    swap(m_Pos,other.m_Pos);swap(m_Size,other.m_Size);swap(m_Format,other.m_Format);
    swap(m_BytePP,other.m_BytePP);swap(m_BitPP,other.m_BitPP);swap(m_Pitch,other.m_Pitch);
    swap(m_Data,other.m_Data);swap(m_DataExt,other.m_DataExt);
    for(int i=0;i<4;++i){swap(m_MColor[i],other.m_MColor[i]);swap(m_AddData[i],other.m_AddData[i]);swap(m_AddDataExt[i],other.m_AddDataExt[i]);swap(m_AddDataVal[i],other.m_AddDataVal[i]);}
}
CBitmap::CBitmap(CBitmap&& other) noexcept : CBitmap() {Swap(other);}
CBitmap& CBitmap::operator=(CBitmap&& other) noexcept {if(this!=&other){Clear();Swap(other);}return *this;}
bool CBitmap::LoadNativePNG(void* bytes,int length,bool rgba) {
    Clear();
    if(!bytes||length<=0||std::size_t(length)>matrixgame::native::PngInputLimit)return false;
    uint32_t w=0,h=0,colors=0,format=0;
    struct Owner {uintptr_t id{};~Owner(){if(id)FilePNG::ReadCancel(id);}} owner;
    owner.id=(rgba?FilePNG::ReadStart_RGBA:FilePNG::ReadStart_Buf)(bytes,uint32_t(length),&w,&h,&colors,&format);
    if(!owner.id)return false;
    try {
        if(format==1)CreateGrayscale(int(w),int(h));
        else if(format==2)CreateRGB(int(w),int(h));
        else if(format==3)CreateRGBA(int(w),int(h));
        else if(format==4)CreatePalate(int(w),int(h),int(colors));
        else throw std::invalid_argument("PNG format");
        const auto consuming=std::exchange(owner.id,uintptr_t(0));
        if(!FilePNG::Read(consuming,m_Data,uint32_t(m_Pitch),static_cast<uint32_t*>(m_AddData[0]))){Clear();return false;}
        return true;
    }catch(...){Clear();throw;}
}
bool CBitmap::LoadNativePNGFile(std::wstring_view name,bool rgba) {
    CFile file{std::wstring(name)};file.OpenRead();const auto size=file.Size();
    if(size<=0||std::uint64_t(size)>matrixgame::native::PngInputLimit){Clear();return false;}
    CBuf bytes;bytes.Len(std::size_t(size));file.Read(bytes.Get(),std::size_t(size));
    return LoadNativePNG(bytes.Get(),int(size),rgba);
}
bool CBitmap::LoadRGBAFromPNG(void* bytes,int length){return LoadNativePNG(bytes,length,true);}
bool CBitmap::LoadRGBAFromPNG(std::wstring_view name){return LoadNativePNGFile(name,true);}
void CBitmap::SaveInBMP(Base::CBuf& buf) const {
    if(!m_Data||!(m_BytePP==1||m_BytePP==3||m_BytePP==4))throw std::invalid_argument("BMP format");
    const uint32_t pitch=(uint32_t(m_Size.x)*m_BytePP+3)&~3u;
    const uint32_t colors=m_BytePP==1?(m_Format==BMF_PALATE?uint32_t(m_AddDataVal[0]):256u):0u;
    const uint32_t offset=54+colors*4,size=offset+pitch*m_Size.y;
    std::vector<uint8_t> file(size,0);
    auto w16=[&](std::size_t at,uint16_t v){matrixgame::native::BitmapWrite(file.data()+at,v);};
    auto w32=[&](std::size_t at,uint32_t v){matrixgame::native::BitmapWrite(file.data()+at,v);};
    w16(0,0x4d42);w32(2,size);w32(10,offset);w32(14,40);w32(18,m_Size.x);w32(22,m_Size.y);
    w16(26,1);w16(28,uint16_t(m_BitPP));w32(34,pitch*m_Size.y);w32(46,colors);
    for(uint32_t i=0;i<colors;++i) {
        const auto c=m_Format==BMF_PALATE?static_cast<const uint32_t*>(m_AddData[0])[i]:i*0x010101u;
        w32(54+i*4,((c&0xff)<<16)|(c&0xff00)|((c>>16)&0xff));
    }
    for(int y=0;y<m_Size.y;++y) {
        auto* dst=file.data()+offset+std::size_t(m_Size.y-1-y)*pitch;
        const auto* src=Data()+std::size_t(y)*m_Pitch;
        if(m_BytePP==1)std::memcpy(dst,src,m_Size.x);
        else for(int x=0;x<m_Size.x;++x){dst[x*m_BytePP]=src[x*m_BytePP+2];dst[x*m_BytePP+1]=src[x*m_BytePP+1];dst[x*m_BytePP+2]=src[x*m_BytePP];if(m_BytePP==4)dst[x*4+3]=src[x*4+3];}
    }
    buf.Clear();buf.Add(file.data(),file.size());
}
void CBitmap::SaveInDDSUncompressed(Base::CBuf& buf) const {
    if(!m_Data||(m_BytePP!=3&&m_BytePP!=4))throw std::invalid_argument("DDS format");
    const auto bytes=uint32_t(m_Size.x)*m_Size.y*m_BytePP;
    std::array<uint32_t,32> header{};
    header[0]=0x20534444;header[1]=124;header[2]=0x81007;header[3]=m_Size.y;header[4]=m_Size.x;header[5]=bytes;
    header[19]=32;header[20]=0x40|(m_BytePP==4?1:0);header[22]=m_BitPP;
    header[23]=0xff0000;header[24]=0xff00;header[25]=0xff;header[26]=m_BytePP==4?0xff000000:0;header[27]=0x1000;
    buf.Clear();buf.Add(header.data(),sizeof(header));
    std::vector<uint8_t> row(std::size_t(m_Size.x)*m_BytePP);
    for(int y=0;y<m_Size.y;++y) {
        const auto* src=Data()+std::size_t(y)*m_Pitch;
        for(int x=0;x<m_Size.x;++x){row[x*m_BytePP]=src[x*m_BytePP+2];row[x*m_BytePP+1]=src[x*m_BytePP+1];row[x*m_BytePP+2]=src[x*m_BytePP];if(m_BytePP==4)row[x*4+3]=src[x*4+3];}
        buf.Add(row.data(),row.size());
    }
}
