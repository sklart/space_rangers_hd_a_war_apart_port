#include "gi_format2_cpu.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace srhd_awa::platform::gi_format2_cpu;
namespace {
void W(std::vector<std::uint8_t>* b,std::size_t at,std::int32_t value){auto u=static_cast<std::uint32_t>(value);for(int n=0;n<4;++n)(*b)[at+n]=static_cast<std::uint8_t>(u>>(8*n));}
void Plane(std::vector<std::uint8_t>* b,int index,int offset,const std::vector<std::uint8_t>&stream,int left,int top,int width,int height){auto at=64+index*32;W(b,at,offset);W(b,at+4,16+static_cast<int>(stream.size()));W(b,at+8,left);W(b,at+12,top);W(b,at+16,left+width);W(b,at+20,top+height);W(b,offset,static_cast<int>(stream.size()));W(b,offset+4,width);W(b,offset+8,height);std::memcpy(b->data()+offset+16,stream.data(),stream.size());}
std::vector<std::uint8_t> Fixture(){std::vector<std::uint8_t>b(320);std::memcpy(b.data(),"gi",2);W(&b,4,1);W(&b,16,4);W(&b,20,3);W(&b,40,2);W(&b,44,3);
  Plane(&b,2,160,{0x84,0,63,32,16,0,0x80,0x02,0x82,8,48,0},0,0,4,3);
  Plane(&b,1,192,{0x82,0,0xf8,0xe0,0x07,0x02,0,0x80,0x84,0x1f,0,0,0xf8,0xe0,0x07,0xff,0xff,0},0,0,4,3);
  Plane(&b,0,232,{0x01,0x83,0,0xf8,0xe0,0x07,0xff,0xff,0,0x80,0x84,0x1f,0,0,0xf8,0xe0,0x07,0xff,0xff,0},0,0,4,3);return b;}
bool Empty(const CpuImage&i){return !i.width&&!i.height&&!i.pitch&&!i.bytes_per_pixel&&i.pixels.empty();}
bool Reject(std::vector<std::uint8_t>b,const char*label){CpuImage i;i.width=1;i.pixels={1};std::string e;if(srhd_awa::platform::gi_format2_cpu::Decode(b.data(),b.size(),nullptr,&i,&e)==Status::Ok||!Empty(i)){std::printf("accepted %s\\n",label);return false;}return true;}
}
int main(){auto b=Fixture();Metadata a,c;CpuImage x,y;std::string e;if(Decode(b.data(),b.size(),&a,&x,&e)!=Status::Ok||Decode(b.data(),b.size(),&c,&y,&e)!=Status::Ok||x.pixels!=y.pixels||x.width!=4||x.height!=3||x.pitch!=16||x.bytes_per_pixel!=4||a.version!=1||a.plane_count!=3)return 1;
  const std::uint8_t expected[]={0,0,250,252,0,0,248,255,0,252,0,255,248,252,248,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,248,0,0,255,0,0,248,255,0,252,0,255,248,252,248,255};
  if(x.pixels.size()!=sizeof expected||std::memcmp(x.pixels.data(),expected,sizeof expected))return 1;
  auto q=b;
  q.resize(63); if(!Reject(q,"short"))return 1;
  q=b; q[0]='x'; if(!Reject(q,"magic"))return 1;
  q=b; W(&q,4,2); if(!Reject(q,"version"))return 1;
  q=b; W(&q,40,1); if(!Reject(q,"format"))return 1;
  q=b; W(&q,16,0); if(!Reject(q,"bounds"))return 1;
  q=b; W(&q,16,0x7fffffff); W(&q,20,0x7fffffff); if(!Reject(q,"huge"))return 1;
  q=b; W(&q,44,2); if(!Reject(q,"planes"))return 1;
  q=b; q.resize(159); if(!Reject(q,"truncated table"))return 1;
  q=b; W(&q,128,-1); if(!Reject(q,"negative offset"))return 1;
  q=b; W(&q,128,321); if(!Reject(q,"offset outside"))return 1;
  q=b; W(&q,132,15); if(!Reject(q,"short plane"))return 1;
  q=b; W(&q,160,-1); if(!Reject(q,"negative stream"))return 1;
  q=b; W(&q,164,0); if(!Reject(q,"zero rle width"))return 1;
  q=b; W(&q,168,0); if(!Reject(q,"zero rle height"))return 1;
  q=b; W(&q,136,1); W(&q,144,5); if(!Reject(q,"destination outside"))return 1;
  q=b; W(&q,160,2); q[176]=0xff; if(!Reject(q,"literal row overflow"))return 1;
  q=b; W(&q,160,1); q[176]=0; if(!Reject(q,"short row"))return 1;
  q=b; W(&q,160,5); q[176]=0x80; if(!Reject(q,"trailing"))return 1;
  q=b; W(&q,160,2); q[176]=0x81; if(!Reject(q,"truncated literal"))return 1;
  q=b; W(&q,160,2); q[176]=1; q[177]=0x80; if(!Reject(q,"blank after pixels"))return 1;
  std::puts("GI FORMAT2 CPU PASS");return 0;}
