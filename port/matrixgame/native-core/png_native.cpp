// MatrixGame - SR2 Planetary battles engine
// Copyright (C) 2012, Elemental Games, Katauri Interactive, CHK-Games
// Licensed under GPLv2 or any later version. See upstream/matrixgame/LICENSE.
// Native implementation of the real FilePNG API; no Windows/device functions.
#include "FilePNG.hpp"
#include "bitmap_limits.hpp"
#include <png.h>
#include <csetjmp>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
namespace {
using matrixgame::native::BitmapLimit;
using matrixgame::native::PngInputLimit;
struct Context {
  png_structp png{};png_infop info{};png_bytep* rows{};
  const std::uint8_t* input{};std::size_t length{},position{};
  std::uint8_t* output{};std::size_t capacity{},written{},allocated{};
  std::uint32_t width{},height{};bool writing{},paletted{};
};
// Windows x64 jmp_buf requires 16-byte alignment although max_align_t is 8.
// Preserve that ABI independently of header size and the CRT malloc alignment.
struct Allocation {void* base;std::size_t size;};
constexpr std::size_t PngAlignment=alignof(std::max_align_t)>16?alignof(std::max_align_t):16;
static_assert(PngAlignment>=alignof(jmp_buf));
png_voidp PNGCBAPI Allocate(png_structp png,png_alloc_size_t size) {
  auto* c=static_cast<Context*>(png_get_mem_ptr(png));
  if(!size||size>BitmapLimit-c->allocated||size>SIZE_MAX-sizeof(Allocation)-PngAlignment)return nullptr;
  auto* base=std::malloc(std::size_t(size)+sizeof(Allocation)+PngAlignment-1);
  if(!base)return nullptr;
  const auto aligned=(reinterpret_cast<std::uintptr_t>(base)+sizeof(Allocation)+PngAlignment-1)&~std::uintptr_t(PngAlignment-1);
  auto* p=reinterpret_cast<Allocation*>(aligned)-1;p->base=base;p->size=std::size_t(size);c->allocated+=p->size;
  return reinterpret_cast<void*>(aligned);
}
void PNGCBAPI Free(png_structp png,png_voidp pointer) {
  if(!pointer)return;
  auto* c=static_cast<Context*>(png_get_mem_ptr(png));auto* p=static_cast<Allocation*>(pointer)-1;
  c->allocated-=p->size;std::free(p->base);
}
void PNGCBAPI Error(png_structp png,png_const_charp) {png_longjmp(png,1);}
void PNGCBAPI Warning(png_structp,png_const_charp) {}
void PNGCBAPI ReadBytes(png_structp png,png_bytep output,png_size_t length) {
  auto* c=static_cast<Context*>(png_get_io_ptr(png));
  if(length>c->length-c->position)png_error(png,"short PNG input");
  std::memcpy(output,c->input+c->position,length);c->position+=length;
}
void PNGCBAPI WriteBytes(png_structp png,png_bytep input,png_size_t length) {
  auto* c=static_cast<Context*>(png_get_io_ptr(png));
  if(length>PngInputLimit-c->written)png_error(png,"PNG output limit");
  if(c->written<=c->capacity&&length<=c->capacity-c->written&&length)
    std::memcpy(c->output+c->written,input,length);
  c->written+=length;
}
void Destroy(Context* c) noexcept {
  if(!c)return;
  if(c->png&&c->rows)png_free(c->png,c->rows);
  if(c->png){if(c->writing)png_destroy_write_struct(&c->png,&c->info);else png_destroy_read_struct(&c->png,&c->info,nullptr);}
  delete c;
}
std::uintptr_t Begin(void* input,std::uint32_t length,std::uint32_t* w,std::uint32_t* h,
    std::uint32_t* palette,std::uint32_t* format,bool rgba) {
  if(w)*w=0;if(h)*h=0;if(palette)*palette=0;if(format)*format=0;
  if(!input||length<8||length>PngInputLimit||!w||!h||!palette||!format||png_sig_cmp(static_cast<png_const_bytep>(input),0,8))return 0;
  auto* c=new(std::nothrow) Context;if(!c)return 0;
  c->input=static_cast<const std::uint8_t*>(input);c->length=length;
  c->png=png_create_read_struct_2(PNG_LIBPNG_VER_STRING,nullptr,Error,Warning,c,Allocate,Free);
  if(!c->png){Destroy(c);return 0;}
  // No C++ objects are constructed across a libpng longjmp. All cleanup state
  // is in the heap context allocated before setjmp.
  if(setjmp(png_jmpbuf(c->png))){Destroy(c);return 0;}
  c->info=png_create_info_struct(c->png);if(!c->info)png_error(c->png,"PNG info allocation");
  png_set_user_limits(c->png,8192,8192);
  png_set_chunk_malloc_max(c->png,BitmapLimit);png_set_chunk_cache_max(c->png,128);
  png_set_read_fn(c->png,c,ReadBytes);png_read_info(c->png,c->info);
  c->width=png_get_image_width(c->png,c->info);c->height=png_get_image_height(c->png,c->info);
  const auto type=png_get_color_type(c->png,c->info),depth=png_get_bit_depth(c->png,c->info);
  if(!c->width||!c->height||(rgba&&std::uint64_t(c->width)*c->height>BitmapLimit/4))png_error(c->png,"PNG dimensions");
  png_set_strip_16(c->png);png_set_packing(c->png);
  if(rgba) {
    if(type==PNG_COLOR_TYPE_PALETTE)png_set_palette_to_rgb(c->png);
    if(type==PNG_COLOR_TYPE_GRAY&&depth<8)png_set_expand_gray_1_2_4_to_8(c->png);
    const bool transparency=png_get_valid(c->png,c->info,PNG_INFO_tRNS)!=0;
    if(transparency)png_set_tRNS_to_alpha(c->png);
    if(type==PNG_COLOR_TYPE_GRAY||type==PNG_COLOR_TYPE_GRAY_ALPHA)png_set_gray_to_rgb(c->png);
    if(!(type&PNG_COLOR_MASK_ALPHA)&&!transparency)png_set_add_alpha(c->png,255,PNG_FILLER_AFTER);
  }
  png_set_interlace_handling(c->png);png_read_update_info(c->png,c->info);
  const auto final_type=png_get_color_type(c->png,c->info);
  std::uint32_t colors=0,result=0;
  if(rgba)result=3;
  else if(final_type==PNG_COLOR_TYPE_GRAY)result=1;
  else if(final_type==PNG_COLOR_TYPE_RGB)result=2;
  else if(final_type==PNG_COLOR_TYPE_RGB_ALPHA)result=3;
  else if(final_type==PNG_COLOR_TYPE_PALETTE) {
    png_colorp table=nullptr;int n=0;
    if(!png_get_PLTE(c->png,c->info,&table,&n)||n<1||n>256)png_error(c->png,"PNG palette");
    colors=std::uint32_t(n);result=4;
  }else png_error(c->png,"unsupported legacy PNG format");
  const auto row=png_get_rowbytes(c->png,c->info);
  if(row>BitmapLimit/c->height || (rgba&&row!=std::size_t(c->width)*4))png_error(c->png,"PNG row extent");
  *w=c->width;*h=c->height;*palette=colors;*format=result;
  c->paletted=result==4;
  return reinterpret_cast<std::uintptr_t>(c);
}
}
namespace FilePNG {
std::uintptr_t ReadStart_Buf(void* input,std::uint32_t bytes,std::uint32_t* w,std::uint32_t* h,std::uint32_t* count,std::uint32_t* format) {
  return Begin(input,bytes,w,h,count,format,false);
}
std::uintptr_t ReadStart_RGBA(void* input,std::uint32_t bytes,std::uint32_t* w,std::uint32_t* h,std::uint32_t* count,std::uint32_t* format) {
  return Begin(input,bytes,w,h,count,format,true);
}
void ReadCancel(std::uintptr_t id) noexcept {Destroy(reinterpret_cast<Context*>(id));}
std::uint32_t Read(std::uintptr_t id,void* pixels,std::uint32_t pitch,std::uint32_t* palette) {
  auto* c=reinterpret_cast<Context*>(id);if(!c)return 0;
  if(!pixels||!pitch||std::uint64_t(pitch)*c->height>BitmapLimit){Destroy(c);return 0;}
  if(setjmp(png_jmpbuf(c->png))){Destroy(c);return 0;}
  if(png_get_rowbytes(c->png,c->info)>pitch)png_error(c->png,"short PNG row");
  png_colorp colors=nullptr;int count=0;
  if(c->paletted&&png_get_PLTE(c->png,c->info,&colors,&count)&&count) {
    if(!palette||count>256)png_error(c->png,"missing PNG palette output");
    for(int i=0;i<count;++i)palette[i]=std::uint32_t(colors[i].red)|std::uint32_t(colors[i].green)<<8|std::uint32_t(colors[i].blue)<<16;
  }
  c->rows=static_cast<png_bytep*>(png_malloc(c->png,std::size_t(c->height)*sizeof(png_bytep)));
  for(std::uint32_t y=0;y<c->height;++y)c->rows[y]=static_cast<png_bytep>(pixels)+std::size_t(y)*pitch;
  png_read_image(c->png,c->rows);png_read_end(c->png,c->info);Destroy(c);return 1;
}
int Write(void* output,int capacity,void* pixels,std::uint32_t pitch,std::uint32_t w,std::uint32_t h,std::uint32_t bytes,int bgr) {
  if(capacity<0||(!output&&capacity)||!pixels||!w||!h||w>8192||h>8192||bytes<1||bytes>4||
      std::uint64_t(w)*bytes>pitch||std::uint64_t(pitch)*h>BitmapLimit)return 0;
  auto* c=new(std::nothrow) Context;if(!c)return 0;
  c->writing=true;c->output=static_cast<std::uint8_t*>(output);c->capacity=capacity;
  c->png=png_create_write_struct_2(PNG_LIBPNG_VER_STRING,nullptr,Error,Warning,c,Allocate,Free);
  if(!c->png){Destroy(c);return 0;}
  if(setjmp(png_jmpbuf(c->png))){Destroy(c);return 0;}
  c->info=png_create_info_struct(c->png);if(!c->info)png_error(c->png,"PNG info allocation");
  const int types[]={0,PNG_COLOR_TYPE_GRAY,PNG_COLOR_TYPE_GRAY_ALPHA,PNG_COLOR_TYPE_RGB,PNG_COLOR_TYPE_RGB_ALPHA};
  png_set_write_fn(c->png,c,WriteBytes,nullptr);
  png_set_IHDR(c->png,c->info,w,h,8,types[bytes],PNG_INTERLACE_NONE,PNG_COMPRESSION_TYPE_BASE,PNG_FILTER_TYPE_BASE);
  if(bgr)png_set_bgr(c->png);png_write_info(c->png,c->info);
  c->rows=static_cast<png_bytep*>(png_malloc(c->png,std::size_t(h)*sizeof(png_bytep)));
  for(std::uint32_t y=0;y<h;++y)c->rows[y]=static_cast<png_bytep>(pixels)+std::size_t(y)*pitch;
  png_write_image(c->png,c->rows);png_write_end(c->png,c->info);
  const auto size=c->written;Destroy(c);return static_cast<int>(size);
}
}
