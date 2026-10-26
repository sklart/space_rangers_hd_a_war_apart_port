// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <deko3d.hpp>
#include <span>
#include "../native-engine/native_texture.hpp"
#include "../native-engine/texture_residency.hpp"
namespace srhd_awa::matrixgame::native_texture {
// Owns real image-capable backing, shared by all live image versions.
class DekoTextureStore {
public:
    struct Arena;
    struct ResidentImage final:Image {
        std::shared_ptr<Arena> arena;
        Residency::Range range{};
        dk::Image image;
        dk::ImageDescriptor descriptor;
        ~ResidentImage();
    };
    DekoTextureStore(DkDevice,DkQueue,std::uint32_t reserved_bytes);
    std::shared_ptr<Image> Upload(const Texture&);
    std::shared_ptr<ResidentImage> AllocateTarget(std::uint32_t,std::uint32_t,std::uint32_t);
    std::vector<std::byte> ReadImage(const dk::Image&,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t level=0);
    void WriteImage(const dk::Image&,std::uint32_t,std::uint32_t,std::uint32_t,std::span<const std::byte>,std::uint32_t level=0);
    std::uint64_t Available()const noexcept;
    std::uint64_t Free()const noexcept;
    std::uint64_t Capacity()const noexcept;
private:
    DkDevice device_;DkQueue queue_;
    std::shared_ptr<Arena> arena_;
    dk::UniqueCmdBuf commands_;
    dk::UniqueMemBlock command_memory_;
};
}
