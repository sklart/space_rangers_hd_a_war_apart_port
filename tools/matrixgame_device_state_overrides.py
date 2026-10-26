"""Port the full original DeviceState TU through an explicit state backend.

Register ordinals index the original tables; they are not D3D numeric IDs. The
Windows table uses real SDK constants and no portable backend invents success.
"""
import re
INPUTS={
    'DeviceState.cpp':'f627dbaa15a1e2a75304469ccbb0cea13f0f10a13810bd30d13fbb3b915aefa3',
    'DeviceState.hpp':'12f087bde83f8be6088cf24aee48d8c8d994231cefcd3bbb855d5ad2690f18c7',
}
SUBDIRS={name:'../3G' for name in INPUTS}

def patch_device_state(files):
    cpp=files['DeviceState.cpp'].decode()
    render=re.findall(r'\bD3DRS_\w+',cpp)
    sampler=re.findall(r'\bD3DSAMP_\w+',cpp)
    if len(render)!=103 or len(set(render))!=103 or len(sampler)!=13:
        raise ValueError('Original device-state register tables changed')
    enums=''
    for kind,keys in [('StoredRenderRegister',render),('StoredSamplerRegister',sampler)]:
        enums+='enum class '+kind+':std::uint32_t {'+','.join(k.split('_',1)[1] for k in keys)+',Count};\n'
    files['device_state_registers.hpp']=('#ifndef SRHD_MATRIXGAME_STORED_REGISTER_TYPES\n#define SRHD_MATRIXGAME_STORED_REGISTER_TYPES\n#include <cstdint>\nnamespace srhd_awa::matrixgame::platform3g {\n'+enums+'}\n#endif\n').encode()
    files['device_state_sdk_tables.inc']=('''// Original ordered DeviceState.cpp tables, resolved by the real Windows SDK.
static constexpr D3DRENDERSTATETYPE stored_render_keys[]{'''+','.join(render)+'''};
static constexpr D3DSAMPLERSTATETYPE stored_sampler_keys[]{'''+','.join(sampler)+'''};
static_assert(std::size(stored_render_keys)==static_cast<unsigned>(StoredRenderRegister::Count));
static_assert(std::size(stored_sampler_keys)==static_cast<unsigned>(StoredSamplerRegister::Count));
''').encode()
    names={'DWORD':'std::uint32_t','BOOL':'std::int32_t',
        'IDirect3DDevice9':'srhd_awa::matrixgame::platform3g::StoredStateBackend',
        'D3DLIGHT9':'srhd_awa::matrixgame::platform3g::StoredLight',
        'D3DMATERIAL9':'srhd_awa::matrixgame::platform3g::StoredMaterial',
        'D3DRENDERSTATETYPE':'srhd_awa::matrixgame::platform3g::StoredRenderRegister',
        'D3DSAMPLERSTATETYPE':'srhd_awa::matrixgame::platform3g::StoredSamplerRegister'}
    names.update({key:'srhd_awa::matrixgame::platform3g::StoredRenderRegister::'+key.split('_',1)[1] for key in render})
    names.update({key:'srhd_awa::matrixgame::platform3g::StoredSamplerRegister::'+key.split('_',1)[1] for key in sampler})
    for name in INPUTS:
        text=files[name].decode().replace('#include "3g.hpp"','')
        for old,new in names.items():text=re.sub(r'\b'+old+r'\b',lambda m:new,text)
        if name.endswith('.hpp'):
            text=text.replace('#define _DEVICE_STATE_INCLUDE','#define _DEVICE_STATE_INCLUDE\n#include "CMain.hpp"\n#include "device_state_backend.hpp"')
            text=text.replace('    std::uint32_t m_RenderStates[RS_COUNT];','    std::uint32_t m_RenderStates[RS_COUNT]{};')
            text=text.replace('m_SamplerStates[SS_COUNT * 8];','m_SamplerStates[SS_COUNT * 8]{};').replace('m_LightsEnabled[LI_COUNT];','m_LightsEnabled[LI_COUNT]{};')
            text=text.replace('m_Lights[LI_COUNT];','m_Lights[LI_COUNT]{};').replace('m_Material;','m_Material{};\n    std::uint32_t m_StoredGroups{};')
            text=text.replace(': m_Device(dev) {}',': m_Device(dev) { if(!dev)throw std::invalid_argument("null device-state backend"); }')
            # A partial failed capture must never expose uninitialized state.
        else:
            text=text.replace('#include "DeviceState.hpp"','#include "DeviceState.hpp"\n#include <cassert>\n#include <cstring>\n#define ASSERT(x) assert(x)\n#define DTRACE()')
            text=re.sub(r'(m_Device->\w+\([^;]+\));',r'srhd_awa::matrixgame::platform3g::CheckDeviceResult(\1,"CDeviceState backend");',text)
            # Original GetLight failures for never-assigned SDK slots leave
            # the zeroed Type unchanged. Capture absence explicitly without
            # swallowing real device errors or faking GetLight success.
            light_queries='''            srhd_awa::matrixgame::platform3g::CheckDeviceResult(m_Device->GetLight(i, m_Lights + i),"CDeviceState backend");
            srhd_awa::matrixgame::platform3g::CheckDeviceResult(m_Device->GetLightEnable(i, m_LightsEnabled + i),"CDeviceState backend");'''
            if text.count(light_queries)!=1:raise ValueError('Original light capture boundary changed')
            text=text.replace(light_queries,'            m_Device->CaptureLightSlot(i, m_Lights + i, m_LightsEnabled + i);',1)
            for method in ('StoreState','RestoreState'):
                signature='void CDeviceState::'+method+'(std::uint32_t states) {'
                check='\n    if(states!=std::uint32_t(DEVSTATE_ALL) && (states & ~15u))throw std::invalid_argument("unknown device-state groups");'
                check+='\n    const auto groups=states & 15u;'
                if method=='StoreState':check+='\n    m_StoredGroups &= ~groups;'
                else:check+='\n    if((m_StoredGroups & groups)!=groups)throw std::logic_error("device state was not captured");'
                text=text.replace(signature,signature+check)
            boundary='}\nvoid CDeviceState::RestoreState'
            if boundary not in text:raise ValueError('Device-state function boundary changed')
            text=text.replace(boundary,'    m_StoredGroups |= groups;\n}\nvoid CDeviceState::RestoreState')
        files[name]=text.encode()
