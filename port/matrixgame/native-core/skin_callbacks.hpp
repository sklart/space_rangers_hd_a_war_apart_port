// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "buffer_geometry.hpp"
#include <memory>
struct SSkin;
namespace srhd_awa::matrixgame {
// Typed native boundaries, not D3D interfaces. Original SSkin remains unchanged.
struct SkinProvider {
    virtual ~SkinProvider()=default;
    virtual std::shared_ptr<const SSkin> AcquireSkin(const wchar_t*,std::uint32_t)=0;
};
class SkinLoadScope {
public:
    explicit SkinLoadScope(SkinProvider&);
    ~SkinLoadScope();
    SkinLoadScope(const SkinLoadScope&)=delete;
    SkinLoadScope& operator=(const SkinLoadScope&)=delete;
private:
    friend const SSkin* AcquireOwnedSkin(const wchar_t*,std::uint32_t);
    friend std::shared_ptr<const SSkin> TakeOwnedSkin(const SSkin*);
    SkinLoadScope* previous_;
    SkinProvider& provider_;
    std::shared_ptr<const SSkin> pending_;
};
const SSkin* AcquireOwnedSkin(const wchar_t*,std::uint32_t);
std::shared_ptr<const SSkin> TakeOwnedSkin(const SSkin*);
bool SkinProviderBound() noexcept;
struct NativeDrawSink {
    virtual ~NativeDrawSink()=default;
    virtual void SetVertexLayout(std::uint32_t)=0;
    virtual void Emit(const MeshRange&)=0;
};
class NativeDrawScope {
public:
    explicit NativeDrawScope(NativeDrawSink&);
    ~NativeDrawScope();
    NativeDrawScope(const NativeDrawScope&)=delete;
    NativeDrawScope& operator=(const NativeDrawScope&)=delete;
private:
    NativeDrawSink* previous_;
};
NativeDrawSink& CurrentNativeDrawSink();
NativeDrawSink* InspectNativeDrawSink() noexcept;
}
