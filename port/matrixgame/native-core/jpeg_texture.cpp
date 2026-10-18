// SPDX-License-Identifier: GPL-2.0-or-later
#include "jpeg_texture.hpp"
#include "bitmap_limits.hpp"
#include <cstdio>
#include <jpeglib.h>
#include <jerror.h>
#include <csetjmp>
#include <memory>
#include <new>
#include <stdexcept>
namespace srhd_awa::matrixgame {
namespace {
static_assert(BITS_IN_JSAMPLE==8,"Native textures require an 8-bit libjpeg build");
struct FailureInfo {jpeg_error_mgr api{};std::jmp_buf jump;bool oom{},limit{};};
struct Context {
  jpeg_decompress_struct jpeg{};FailureInfo error;
  jpeg_memory_mgr original{};jpeg_progress_mgr progress{};
  std::size_t budget{},requested{};
  std::vector<JSAMPLE> row;std::vector<std::byte> rgba;
  ~Context(){if(jpeg.mem)jpeg_destroy_decompress(&jpeg);}
};
void Fail(j_common_ptr common) {
  auto* error=reinterpret_cast<FailureInfo*>(common->err);
  error->oom=common->err->msg_code==JERR_OUT_OF_MEMORY;
  std::longjmp(error->jump,1);
}
void Message(j_common_ptr common,int level){if(level<0)Fail(common);}
Context& Owner(j_common_ptr common){return *static_cast<Context*>(common->client_data);}
void Charge(j_common_ptr common,std::size_t bytes) {
  auto& c=Owner(common);
  if(bytes>c.budget-c.requested){c.error.limit=true;Fail(common);}
  c.requested+=bytes;
}
void ChargeRows(j_common_ptr common,JDIMENSION width,JDIMENSION rows,std::size_t unit) {
  auto& c=Owner(common);
  if(width>(c.budget-sizeof(void*))/unit){c.error.limit=true;Fail(common);}
  const auto row=std::size_t(width)*unit+sizeof(void*);
  if(rows>(c.budget-c.requested)/row){c.error.limit=true;Fail(common);}
  Charge(common,row*rows);
}
void* Small(j_common_ptr common,int pool,std::size_t size){Charge(common,size);return Owner(common).original.alloc_small(common,pool,size);}
void* Large(j_common_ptr common,int pool,std::size_t size){Charge(common,size);return Owner(common).original.alloc_large(common,pool,size);}
JSAMPARRAY Samples(j_common_ptr common,int pool,JDIMENSION width,JDIMENSION rows) {
  ChargeRows(common,width,rows,sizeof(JSAMPLE));return Owner(common).original.alloc_sarray(common,pool,width,rows);
}
JBLOCKARRAY Blocks(j_common_ptr common,int pool,JDIMENSION width,JDIMENSION rows) {
  ChargeRows(common,width,rows,sizeof(JBLOCK));return Owner(common).original.alloc_barray(common,pool,width,rows);
}
jvirt_sarray_ptr VirtualSamples(j_common_ptr common,int pool,boolean zero,JDIMENSION width,JDIMENSION rows,JDIMENSION access) {
  ChargeRows(common,width,rows,sizeof(JSAMPLE));return Owner(common).original.request_virt_sarray(common,pool,zero,width,rows,access);
}
jvirt_barray_ptr VirtualBlocks(j_common_ptr common,int pool,boolean zero,JDIMENSION width,JDIMENSION rows,JDIMENSION access) {
  ChargeRows(common,width,rows,sizeof(JBLOCK));return Owner(common).original.request_virt_barray(common,pool,zero,width,rows,access);
}
void Progress(j_common_ptr common) {
  auto& c=Owner(common);
  if(c.jpeg.input_scan_number>256){c.error.limit=true;Fail(common);}
}
}
TextureAsset DecodeJpegTexture(std::span<const std::byte> bytes,std::size_t budget) {
  if(bytes.size()<4||bytes.size()>::matrixgame::native::PngInputLimit||budget<sizeof(void*)||budget>::matrixgame::native::BitmapLimit)
    throw std::length_error("JPEG input/codec budget");
  // All nontrivial objects are created before setjmp or after the last codec call.
  // The callback path only mutates heap state and never throws through C frames.
  auto owner=std::make_unique<Context>();auto* c=owner.get();c->budget=budget;
  c->jpeg.err=jpeg_std_error(&c->error.api);c->error.api.error_exit=Fail;c->error.api.emit_message=Message;
  if(setjmp(c->error.jump)) {
    if(c->error.limit)throw std::length_error("JPEG codec allocation/scan budget");
    if(c->error.oom)throw std::bad_alloc();
    throw std::invalid_argument("JPEG decode failed");
  }
  jpeg_create_decompress(&c->jpeg);c->jpeg.client_data=c;c->original=*c->jpeg.mem;
  c->jpeg.mem->alloc_small=Small;c->jpeg.mem->alloc_large=Large;c->jpeg.mem->alloc_sarray=Samples;c->jpeg.mem->alloc_barray=Blocks;
  c->jpeg.mem->request_virt_sarray=VirtualSamples;c->jpeg.mem->request_virt_barray=VirtualBlocks;
  c->jpeg.mem->max_memory_to_use=long(budget);c->progress.progress_monitor=Progress;c->jpeg.progress=&c->progress;
  jpeg_mem_src(&c->jpeg,reinterpret_cast<const unsigned char*>(bytes.data()),static_cast<unsigned long>(bytes.size()));
  if(jpeg_read_header(&c->jpeg,TRUE)!=JPEG_HEADER_OK||c->jpeg.data_precision!=8||
      (c->jpeg.jpeg_color_space!=JCS_GRAYSCALE&&c->jpeg.jpeg_color_space!=JCS_RGB&&c->jpeg.jpeg_color_space!=JCS_YCbCr))
    throw std::invalid_argument("unsupported JPEG precision/color space");
  const auto width=c->jpeg.image_width,height=c->jpeg.image_height;
  const auto pitch=::matrixgame::native::BitmapPitch(int(width),int(height),4);
  c->rgba.resize(std::size_t(pitch)*height);c->row.resize(std::size_t(width)*3);
  c->jpeg.out_color_space=JCS_RGB;c->jpeg.dct_method=JDCT_ISLOW;c->jpeg.do_fancy_upsampling=TRUE;c->jpeg.do_block_smoothing=TRUE;
  if(!jpeg_start_decompress(&c->jpeg)||c->jpeg.output_width!=width||c->jpeg.output_height!=height||c->jpeg.output_components!=3)
    throw std::invalid_argument("JPEG output layout");
  while(c->jpeg.output_scanline<height) {
    const auto y=c->jpeg.output_scanline;JSAMPROW row=c->row.data();
    if(jpeg_read_scanlines(&c->jpeg,&row,1)!=1)throw std::invalid_argument("JPEG suspended input");
    auto* target=c->rgba.data()+std::size_t(y)*pitch;
    for(std::size_t x=0;x<width;++x){for(unsigned k=0;k<3;++k)target[x*4+k]=std::byte(c->row[x*3+k]);target[x*4+3]=std::byte{255};}
  }
  if(!jpeg_finish_decompress(&c->jpeg)||c->jpeg.src->bytes_in_buffer)throw std::invalid_argument("JPEG end/trailing bytes");
  jpeg_destroy_decompress(&c->jpeg);
  TextureAsset result;result.levels.push_back({width,height,std::uint32_t(pitch),std::move(c->rgba)});return result;
}
}
