// SPDX-License-Identifier: GPL-2.0-or-later
#include "texture_request.hpp"
#include "CBlockPar.hpp"
#include "CFile.hpp"
#include "bitmap_limits.hpp"
#include <cstring>
#include <stdexcept>
namespace srhd_awa::matrixgame {
std::wstring ReplaceTextureFileExt(std::wstring_view name,std::wstring_view ext) {
  // Windows path separators on every host; do not convert UTF-16 via host locale.
  const auto query=name.find(L'?');auto file=std::wstring(name.substr(0,query));
  const auto slash=file.find_last_of(L"/\\"),dot=file.find_last_of(L'.');
  const auto first=slash==file.npos?0:slash+1;
  if(dot!=file.npos&&dot>first)file.resize(dot);
  if(!ext.empty()&&ext.front()!=L'.')file+=L'.';file+=ext;
  if(query!=name.npos)file+=name.substr(query);
  return file;
}
TextureRequest TextureRequest::Resolve(std::wstring_view value) {
  if(value.empty()||value.size()>32768||value.find(L'\0')!=value.npos)throw std::invalid_argument("texture request name");
  Base::ParamParser name{std::wstring(value)};TextureRequest result;
  result.name=value;result.flags=ParseOriginalTextureFlags(name);
  const auto plain=name.GetStrPar(0,L"?");
  if(plain.empty()||!Base::CFile::FileExist(result.path,plain.c_str(),L"png~jpg~bmp~dds~strg",false))
    throw std::runtime_error("MatrixGame texture file not found");
  return result;
}
TextureAsset TextureRequest::Load() const {
  if(path.empty()||path.find(L'\0')!=path.npos)throw std::invalid_argument("texture request path");
  Base::CFile file{path};file.OpenRead();const auto size=file.Size();
  if(size<=0||std::uint64_t(size)>::matrixgame::native::PngInputLimit)throw std::length_error("texture request input budget");
  std::vector<std::byte> bytes(std::size_t(size),std::byte{});file.Read(bytes.data(),bytes.size());
  const bool storage=bytes.size()>=4&&!std::memcmp(bytes.data(),"STRG",4);
  if(storage!=bool(flags&TF_COMPRESSED))throw std::invalid_argument("texture Compressed flag/payload mismatch");
  return TextureAsset::Decode(bytes);
}
}
