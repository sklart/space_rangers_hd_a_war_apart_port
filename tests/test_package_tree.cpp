#include "package.hpp"
#include <cstdio>
int main(int argc,char**argv){if(argc!=2)return 2;srhd_awa::package::Package p;std::string e;if(!p.Open(std::string(argv[1])+"/DATA/common.pkg",&e)){std::fprintf(stderr,"%s\n",e.c_str());return 1;}auto s=p.Summarize();if(!s.entries||!s.files||!p.Resolve("data")){std::fprintf(stderr,"tree invalid\n");return 1;}std::printf("folders=%u files=%u entries=%u depth=%u\n",s.folders,s.files,s.entries,s.max_depth);return 0;}
