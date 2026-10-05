#include "package.hpp"
#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstring>
#include <functional>
#include <limits>
#include <zlib.h>

namespace srhd_awa::package {
namespace { constexpr uint32_t kRecord = 158; constexpr uint32_t kHeader = 12; uint32_t le(const uint8_t* p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;} std::string text(const char* p,size_t n){size_t l=0;while(l<n&&p[l])++l;return std::string(p,l);} std::string upper(std::string s){for(char& c:s)if(c>='a'&&c<='z')c-=32;return s;} int seek64(FILE*f,uint64_t p){
#if defined(_WIN32) || defined(__MINGW32__) || defined(__MINGW64__)
return _fseeki64(f,static_cast<__int64>(p),SEEK_SET);
#elif defined(__MSYS__)
return p > LONG_MAX ? -1 : std::fseek(f, static_cast<long>(p), SEEK_SET);
#else
return fseeko(f,static_cast<off_t>(p),SEEK_SET);
#endif
} uint64_t tell64(FILE*f){
#if defined(_WIN32) || defined(__MINGW32__) || defined(__MINGW64__)
return static_cast<uint64_t>(_ftelli64(f));
#elif defined(__MSYS__)
return static_cast<uint64_t>(std::ftell(f));
#else
return static_cast<uint64_t>(ftello(f));
#endif
} int seek_end(FILE*f){
#if defined(_WIN32) || defined(__MINGW32__) || defined(__MINGW64__)
return _fseeki64(f,0,SEEK_END);
#elif defined(__MSYS__)
return std::fseek(f, 0, SEEK_END);
#else
return fseeko(f,0,SEEK_END);
#endif
} }
bool Package::Open(const std::string& path,std::string* err){ FILE* f=std::fopen(path.c_str(),"rb");if(!f){*err="open failed";return false;} if(seek_end(f)|| (size_=tell64(f))<4){std::fclose(f);*err="size failed";return false;} uint8_t b[4]; seek64(f,0); if(std::fread(b,1,4,f)!=4){std::fclose(f);*err="root read failed";return false;} std::fclose(f); path_=path;root_=std::make_unique<Folder>();return LoadFolder(le(b),root_.get(),0,err); }
bool Package::LoadFolder(uint32_t off,Folder* folder,uint32_t depth,std::string* err){ if(off>size_||size_-off<kHeader){*err="invalid child offset";return false;} FILE*f=std::fopen(path_.c_str(),"rb");if(!f||seek64(f,off)){if(f)std::fclose(f);*err="seek failed";return false;} uint8_t h[12];if(std::fread(h,1,12,f)!=12){std::fclose(f);*err="short header";return false;}auto hs=le(h),count=le(h+4),rs=le(h+8);if(hs<kHeader||hs>size_-off||rs!=kRecord||count>(size_-off-kHeader)/rs){std::fclose(f);*err="invalid header";return false;} std::vector<uint8_t> raw(size_t(count)*rs);if(!raw.empty()&&std::fread(raw.data(),1,raw.size(),f)!=raw.size()){std::fclose(f);*err="short records";return false;}std::fclose(f);folder->entries.resize(count);for(uint32_t i=0;i<count;++i){auto*p=raw.data()+size_t(i)*rs;auto&e=folder->entries[i];e.stored_size=le(p);e.data_size=le(p+4);e.upper_name=text((char*)p+8,63);e.original_name=text((char*)p+71,63);e.kind=(int32_t)le(p+134);e.kind_copy=e.kind;e.flags=le(p+142);e.target_offset=le(p+150);}for(auto&e:folder->entries)if(e.kind==3&&e.flags==0){e.child=std::make_unique<Folder>();if(!LoadFolder(e.target_offset,e.child.get(),depth+1,err))return false;}return true; }
const Entry* Package::Resolve(const std::string& path)const{const Folder*f=root_.get();size_t start=0;while(f){auto pos=path.find_first_of("/\\",start);auto part=upper(path.substr(start,pos==std::string::npos?pos:pos-start));const Entry*found=nullptr;for(auto&e:f->entries)if(!e.flags&&e.upper_name==part){found=&e;break;}if(!found)return nullptr;if(pos==std::string::npos)return found;if(found->kind!=3||!found->child)return nullptr;f=found->child.get();start=pos+1;}return nullptr;}
uint32_t Package::OpenEntryByPath(const std::string& path,std::string*err){const Entry*entry=Resolve(path);if(!entry||entry->kind==3){*err="entry not found";return 0;}for(size_t i=0;i<open_entries_.size();++i)if(!open_entries_[i].open){open_entries_[i]={entry,0,true};return static_cast<uint32_t>(i+1);}if(open_entries_.size()>=16){*err="package open-slot limit";return 0;}open_entries_.push_back({entry,0,true});return static_cast<uint32_t>(open_entries_.size());}
bool Package::SeekEntry(uint32_t handle,uint64_t position,std::string*err){if(!handle||handle>open_entries_.size()||!open_entries_[handle-1].open||position>open_entries_[handle-1].entry->data_size){*err="invalid package handle";return false;}open_entries_[handle-1].position=position;return true;}
bool Package::SeekEntry(uint32_t handle,uint32_t offset,uint32_t origin,std::string*err){if(!handle||handle>open_entries_.size()||!open_entries_[handle-1].open){*err="invalid package handle";return false;}const auto&open=open_entries_[handle-1];uint64_t position=0;if(origin==0)position=offset;else if(origin==1)position=open.position+offset;else if(origin==2){if(offset>open.entry->data_size){*err="invalid package seek";return false;}position=open.entry->data_size-offset;}else{*err="invalid package seek origin";return false;}return SeekEntry(handle,position,err);}
size_t Package::ReadEntry(uint32_t handle,void*dst,size_t bytes,std::string*err){if(!handle||handle>open_entries_.size()||!open_entries_[handle-1].open){*err="invalid package handle";return 0;}auto& open=open_entries_[handle-1];std::vector<uint8_t> payload;if(!ReadPayload(*open.entry,&payload,err))return 0;const size_t available=static_cast<size_t>(open.entry->data_size-open.position);const size_t count=std::min(bytes,available);std::memcpy(dst,payload.data()+open.position,count);open.position+=count;return count;}
uint64_t Package::GetEntryPosition(uint32_t handle)const{return handle&&handle<=open_entries_.size()&&open_entries_[handle-1].open?open_entries_[handle-1].position:UINT64_MAX;}
uint64_t Package::GetEntrySize(uint32_t handle)const{return handle&&handle<=open_entries_.size()&&open_entries_[handle-1].open?open_entries_[handle-1].entry->data_size:UINT64_MAX;}
void Package::CloseEntry(uint32_t handle){if(handle&&handle<=open_entries_.size())open_entries_[handle-1].open=false;}
bool Package::ReadPayload(const Entry&e,std::vector<uint8_t>*out,std::string*err){uint64_t at=uint64_t(e.target_offset)+4;if(at>size_){*err="invalid payload offset";return false;}FILE*f=std::fopen(path_.c_str(),"rb");if(!f||seek64(f,at)){if(f)std::fclose(f);*err="payload seek failed";return false;}out->resize(e.data_size);if(e.kind!=2){bool ok=e.data_size<=size_-at&&(!e.data_size||std::fread(out->data(),1,out->size(),f)==out->size());std::fclose(f);if(!ok)*err="payload read failed";return ok;}
uint64_t remaining=e.data_size;size_t written=0;while(remaining){uint8_t size_bytes[4];if(std::fread(size_bytes,1,4,f)!=4){std::fclose(f);*err="short compressed block size";return false;}const uint32_t stored=le(size_bytes);if(stored<8||stored>72112||stored>size_){std::fclose(f);*err="invalid compressed block size";return false;}std::vector<uint8_t> packed(stored),decoded(65536);if(std::fread(packed.data(),1,stored,f)!=stored||std::memcmp(packed.data(),"ZL02",4)!=0){std::fclose(f);*err="invalid compressed block header";return false;}const size_t need=static_cast<size_t>(std::min<uint64_t>(remaining,decoded.size()));if(le(packed.data()+4)!=need){std::fclose(f);*err="unexpected compressed block size";return false;}uLongf decoded_size=decoded.size();if(uncompress(decoded.data(),&decoded_size,packed.data()+8,stored-8)!=Z_OK||decoded_size!=need){std::fclose(f);*err="zlib block decode failed";return false;}std::memcpy(out->data()+written,decoded.data(),need);written+=need;remaining-=need;}std::fclose(f);return true;
}
Summary Package::Summarize()const{Summary s;s.tree_hash=UINT64_C(1469598103934665603);std::function<void(const Folder&,std::string,uint32_t)> walk=[&](const Folder&f,std::string p,uint32_t d){++s.folders;s.max_depth=std::max(s.max_depth,d);for(auto&e:f.entries){++s.entries;auto q=p+(p.empty()?"":"/")+e.original_name;if(e.kind==3&&e.child)walk(*e.child,q,d+1);else{++s.files;s.paths.push_back(q+"|"+std::to_string(e.kind)+"|"+std::to_string(e.data_size)+"|"+std::to_string(e.target_offset));}}};walk(*root_,"",0);std::sort(s.paths.begin(),s.paths.end());for(const auto&path:s.paths){for(unsigned char c:path){s.tree_hash^=c;s.tree_hash*=UINT64_C(1099511628211);}s.tree_hash^='\n';s.tree_hash*=UINT64_C(1099511628211);}return s;}
}
