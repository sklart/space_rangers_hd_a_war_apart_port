"""Preserve the original S3D_Default body through actual typed device operations."""
import re
from matrixgame_vector_object_overrides import function
def patch_device_defaults(files):
    body=function(files['3g.cpp'].decode(),'void S3D_Default()')
    body=body.replace('void S3D_Default() {','void S3D_Default() {\n    auto& device=platform3g::ReadyDevice();')
    body=body.replace('g_D3DD->SetTransform(D3DTS_WORLD, &mWorld)','device.SetWorldTransform(std::bit_cast<platform3g::WorldMatrix>(mWorld))')
    body=body.replace('g_D3DD->SetTexture(i, NULL)','device.ClearTexture(i)')
    body=body.replace('g_D3DD->','device.')
    body=body.replace('*((LPDWORD)(&fBias))','std::bit_cast<std::uint32_t>(fBias)')
    names={
        'D3DSAMP_MAGFILTER':'SamplerRegister::MagFilter','D3DSAMP_MINFILTER':'SamplerRegister::MinFilter','D3DSAMP_MIPFILTER':'SamplerRegister::MipFilter',
        'D3DSAMP_ADDRESSU':'SamplerRegister::AddressU','D3DSAMP_ADDRESSV':'SamplerRegister::AddressV','D3DSAMP_MIPMAPLODBIAS':'SamplerRegister::LodBias',
        'D3DTSS_TEXCOORDINDEX':'platform3g::TextureStageRegister::TexcoordIndex',
        'D3DRS_SHADEMODE':'RasterRegister::ShadeMode','D3DRS_ALPHABLENDENABLE':'RasterRegister::BlendEnable','D3DRS_SRCBLEND':'RasterRegister::SourceBlend',
        'D3DRS_DESTBLEND':'RasterRegister::DestinationBlend','D3DRS_ALPHATESTENABLE':'RasterRegister::AlphaTest','D3DRS_ALPHAREF':'RasterRegister::AlphaReference',
        'D3DRS_ALPHAFUNC':'RasterRegister::AlphaCompare','D3DRS_CULLMODE':'RasterRegister::CullMode','D3DRS_LIGHTING':'RasterRegister::Lighting',
        'D3DRS_ZENABLE':'RasterRegister::DepthTest','D3DRS_ZWRITEENABLE':'RasterRegister::DepthWrite','D3DRS_ZFUNC':'RasterRegister::DepthCompare',
        'D3DTEXF_LINEAR':'static_cast<std::uint32_t>(TextureFilter::Linear)','D3DTADDRESS_WRAP':'static_cast<std::uint32_t>(TextureAddress::Repeat)',
        'D3DSHADE_GOURAUD':'2u','D3DBLEND_SRCALPHA':'static_cast<std::uint32_t>(RasterBlendFactor::SourceAlpha)',
        'D3DBLEND_INVSRCALPHA':'static_cast<std::uint32_t>(RasterBlendFactor::InverseSourceAlpha)',
        'D3DCMP_GREATEREQUAL':'static_cast<std::uint32_t>(RasterCompare::GreaterEqual)','D3DCMP_LESSEQUAL':'static_cast<std::uint32_t>(RasterCompare::LessEqual)',
        'D3DCULL_CCW':'3u','D3DZB_TRUE':'1u','TRUE':'1u','FALSE':'0u',
    }
    for old,new in names.items():body=re.sub(r'\b'+old+r'\b',lambda m:new,body)
    files['3GDeviceDefaults.cpp']=('''// Original 3g.cpp S3D_Default; GPL-2.0-or-later. Not the full TU.
#include "device_defaults.hpp"
#include "platform_3g.hpp"
#include "MatrixSampleStateManager.hpp"
#include "math_types.hpp"
#include <bit>
using namespace srhd_awa::matrixgame;
#define ASSERT_DX(call) platform3g::CheckDeviceResult((call),#call)
'''+body+'\n#undef ASSERT_DX\n').encode()
