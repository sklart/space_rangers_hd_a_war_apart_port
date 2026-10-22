// SPDX-License-Identifier: GPL-2.0-or-later
#include "cached_frame.hpp"
namespace srhd_awa::matrixgame {
CachedFrame PrepareCachedFrame(NativeInitRuntime& runtime,CVectorObject& object,int frame,
    std::span<const CachedFrameMaterial> materials) {
    if(!runtime.CpuReady() || object.m_Cache!=g_Cache)
        throw std::logic_error("VO has no active native cache owner");
    object.Prepare();
    if(materials.size()!=std::size_t(object.NativeSurfaceCount()))
        throw std::invalid_argument("explicit VO surface materials required");
    CachedFrame result;
    for(const auto& material:materials) {
        material.state.raster.Validate();material.state.sampling.Validate();
        if(material.state.texture_slot==0 || material.state.texture_slot>=64)
            throw std::invalid_argument("explicit cached texture slot required");
        // Conflicting textures for one slot must never silently use the last.
        auto image=material.texture.Read();
        for(std::size_t i=0;i<result.textures.size();++i)
            if(materials[i].state.texture_slot==material.state.texture_slot && result.textures[i]!=image)
                throw std::invalid_argument("conflicting cached texture slot");
        result.textures.push_back(std::move(image));
    }
    for(const auto& range:object.PrepareNativeFrame(frame)) {
        if(range.surface<0 || std::size_t(range.surface)>=materials.size())
            throw std::out_of_range("VO surface index");
        const auto& state=materials[range.surface].state;
        result.submissions.push_back({range,state.draw,state.texture_slot,state.sampling,state.raster});
    }
    std::vector<MeshRange> ranges;for(const auto& s:result.submissions)ranges.push_back(s.range);
    (void)buffers::ValidateBoundVO(buffers::Current(),ranges);
    return result;
}
}
