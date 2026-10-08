#include "gai_cpu.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace srhd_awa::platform::gai_cpu;
namespace gi = srhd_awa::platform::gi_format0_cpu;
namespace {
constexpr std::size_t kGiSize = 104;
constexpr unsigned char kCompressed[] = {
  0x78,0xda,0x4b,0xcf,0x8c,0x31,0x60,0x64,0x40,0x00,0x26,0x28,0x66,0xf8,0xc1,0xc0,
  0xf0,0x80,0x9d,0x81,0x41,0x1e,0x49,0x0e,0x59,0x1d,0x0c,0x24,0x00,0x31,0x07,0x36,
  0xfd,0x70,0xf0,0xff,0x3f,0xc3,0x8f,0x07,0xec,0x00,0x48,0x07,0x07,0xaa};
void W32(std::vector<unsigned char>* b, std::size_t at, std::int32_t n) {
  const auto u = static_cast<std::uint32_t>(n); for (int i=0;i<4;++i) (*b)[at+i]=static_cast<unsigned char>(u>>(i*8));
}
void WU32(std::vector<unsigned char>* b, std::size_t at, std::uint32_t n) { W32(b,at,static_cast<std::int32_t>(n)); }
std::vector<unsigned char> Gi() {
  std::vector<unsigned char> b(kGiSize); std::memcpy(b.data(),"gi\0",3); W32(&b,4,1); W32(&b,16,2); W32(&b,20,2);
  WU32(&b,24,0xf800); WU32(&b,28,0x07e0); WU32(&b,32,0x001f); W32(&b,40,0); W32(&b,44,1);
  W32(&b,64,96); W32(&b,68,8); W32(&b,80,2); W32(&b,84,2);
  const unsigned char pixels[]={0,0,255,255,0,248,224,7}; std::memcpy(b.data()+96,pixels,sizeof pixels); return b;
}
std::vector<unsigned char> Gai(bool sequence=true) {
  const auto raw=Gi(); const std::size_t frames=3, directory=48+frames*8, raw_at=directory, zl_at=raw_at+raw.size(), table=zl_at+8+sizeof kCompressed;
  std::vector<unsigned char> b(table+(sequence?56:0)); std::memcpy(b.data(),"gai\0",4); W32(&b,4,1); W32(&b,16,2); W32(&b,20,2); W32(&b,24,3); W32(&b,28,0);
  W32(&b,32,sequence?static_cast<std::int32_t>(table):0); W32(&b,36,sequence?56:0);
  W32(&b,48,static_cast<std::int32_t>(raw_at)); W32(&b,52,static_cast<std::int32_t>(raw.size()));
  W32(&b,56,static_cast<std::int32_t>(zl_at)); W32(&b,60,static_cast<std::int32_t>(8+sizeof kCompressed));
  std::memcpy(b.data()+raw_at,raw.data(),raw.size()); std::memcpy(b.data()+zl_at,"ZL01",4); W32(&b,zl_at+4,static_cast<std::int32_t>(raw.size())); std::memcpy(b.data()+zl_at+8,kCompressed,sizeof kCompressed);
  if(sequence){W32(&b,table,2); W32(&b,table+8,24); W32(&b,table+16,44); W32(&b,table+24,2); W32(&b,table+28,0); W32(&b,table+32,20); W32(&b,table+36,1); W32(&b,table+40,40); W32(&b,table+44,1); W32(&b,table+48,2); W32(&b,table+52,60);} return b;
}
std::uint32_t C(const std::vector<std::uint8_t>&v){std::uint32_t c=~0u;for(auto b:v){c^=b;for(int n=0;n<8;++n)c=(c>>1)^((c&1)?0xedb88320u:0);}return~c;}
std::uint64_t F(const std::vector<std::uint8_t>&v){std::uint64_t h=14695981039346656037ull;for(auto b:v)h=(h^b)*1099511628211ull;return h;}
bool Empty(const gi::CpuImage&i){return !i.width&&!i.height&&!i.pitch&&!i.bytes_per_pixel&&i.pixels.empty();}
bool Reject(std::vector<unsigned char>b,const char*label,int frame=0){GaiFramePayload p;gi::CpuImage image;std::string error; if(DecodeGaiFormat0Frame(b.empty()?nullptr:b.data(),b.size(),frame,nullptr,nullptr,nullptr,&image,&error)==Status::Ok||!Empty(image)||!p.gi_bytes.empty()){std::printf("accepted corrupt %s\n",label);return false;}return true;}
}
int main(){
  auto b=Gai(); GaiMetadata meta{}; GaiFrameInfo raw{}, compressed{}, empty{}; GaiSequence first{}, second{}; gi::Metadata ga{},gb{};gi::CpuImage a,c;
  const auto validate_status=ValidateGai(b.data(),b.size(),&meta); const auto raw_status=ReadGaiFrameInfo(b.data(),b.size(),0,&raw); const auto compressed_status=ReadGaiFrameInfo(b.data(),b.size(),1,&compressed); const auto empty_status=ReadGaiFrameInfo(b.data(),b.size(),2,&empty); const auto raw_decode_status=DecodeGaiFormat0Frame(b.data(),b.size(),0,&meta,&raw,&ga,&a); const auto compressed_decode_status=DecodeGaiFormat0Frame(b.data(),b.size(),1,nullptr,&compressed,&gb,&c);
  const auto sequence0_status=ReadGaiSequence(b.data(),b.size(),0,&first); const auto sequence1_status=ReadGaiSequence(b.data(),b.size(),1,&second);
  const bool valid_path=validate_status==Status::Ok&&meta.version==1&&meta.frame_count==3&&meta.flags==0&&meta.sequence_table_present&&meta.sequence_count==2&&sequence0_status==Status::Ok&&sequence1_status==Status::Ok&&first.frames.size()==2&&first.frames[0].source_frame_index==0&&first.frames[0].delay_ms==20&&first.frames[1].source_frame_index==1&&first.frames[1].delay_ms==40&&second.frames.size()==1&&second.frames[0].source_frame_index==2&&second.frames[0].delay_ms==60&&
    raw_status==Status::Ok&&raw.encoding==FrameEncoding::RawGi&&compressed_status==Status::Ok&&compressed.encoding==FrameEncoding::Zl01&&empty_status==Status::Ok&&empty.encoding==FrameEncoding::Empty&&
    raw_decode_status==Status::Ok&&compressed_decode_status==Status::Ok&&a.pixels==c.pixels&&ga.width==gb.width&&ga.height==gb.height&&a.width==2&&a.height==2&&a.pitch==8&&a.bytes_per_pixel==4&&C(a.pixels)==0xf8a05355u&&F(a.pixels)==0x2774951ee33e6f0dull;
  if (!valid_path) std::fprintf(stderr,"valid path failed v=%u r=%u c=%u e=%u dr=%u dc=%u meta=%d/%d seq=%d/%d enc=%u/%u/%u image=%d,%d,%d,%d crc=%08x fnv=%016llx\n",static_cast<unsigned>(validate_status),static_cast<unsigned>(raw_status),static_cast<unsigned>(compressed_status),static_cast<unsigned>(empty_status),static_cast<unsigned>(raw_decode_status),static_cast<unsigned>(compressed_decode_status),meta.version,meta.frame_count,meta.sequence_table_present?1:0,meta.sequence_count,static_cast<unsigned>(raw.encoding),static_cast<unsigned>(compressed.encoding),static_cast<unsigned>(empty.encoding),a.width,a.height,a.pitch,a.bytes_per_pixel,C(a.pixels),static_cast<unsigned long long>(F(a.pixels)));
  bool ok=valid_path;
  GaiFramePayload p;std::string error; ok=ok&&ExtractGaiFrame(b.data(),b.size(),2,&p,&error)==Status::EmptyFrame&&p.gi_bytes.empty()&&DecodeGaiFormat0Frame(b.data(),b.size(),1,nullptr,nullptr,nullptr,&c,&error)==Status::Ok&&Empty(c)==false;
  const auto compressed_at=static_cast<std::size_t>(compressed.data_offset); auto x=b; x.resize(47);ok=ok&&Reject(x,"short header");x=b;x[0]='x';ok=ok&&Reject(x,"magic");x=b;W32(&x,4,2);ok=ok&&Reject(x,"version");x=b;W32(&x,24,0);ok=ok&&Reject(x,"zero frames");x=b;W32(&x,24,-1);ok=ok&&Reject(x,"negative frames");x=b;W32(&x,24,100001);ok=ok&&Reject(x,"huge frames");x=b;W32(&x,48,-1);ok=ok&&Reject(x,"negative frame offset");x=b;W32(&x,52,-1);ok=ok&&Reject(x,"negative frame size");x=b;W32(&x,64,-1);ok=ok&&Reject(x,"negative empty frame size");x=b;W32(&x,48,99999);ok=ok&&Reject(x,"outside frame");x=b;W32(&x,32,99999);ok=ok&&Reject(x,"bad sequence offset");x=b;W32(&x,36,-1);ok=ok&&Reject(x,"negative sequence size");x=b;W32(&x,static_cast<std::size_t>(meta.sequence_table_offset),-1);ok=ok&&Reject(x,"negative sequence count");x=b;W32(&x,static_cast<std::size_t>(meta.sequence_table_offset)+28,-1);ok=ok&&Reject(x,"invalid source frame");x=b;W32(&x,compressed_at+4,0);ok=ok&&Reject(x,"compressed zero size",1);x=b;W32(&x,compressed_at+4,0x7fffffff);ok=ok&&Reject(x,"compressed huge size",1);x=b;x[compressed_at+8]=0;ok=ok&&Reject(x,"invalid compressed data",1);x=b;x[compressed_at+1]='Z';ok=ok&&Reject(x,"unknown encoding",1);ok=ok&&ReadGaiSequence(b.data(),b.size(),2,&first)==Status::InvalidFrame;
  if(!ok) { std::fputs("GAI CPU FAIL\n", stderr); return 1; }
  std::puts("GAI CPU PASS");
}
