#include "okgf_bridge.hpp"
#include <okgf.h>
#include <cstddef>
namespace srhd_awa::platform::okgf_bridge {
static_assert(sizeof(WindowsSdk::TRect) == sizeof(OkgfRect));
static_assert(offsetof(WindowsSdk::TRect, Left) == offsetof(OkgfRect, left));
static_assert(offsetof(WindowsSdk::TRect, Top) == offsetof(OkgfRect, top));
static_assert(offsetof(WindowsSdk::TRect, Right) == offsetof(OkgfRect, right));
static_assert(offsetof(WindowsSdk::TRect, Bottom) == offsetof(OkgfRect, bottom));
void CopyWord(void* d,std::int32_t dp,std::int32_t dx,std::int32_t dy,void* s,std::int32_t sp,std::int32_t sx,std::int32_t sy,std::int32_t w,std::int32_t h){::OKGR_Copy_XY_XY_WORD(d,dp,dx,dy,s,sp,sx,sy,w,h);}
void FillWord(void* p,std::int32_t q,std::int32_t w,std::int32_t h,std::uint16_t c){::OKGR_Fill_WORD(p,q,w,h,c);}
void Convert565ToBgra(void* s,std::int32_t sp,void* d,std::int32_t dp,std::int32_t w,std::int32_t h){::OKGF_Convert565toBGRA(s,sp,d,dp,w,h);}
void PixelAlpha16(void* p,std::uint16_t c,std::uint8_t a){::OKGR_PixelAlpha_16(p,c,a);}
void* BeginImageRead(void* source,std::int32_t size,std::int32_t* width,std::int32_t* height){return ::OKGF_ReadStart_Buf(source,size,width,height);}
std::int32_t ReadImagePixels(void* context,void* pixels,std::int32_t pitch,std::uint32_t red,std::uint32_t green,std::uint32_t blue,std::uint32_t alpha,std::int32_t bpp){return ::OKGF_Read(static_cast<OkgfReadContext*>(context),pixels,pitch,red,green,blue,alpha,bpp);}
void LineIp16(void* p,std::int32_t q,std::int32_t x,std::int32_t y,std::uint32_t c,std::int32_t x2,std::int32_t y2,std::uint32_t c2){::OKGF_LineIp_16(p,q,x,y,c,x2,y2,c2);}
void Triangle16(void* p,std::int32_t q,std::int32_t x,std::int32_t y,std::uint32_t c,std::int32_t x2,std::int32_t y2,std::uint32_t c2,std::int32_t x3,std::int32_t y3,std::uint32_t c3,const WindowsSdk::TRect* r){
  if (!r) { ::OKGF_Triangle_16(p,q,x,y,c,x2,y2,c2,x3,y3,c3,nullptr); return; }
  OkgfRect clip{r->Left,r->Top,r->Right,r->Bottom};
  ::OKGF_Triangle_16(p,q,x,y,c,x2,y2,c2,x3,y3,c3,&clip);
}
}
