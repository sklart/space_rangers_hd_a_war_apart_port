#include "package.hpp"
#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <zlib.h>
#ifdef _WIN32
#include <filesystem>
#endif

namespace srhd_awa::package {
namespace { struct FileCloser { void operator()(FILE* f) const noexcept { if (f) std::fclose(f); }  };
FILE* open_read(const std::string& path, bool path_is_utf8) {
#ifdef _WIN32
  if (!path_is_utf8) return std::fopen(path.c_str(), "rb");
  std::u8string utf8(path.size(), char8_t{});
  std::memcpy(utf8.data(), path.data(), path.size());
  try { return _wfopen(std::filesystem::path(utf8).c_str(), L"rb"); }
  catch (const std::filesystem::filesystem_error&) { return nullptr; }
#else
  (void)path_is_utf8;
  return std::fopen(path.c_str(), "rb");
#endif
}
constexpr uint32_t kRecord = 158; constexpr uint32_t kHeader = 12; constexpr uint32_t kMaxFolderEntries = 1u << 20; constexpr uint32_t kMaxPayloadSize = 256u << 20; uint32_t le(const uint8_t* p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;} std::string text(const char* p,size_t n){size_t l=0;while(l<n&&p[l])++l;return std::string(p,l);} std::string upper(std::string s){for(char& c:s)if(c>='a'&&c<='z')c-=32;return s;} int seek64(FILE*f,uint64_t p){
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
bool Package::Open(const std::string& path, std::string* err) { return OpenPath(path, false, err); }
bool Package::OpenUtf8(const std::string& path, std::string* err) { return OpenPath(path, true, err); }
bool Package::OpenPath(const std::string& path, bool utf8, std::string* err){ open_entries_.clear(); root_.reset(); FILE* f=open_read(path, utf8);std::unique_ptr<FILE, FileCloser> file(f);if(!f){*err="open failed";return false;} if(seek_end(f)|| (size_=tell64(f))<4){file.reset();*err="size failed";return false;} uint8_t b[4]; seek64(f,0); if(std::fread(b,1,4,f)!=4){file.reset();*err="root read failed";return false;} file.reset(); path_=path;utf8_path_=utf8;root_=std::make_unique<Folder>();std::vector<uint32_t> active;return LoadFolder(le(b),root_.get(),0,active,err); }
bool Package::LoadFolder(uint32_t off,Folder* folder,uint32_t depth,std::vector<uint32_t>& active,std::string* err){
 if(std::find(active.begin(),active.end(),off)!=active.end()){*err="cyclic folder reference";return false;} active.push_back(off); struct Guard{std::vector<uint32_t>&v;~Guard(){v.pop_back();}} guard{active};
 if(off>size_||size_-off<kHeader){*err="invalid child offset";return false;} FILE*f=open_read(path_, utf8_path_);std::unique_ptr<FILE, FileCloser> file(f);if(!f||seek64(f,off)){if(f)file.reset();*err="seek failed";return false;} uint8_t h[12];if(std::fread(h,1,12,f)!=12){file.reset();*err="short header";return false;}auto hs=le(h),count=le(h+4),rs=le(h+8);if(hs<kHeader||hs>size_-off||rs!=kRecord||count>kMaxFolderEntries||count>(size_-off-kHeader)/rs){file.reset();*err="invalid header";return false;} std::vector<uint8_t> raw(size_t(count)*rs);if(!raw.empty()&&std::fread(raw.data(),1,raw.size(),f)!=raw.size()){file.reset();*err="short records";return false;}file.reset();folder->entries.resize(count);for(uint32_t i=0;i<count;++i){auto*p=raw.data()+size_t(i)*rs;auto&e=folder->entries[i];e.stored_size=le(p);e.data_size=le(p+4);e.upper_name=text((char*)p+8,63);e.original_name=text((char*)p+71,63);e.kind=(int32_t)le(p+134);e.kind_copy=e.kind;e.flags=le(p+142);e.target_offset=le(p+150);}for(auto&e:folder->entries)if(e.kind==3&&e.flags==0){e.child=std::make_unique<Folder>();if(!LoadFolder(e.target_offset,e.child.get(),depth+1,active,err))return false;}return true; }
const Entry* Package::Resolve(const std::string& path)const{const Folder*f=root_.get();size_t start=0;while(f){auto pos=path.find_first_of("/\\",start);auto part=upper(path.substr(start,pos==std::string::npos?pos:pos-start));const Entry*found=nullptr;for(auto&e:f->entries)if(!e.flags&&e.upper_name==part){found=&e;break;}if(!found)return nullptr;if(pos==std::string::npos)return found;if(found->kind!=3||!found->child)return nullptr;f=found->child.get();start=pos+1;}return nullptr;}
uint32_t Package::OpenEntryByPath(const std::string& path, std::string* err) {
  const Entry* entry = Resolve(path);
  if (!entry || entry->kind == 3) { *err = "entry not found"; return 0; }
  if (entry->data_size > kMaxPayloadSize || uint64_t(entry->target_offset) + 4 > size_) {
    *err = "invalid payload size"; return 0;
  }
  OpenEntry candidate;
  candidate.entry = entry;
  candidate.open = true;
  candidate.block_offset = uint64_t(entry->target_offset) + 4;
  candidate.block_anchors[0] = candidate.block_offset;
  const auto blocks = (uint64_t(entry->data_size) + 65535) / 65536;
  candidate.anchor_stride = std::max<uint32_t>(1, (blocks + 63) / 64);
  candidate.file.reset(open_read(path_, utf8_path_));
  if (!candidate.file) { *err = "payload open failed"; return 0; }
  for (size_t i = 0; i < open_entries_.size(); ++i) {
    if (!open_entries_[i].open) {
      open_entries_[i] = std::move(candidate);
      return static_cast<uint32_t>(i + 1);
    }
  }
  if (open_entries_.size() >= 16) { *err = "package open-slot limit"; return 0; }
  open_entries_.push_back(std::move(candidate));
  return static_cast<uint32_t>(open_entries_.size());
}
bool Package::SeekEntry(uint32_t handle,uint64_t position,std::string*err){if(!handle||handle>open_entries_.size()||!open_entries_[handle-1].open||position>open_entries_[handle-1].entry->data_size){*err="invalid package handle";return false;}open_entries_[handle-1].position=position;return true;}
bool Package::SeekEntry(uint32_t handle,uint32_t offset,uint32_t origin,std::string*err){if(!handle||handle>open_entries_.size()||!open_entries_[handle-1].open){*err="invalid package handle";return false;}const auto&open=open_entries_[handle-1];uint64_t position=0;if(origin==0)position=offset;else if(origin==1)position=open.position+offset;else if(origin==2){if(offset>open.entry->data_size){*err="invalid package seek";return false;}position=open.entry->data_size-offset;}else{*err="invalid package seek origin";return false;}return SeekEntry(handle,position,err);}
size_t Package::ReadAt(OpenEntry& open, uint64_t offset, void* destination, size_t bytes) {
  FILE* file = open.file.get();
  if (std::ferror(file) || std::feof(file)) {
    std::clearerr(file);
    open.file_position_valid = false;
  }
  if (!open.file_position_valid || open.file_position != offset) {
    if (seek64(file, offset)) { open.file_position_valid = false; return 0; }
    open.file_position = offset;
    open.file_position_valid = true;
  }
  const auto read = std::fread(destination, 1, bytes, file);
  open.file_position += read;
  if (read != bytes) open.file_position_valid = false;
  return read;
}
bool Package::LoadReadBlock(OpenEntry& open, uint32_t index, std::string* err) {
  constexpr uint32_t block_size = 65536;
  if (open.block_cached && index == open.block_index) return true;
  if (open.block_cached && index > open.block_index) {
    // The next header immediately follows the cached packed block. Its size
    // is already known; neither seek nor re-reading its header is necessary.
    open.block_offset += 4 + open.packed.size();
    ++open.block_index;
  }
  if (index < open.block_index) {
    open.block_index = 0;
    open.block_offset = uint64_t(open.entry->target_offset) + 4;
  }
  if (open.block_index != index) {
    for (size_t slot = index / open.anchor_stride + 1; slot > 0; --slot) {
      const auto anchor = static_cast<uint32_t>(slot - 1) * open.anchor_stride;
      if (anchor <= open.block_index) break;
      if (open.block_anchors[slot - 1]) {
        open.block_index = anchor;
        open.block_offset = open.block_anchors[slot - 1];
        break;
      }
    }
  }
  open.block_cached = false;
  while (true) {
    const auto offset = open.block_offset;
    uint8_t size_bytes[4];
    if (offset > size_ || size_ - offset < 4 ||
        ReadAt(open, offset, size_bytes, 4) != 4) {
      *err = "short compressed block size"; return false;
    }
    const auto stored = le(size_bytes);
    if (stored < 8 || stored > 72112 || stored > size_ - offset - 4) {
      *err = "invalid compressed block size"; return false;
    }
    if (open.block_index % open.anchor_stride == 0)
      open.block_anchors[open.block_index / open.anchor_stride] = offset;
    if (open.block_index < index) {
      open.block_offset += 4 + stored;
      ++open.block_index;
      continue;
    }
    // Reserve the format maximum once; vector growth must not keep replacing
    // increasingly large buffers during a music stream.
    if (open.packed.capacity() < 72112) open.packed.reserve(72112);
    open.packed.resize(stored);
    if (ReadAt(open, offset + 4, open.packed.data(), stored) != stored ||
        std::memcmp(open.packed.data(), "ZL02", 4) != 0) {
      *err = "invalid compressed block header"; return false;
    }
    const size_t need = std::min<uint64_t>(block_size,
        open.entry->data_size - uint64_t(index) * block_size);
    if (le(open.packed.data() + 4) != need) {
      *err = "unexpected compressed block size"; return false;
    }
    open.decoded.resize(block_size);
    uLongf decoded_size = block_size;
    if (uncompress(open.decoded.data(), &decoded_size, open.packed.data() + 8,
                   stored - 8) != Z_OK || decoded_size != need) {
      *err = "zlib block decode failed"; return false;
    }
    open.block_cached = true;
    return true;
  }
}

size_t Package::ReadEntry(uint32_t handle, void* dst, size_t bytes, std::string* err) {
  if (!handle || handle > open_entries_.size() || !open_entries_[handle - 1].open) {
    *err = "invalid package handle"; return 0;
  }
  auto& open = open_entries_[handle - 1];
  const auto count = std::min<uint64_t>(bytes, open.entry->data_size - open.position);
  if (!count) return 0;
  if (!dst) { *err = "null package destination"; return 0; }
  if (open.entry->kind != 2) {
    const auto offset = uint64_t(open.entry->target_offset) + 4 + open.position;
    if (offset > size_ || count > size_ - offset ||
        ReadAt(open, offset, dst, count) != count) {
      *err = "payload read failed"; return 0;
    }
  } else {
    size_t copied = 0;
    while (copied < count) {
      const auto position = open.position + copied;
      if (!LoadReadBlock(open, static_cast<uint32_t>(position / 65536), err)) return 0;
      const size_t within = position % 65536;
      const size_t chunk = std::min<size_t>(count - copied, 65536 - within);
      std::memcpy(static_cast<uint8_t*>(dst) + copied, open.decoded.data() + within, chunk);
      copied += chunk;
    }
  }
  open.position += count;
  return count;
}
uint64_t Package::GetEntryPosition(uint32_t handle)const{return handle&&handle<=open_entries_.size()&&open_entries_[handle-1].open?open_entries_[handle-1].position:UINT64_MAX;}
uint64_t Package::GetEntrySize(uint32_t handle)const{return handle&&handle<=open_entries_.size()&&open_entries_[handle-1].open?open_entries_[handle-1].entry->data_size:UINT64_MAX;}
void Package::CloseEntry(uint32_t handle){if(handle&&handle<=open_entries_.size())open_entries_[handle-1]=OpenEntry{};}
bool Package::ReadPayload(const Entry&e,std::vector<uint8_t>*out,std::string*err){uint64_t at=uint64_t(e.target_offset)+4;if(at>size_||e.data_size>kMaxPayloadSize){*err="invalid payload size";return false;}FILE*f=open_read(path_, utf8_path_);std::unique_ptr<FILE, FileCloser> file(f);if(!f||seek64(f,at)){if(f)file.reset();*err="payload seek failed";return false;}if(e.kind!=2){if(e.data_size>size_-at){file.reset();*err="payload read failed";return false;}out->resize(e.data_size);bool ok=!e.data_size||std::fread(out->data(),1,out->size(),f)==out->size();file.reset();if(!ok)*err="payload read failed";return ok;}
out->resize(e.data_size);uint64_t remaining=e.data_size,cursor=at;size_t written=0;std::vector<uint8_t> packed,decoded(65536);while(remaining){if(cursor>size_||size_-cursor<4){file.reset();*err="short compressed block size";return false;}uint8_t size_bytes[4];if(std::fread(size_bytes,1,4,f)!=4){file.reset();*err="short compressed block size";return false;}cursor+=4;const uint32_t stored=le(size_bytes);if(stored<8||stored>72112||stored>size_-cursor){file.reset();*err="invalid compressed block size";return false;}packed.resize(stored);if(std::fread(packed.data(),1,stored,f)!=stored||std::memcmp(packed.data(),"ZL02",4)!=0){file.reset();*err="invalid compressed block header";return false;}cursor+=stored;const size_t need=static_cast<size_t>(std::min<uint64_t>(remaining,decoded.size()));if(le(packed.data()+4)!=need){file.reset();*err="unexpected compressed block size";return false;}uLongf decoded_size=decoded.size();if(uncompress(decoded.data(),&decoded_size,packed.data()+8,stored-8)!=Z_OK||decoded_size!=need){file.reset();*err="zlib block decode failed";return false;}std::memcpy(out->data()+written,decoded.data(),need);written+=need;remaining-=need;}file.reset();return true;
}
Summary Package::Summarize()const{Summary s;s.tree_hash=UINT64_C(1469598103934665603);std::function<void(const Folder&,std::string,uint32_t)> walk=[&](const Folder&f,std::string p,uint32_t d){++s.folders;s.max_depth=std::max(s.max_depth,d);for(auto&e:f.entries){++s.entries;auto q=p+(p.empty()?"":"/")+e.original_name;if(e.kind==3&&e.child)walk(*e.child,q,d+1);else{++s.files;s.paths.push_back(q+"|"+std::to_string(e.kind)+"|"+std::to_string(e.data_size)+"|"+std::to_string(e.target_offset));}}};walk(*root_,"",0);std::sort(s.paths.begin(),s.paths.end());for(const auto&path:s.paths){for(unsigned char c:path){s.tree_hash^=c;s.tree_hash*=UINT64_C(1099511628211);}s.tree_hash^='\n';s.tree_hash*=UINT64_C(1099511628211);}return s;}
}
