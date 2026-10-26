// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "legacy_texture_backend.hpp"
#include "texture_residency.hpp"
#include <functional>
#include <memory>
#include <vector>
#include <array>
namespace srhd_awa::matrixgame::native_texture {
inline constexpr platform3g::DeviceResult OutOfMemory=static_cast<std::int32_t>(0x8007000eu);
inline constexpr platform3g::DeviceResult OutOfVideoMemory=static_cast<std::int32_t>(0x8876017cu);
struct Image {virtual ~Image()=default;};
struct Level {
    std::uint32_t width{},height{},pitch{},rows{};
    std::vector<std::byte> bytes;
    bool locked{},read_only{};
};
struct Session;
struct Texture {
    std::shared_ptr<Session> session;
    std::uint32_t format{},pool{},usage{},priority{};
    std::vector<Level> levels;
    std::shared_ptr<Image> image;
    bool dirty{true},render_initialized{};
};
// Borrow fields that contain device-owned shared_ptr references. Never retain
// the resources, allocate, or infer ownership from a raw pointer alone. The
// ledger lives only for one quiescent, single-render-thread Reset query.
class DeviceReferences {
    std::array<const std::shared_ptr<Texture>*,16> fields_{};
    unsigned count_{};bool denied_{};
public:
    void Add(const std::shared_ptr<Texture>& field)noexcept{
        if(!field)return;for(unsigned i=0;i<count_;++i)if(fields_[i]==&field)return;
        if(count_==fields_.size()){denied_=true;return;}fields_[count_++]=&field;
    }
    void Add(std::shared_ptr<Texture>&&)=delete;
    void Deny()noexcept{denied_=true;}
    bool Denied()const noexcept{return denied_;}
    unsigned Count(const std::shared_ptr<Texture>& value)const noexcept{
        unsigned result=0;for(unsigned i=0;i<count_;++i){const auto& field=*fields_[i];
            result+=field.get()==value.get() && !field.owner_before(value) && !value.owner_before(field);}
        return result;
    }
};
struct Session {
    std::function<std::shared_ptr<Image>(const Texture&)> upload;
    std::vector<std::weak_ptr<Texture>> textures;
    bool DefaultResourcesLive()const noexcept{
        for(const auto& weak:textures)if(auto texture=weak.lock();texture && texture->pool==0)return true;
        return false;
    }
    bool ExternalDefaultResourcesLive(const DeviceReferences& device)const noexcept{
        if(device.Denied())return true;
        for(const auto& weak:textures)if(auto texture=weak.lock();texture && texture->pool==0)
            if(texture.use_count()>1+device.Count(texture))return true;
        return false;
    }
    void Invalidate()noexcept;
    void Shutdown()noexcept;
};
class Provider {
public:virtual ~Provider()=default;
    virtual std::shared_ptr<Session> TextureSession()=0;
};
// Registered opaque native tokens travel through the original pointer fields.
// They have NO COM vtable and are never dereferenced as IDirect3DTexture9.
std::shared_ptr<Texture> Find(void*);
std::size_t HandleCount()noexcept;
void Release(void*)noexcept;
void Preload(void*);
std::shared_ptr<Image> Resident(const std::shared_ptr<Texture>&);
// Tight mip rows are required by the deko copyBufferToImage transfer contract.
void ValidateUpload(const Texture&);
std::uint32_t Priority(void*,std::uint32_t);
std::uint32_t Levels(void*);
platform3g::DeviceResult Describe(void*,std::uint32_t,legacy_texture::LevelDesc*);
platform3g::DeviceResult Lock(void*,std::uint32_t,legacy_texture::LockedRect*,const legacy_texture::TextureRect*,std::uint32_t);
platform3g::DeviceResult Unlock(void*,std::uint32_t);
platform3g::DeviceResult Create(std::shared_ptr<Session>,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t,IDirect3DTexture9**);
platform3g::DeviceResult Create(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t,IDirect3DTexture9**);
platform3g::DeviceResult Decode(const void*,std::size_t,std::uint32_t,std::uint32_t,std::uint32_t,IDirect3DTexture9**);
platform3g::DeviceResult Decode(std::shared_ptr<Session>,const void*,std::size_t,std::uint32_t,std::uint32_t,std::uint32_t,IDirect3DTexture9**);
void Update(void*,void*);
}
