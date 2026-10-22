"""Port the complete original sampler-manager TU to the real typed device boundary."""
import re
INPUTS={
    'MatrixSampleStateManager.cpp':'6d2562c1ba84c166023efbdff1b16b92a7e0904bd5a22474d17037f8702b9b1e',
    'MatrixSampleStateManager.hpp':'123cac7ee5a6ad506e17a7dbb0a1694012184a362340f89eb735273b6c502df7',
}
SUBDIRS={name:'../../MatrixGame/src' for name in INPUTS}
def patch_engine_sampler(files):
    names={
        'HRESULT':'srhd_awa::matrixgame::platform3g::DeviceResult',
        'DWORD':'std::uint32_t',
        'D3DSAMPLERSTATETYPE':'srhd_awa::matrixgame::SamplerRegister',
        'D3DSAMP_MAGFILTER':'srhd_awa::matrixgame::SamplerRegister::MagFilter',
        'D3DSAMP_MINFILTER':'srhd_awa::matrixgame::SamplerRegister::MinFilter',
        'D3DSAMP_MIPFILTER':'srhd_awa::matrixgame::SamplerRegister::MipFilter',
        'D3DSAMP_MAXANISOTROPY':'srhd_awa::matrixgame::SamplerRegister::MaxAnisotropy',
        'D3DTEXF_LINEAR':'static_cast<std::uint32_t>(srhd_awa::matrixgame::TextureFilter::Linear)',
        'D3DTEXF_ANISOTROPIC':'static_cast<std::uint32_t>(srhd_awa::matrixgame::TextureFilter::Anisotropic)',
        'D3D_OK':'srhd_awa::matrixgame::platform3g::DeviceSuccess',
    }
    for name in INPUTS:
        text=files[name].decode()
        text=text.replace('#include <windows.h>','').replace('#include "d3d9types.h"','').replace('#include "3g.hpp"','')
        text=text.replace('g_D3DDCaps.MaxAnisotropy','srhd_awa::matrixgame::platform3g::Current().caps.max_anisotropy')
        text=text.replace('g_D3DD->SetSamplerState','srhd_awa::matrixgame::platform3g::ReadyDevice().SetSamplerState')
        for old,new in names.items():text=re.sub(r'\b'+old+r'\b',lambda m:new,text)
        if name.endswith('.hpp'):text=text.replace('#define MATRIX_SAMPLER_INCLUDE','#define MATRIX_SAMPLER_INCLUDE\n#include "platform_3g.hpp"')
        files[name]=text.encode()
