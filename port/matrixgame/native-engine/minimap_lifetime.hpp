// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "legacy_texture_backend.hpp"
#include <cstdint>
namespace srhd_awa::matrixgame::minimap_lifetime {
class TextureOwner {
 IDirect3DTexture9* value_{};
public:
 TextureOwner()=default;TextureOwner(const TextureOwner&)=delete;
 ~TextureOwner(){legacy_texture::Release(value_);}
 operator IDirect3DTexture9*()const noexcept{return value_;}
 IDirect3DTexture9** put(){if(value_)throw std::logic_error("minimap texture output already owned");return &value_;}
 void detach()noexcept{value_=nullptr;}
 void reset()noexcept{legacy_texture::Release(value_);value_=nullptr;}
};
class TextureSlotRollback {
 IDirect3DTexture9*& value_;bool committed_{};
public:
 explicit TextureSlotRollback(IDirect3DTexture9*& value):value_(value){if(value)throw std::logic_error("minimap staging slot already owned");}
 TextureSlotRollback(const TextureSlotRollback&)=delete;
 ~TextureSlotRollback(){if(!committed_){legacy_texture::Release(value_);value_=nullptr;}}
 void Commit()noexcept{committed_=true;}
};
class FlagRollback {
 std::uint32_t& value_;std::uint32_t mask_,before_;bool committed_{};
public:
 FlagRollback(std::uint32_t& value,std::uint32_t mask):value_(value),mask_(mask),before_(value&mask){}
 FlagRollback(const FlagRollback&)=delete;
 ~FlagRollback(){if(!committed_)value_=(value_&~mask_)|before_;}
 void Commit()noexcept{committed_=true;}
};
template<class T>class ValueRollback {
 T& value_;T before_;bool committed_{};
public:
 explicit ValueRollback(T& value):value_(value),before_(value){}
 ValueRollback(const ValueRollback&)=delete;
 ~ValueRollback(){if(!committed_)value_=before_;}
 void Commit()noexcept{committed_=true;}
};
}
