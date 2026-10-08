#include "trans_bitmap_cpu.hpp"
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
namespace { using srhd_awa::platform::trans_bitmap_cpu::TransBitmap; using srhd_awa::platform::okgf_rle_bridge::Rect;
bool E(bool v,const char*s){if(!v)std::fprintf(stderr,"FAIL: %s\n",s);return v;} void W(std::vector<std::uint8_t>*b,int p,std::uint32_t v){for(int i=0;i<4;++i)(*b)[p+i]=static_cast<std::uint8_t>(v>>(i*8));}
std::vector<std::uint8_t>B(){std::vector<std::uint8_t>b(70);b[0]='B';b[1]='M';W(&b,2,70);W(&b,10,54);W(&b,14,40);W(&b,18,2);W(&b,22,2);b[26]=1;b[28]=24;W(&b,34,16);b[54]=255;b[57]=0;b[62]=0;b[63]=0;b[64]=255;b[65]=0;b[66]=255;return b;}
bool T(){const auto b=B();TransBitmap x;std::string e;if(!E(x.Load(b.data(),b.size(),"",&e),"load")||!E(x.width()==2&&x.height()==2,"natural"))return false;std::vector<std::uint16_t>d(16,0x001f);if(!E(x.Draw(d.data(),4,4,4,1,1,false,{0,0,4,4},&e),"normal")||!E(d[5]==0xf800&&d[6]==0x07e0&&d[9]==0x001f,"key"))return false;d.assign(16,0x001f);if(!E(x.Draw(d.data(),4,4,4,1,1,true,{0,0,4,4},&e)&&d[5]==0x780f,"half"))return false;if(!E(x.Load(b.data(),b.size(),"270",&e)&&x.width()==2&&x.height()==2,"rotate"))return false;if(!E(x.Load(b.data(),b.size(),"Stretch=3,1",&e)&&x.width()==3&&x.height()==1,"stretch"))return false;return E(x.Load(b.data(),b.size(),"270&Stretch=1,3",&e)&&x.width()==1&&x.height()==3,"combined");}
}int main(){if(!T())return 1;std::puts("TRANS BITMAP CPU PASS");return 0;}
