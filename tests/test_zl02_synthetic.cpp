#include "package.hpp"
#include <cstdio>
#include <cstring>
#include <vector>
#include <zlib.h>

static void u32(FILE* f, uint32_t n) { for (int i=0;i<4;++i) std::fputc((n>>(i*8))&255,f); }
static bool run(size_t size) {
  std::vector<unsigned char> source(size); for(size_t i=0;i<size;++i) source[i]=static_cast<unsigned char>(i*37u);
  FILE* f=std::fopen("build/synthetic.pkg","wb"); if(!f) return false;
  u32(f,4); u32(f,170); u32(f,1); u32(f,158);
  u32(f,0); u32(f,static_cast<uint32_t>(size)); char names[126]{}; std::memcpy(names,"FILE",4); std::memcpy(names+63,"FILE",4); std::fwrite(names,1,126,f); u32(f,2); u32(f,2); u32(f,0); u32(f,0); u32(f,174); u32(f,0);
  u32(f,0); size_t at=0; while(at<size){size_t n=std::min<size_t>(65536,size-at); uLong bound=compressBound(n); std::vector<unsigned char> packed(bound); compress2(packed.data(),&bound,source.data()+at,n,Z_BEST_COMPRESSION); u32(f,static_cast<uint32_t>(bound+8)); std::fwrite("ZL02",1,4,f); u32(f,static_cast<uint32_t>(n)); std::fwrite(packed.data(),1,bound,f); at+=n;} std::fclose(f);
  srhd_awa::package::Package p; std::string e; std::vector<uint8_t> actual; const srhd_awa::package::Entry* entry=nullptr; bool ok=p.Open("build/synthetic.pkg",&e)&&(entry=p.Resolve("FILE"))&&p.ReadPayload(*entry,&actual,&e)&&actual==source; if(!ok)std::fprintf(stderr,"error=%s\n",e.c_str()); std::remove("build/synthetic.pkg"); return ok;
}
int main(){for(size_t n:{size_t(1),size_t(65535),size_t(65536),size_t(65537),size_t(131072),size_t(131079)})if(!run(n)){std::fprintf(stderr,"failed=%zu\n",n);return 1;}std::puts("synthetic ZL02 passed");}
