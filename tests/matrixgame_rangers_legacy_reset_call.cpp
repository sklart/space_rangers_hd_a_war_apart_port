// SPDX-License-Identifier: GPL-2.0-or-later
#include "units/Direct3D9.hpp"
#include "rangers_legacy_presentation.hpp"
#include <cstring>
// Exercise the generated ORIGINAL inline entry, not merely a parallel test API.
std::int32_t CallOriginalRangersReset(void* device,void* bytes){
 return Direct3D9::IDirect3DDevice9_Reset(pas::ComView<Direct3D9::IDirect3DDevice9_Tag>(device),bytes);
}
static_assert(sizeof(Direct3D9::TD3DPresentParameters)==56);
static_assert(sizeof(Direct3D9::TD3DPresentParameters)==sizeof(srhd_awa::matrixgame::RangersLegacyPresentation));
#define SAME_FIELD(name) static_assert(offsetof(Direct3D9::TD3DPresentParameters,name)==offsetof(srhd_awa::matrixgame::RangersLegacyPresentation,name))
SAME_FIELD(BackBufferWidth);SAME_FIELD(BackBufferHeight);SAME_FIELD(BackBufferFormat);SAME_FIELD(BackBufferCount);
SAME_FIELD(MultiSampleType);SAME_FIELD(MultiSampleQuality);SAME_FIELD(SwapEffect);SAME_FIELD(DeviceWindow);
SAME_FIELD(Windowed);SAME_FIELD(EnableAutoDepthStencil);SAME_FIELD(AutoDepthStencilFormat);SAME_FIELD(Flags);
SAME_FIELD(FullScreenRefreshRateInHz);SAME_FIELD(PresentationInterval);
#undef SAME_FIELD
