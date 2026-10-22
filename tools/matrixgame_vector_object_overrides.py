"""Pinned CVectorObject lifecycle and selected Draw via typed native callbacks.

This is an explicit implementation subset, not a replacement loader. The full
engine shares the complete CVectorObject declaration and retains original
animator/group declarations. Full-TU GPU methods and legacy material parity stay open.
"""
import re

INPUTS = {'Cache.hpp': 'e1f9ccc07faa08d5182e52ec45516b800962326ce8ca08809935ac75a364e62b'}
SUBDIRS = {'Cache.hpp': '../3G'}


def function(text, signature):
    if text.count(signature) != 1:
        raise ValueError('Pinned VO function changed: ' + signature)
    start = text.index(signature)
    opening = text.index('{', start)
    depth, end = 1, opening + 1
    while depth:
        if text[end] == '{': depth += 1
        elif text[end] == '}': depth -= 1
        end += 1
    return text[start:end]


def patch_vector_object(files):
    # Input hashes are checked by matrixgame_vector_overrides before this step.
    h = files['VectorObject.hpp'].decode()
    c = files['VectorObject.cpp'].decode()
    end = h.index('\nstruct SSkin {')
    header = h[:end] + '\n' + h[end+1:h.index('\nstruct SColorInterval',end)]
    for include in ('d3d9.h', 'd3dx9tex.h', 'D3DControl.hpp', 'ShadowProj.hpp', 'CBillboard.hpp'):
        header = header.replace('#include "' + include + '"', '')
    header = header.replace('extern IDirect3DDevice9 *g_D3DD;', '''#include "Math3D.hpp"
#include "vector_object_cpu_math.hpp"
#include "CBlockPar.hpp"
class CTextureManaged;
class CBaseTexture;
class CVOShadowCliper;
struct SProjData;''')
    # Keep the actual vertex layout selector used by BigVB, without device APIs.
    header = header.replace('#define VO_FVF (D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1)',
                            '#define VO_FVF 0x112u // original XYZ | NORMAL | TEX1 layout')
    original = function(header, 'static void DrawBegin(void)')
    native_begin=original.replace('ASSERT_DX(g_D3DD->SetFVF(VO_FVF));',
        'srhd_awa::matrixgame::CurrentNativeDrawSink().SetVertexLayout(VO_FVF);')
    native_begin=native_begin.replace('        m_IB->BeforeDraw();',
        '        if(!m_IB || !m_VB)throw std::logic_error("VO buffers not loaded");\n        m_IB->BeforeDraw();')
    header = header.replace(original,native_begin)
    header = header.replace('    CBlockPar m_Props;',
        '    CBlockPar m_Props;\n    std::vector<std::shared_ptr<const SSkin>> m_NativeSkinOwners;')
    selected = c[c.index('CBigVB<SVOVertex> *CVectorObject::m_VB;'):c.index('bool CVectorObject::PickSimple(')]
    selected += '\n' + function(c, 'void CVectorObject::GetBound(') + '\n'
    selected = re.sub(r'^#pragma warning.*$', '', selected, flags=re.M)
    selected = selected.replace('DTRACE();', '')
    allocation = '''    if (m_VB == NULL) {
        m_VB = CBigVB<SVOVertex>::NewBigVB(g_CacheHeap);
        m_IB = CBigIB::NewBigIB(g_CacheHeap);
    }'''
    if selected.count(allocation) != 1:
        raise ValueError('Pinned VO buffer construction changed')
    # Unload destroys the shared buffers when the last source leaves. The
    # original constructor-only allocation breaks a subsequent LoadSpecial on
    # that same object. Allocate after validation for each real load instead;
    # constructing an unloaded/failed object then owns no empty buffer objects.
    selected = selected.replace(allocation, '')
    selected = selected.replace('''    Unload();

    CStorage stor(g_CacheHeap);''', '''    if (olf<OLF_NO_TEX || olf>OLF_SIMPLE)
        throw std::invalid_argument("VO load flags");
    if ((olf==OLF_SIMPLE || olf==OLF_AUTO) && !sg)
        throw std::invalid_argument("VO skin provider required");
    if (olf!=OLF_NO_TEX && sg && (sg!=srhd_awa::matrixgame::AcquireOwnedSkin || !srhd_awa::matrixgame::SkinProviderBound()))
        throw std::logic_error("UNSUPPORTED: unowned skin provider");
    Unload();

    CStorage stor(g_CacheHeap);''')
    selected=selected.replace('void CVectorObject::Unload() {',
        'void CVectorObject::Unload() {\n    m_NativeSkinOwners.clear();')
    old_skin='''    for (int i = 0; i < m_Geometry.m_SurfacesCnt; ++i) {
        SVOSurface *s = m_Geometry.m_Surfaces + i;
        s->skin = sg(s->texname.c_str(), gsp);
    }'''
    if selected.count(old_skin)!=1:raise ValueError('Pinned skin assignment changed')
    selected=selected.replace(old_skin,'''    try {
        if(!sg)throw std::invalid_argument("VO skin provider required");
'''+old_skin.replace('        s->skin = sg(s->texname.c_str(), gsp);',
        '        s->skin = sg(s->texname.c_str(), gsp);\n        m_NativeSkinOwners.push_back(srhd_awa::matrixgame::TakeOwnedSkin(s->skin));')+'''
    } catch(...) {Unload();throw;}
''')
    selected = selected.replace('''#ifdef _DEBUG
    bool ok =
#endif
            stor.Load(m_Name.c_str());''', '''    if (!stor.Load(m_Name.c_str()))
        throw std::runtime_error("CVectorObject resource load failed");
    // Validate the same storage before the legacy raw payload copies.
    // The checked reader is not used to populate m_Geometry.
    const auto checked = srhd_awa::matrixgame::VectorGeometry::Load(stor);
    for (const auto& a : checked.Animations())
        if (a.name.size() >= 32) throw std::invalid_argument("VO animation name extent");
    for (const auto& m : checked.MatrixNames())
        if (m.name.size() >= 32) throw std::invalid_argument("VO matrix name extent");
''' + allocation)
    # Portable builds explicitly reject debug ABI; its removed load 'ok' is not used.
    selected = re.sub(r'#ifdef _DEBUG\n    if \(!ok\) \{.*?\n#endif\n', '', selected, flags=re.S)
    for old, new in (('DWORD','std::uint32_t'), ('WORD','std::uint16_t'), ('BYTE','std::uint8_t')):
        header = re.sub(r'\b'+old+r'\b', new, header)
        selected = re.sub(r'\b'+old+r'\b', new, selected)
    header = header.replace('DTRACE();', '')
    header = header.replace('#pragma once', '''#pragma once
#if defined(_DEBUG) || defined(_TRACE) || defined(MEM_SPY_ENABLE)
#error CVectorObject CPU slice supports the pinned release lifecycle only
#endif''', 1)
    # Bridge only geometry submission, without claiming the unported skin/pass
    # callbacks of Draw. Keep the original surface-first union traversal and
    # original source Select calls; both bases belong to the shared Big buffers.
    header = header.replace('#include "vector_object_cpu_math.hpp"', '#include "vector_object_cpu_math.hpp"\n#include "skin_callbacks.hpp"')
    header = header.replace('    void BeforeDraw(void);', '''    // Explicit geometry bridge, not the legacy skin-driven Draw implementation.
    std::vector<srhd_awa::matrixgame::MeshRange> PrepareNativeFrame(int frame);
    int NativeSurfaceCount() const noexcept { return m_Geometry.m_SurfacesCnt; }
    void BeforeDraw(void);''')
    draw = function(c, 'void CVectorObject::Draw(int noframe,')
    begin = draw.index('    int vbase =')
    end = draw.index('    int pass =', begin)
    selects = draw[begin:end].replace('return;', 'throw std::runtime_error("VO buffer preparation failed");')
    begin = draw.index('            SVOKadr *k =')
    end = draw.index('#ifdef _DEBUG', begin)
    traversal = draw[begin:end]
    selected += '''
std::vector<srhd_awa::matrixgame::MeshRange> CVectorObject::PrepareNativeFrame(int noframe) {
    if (!IsLoaded() || noframe < 0 || noframe >= GetFramesCnt())
        throw std::out_of_range("VO native frame");
    // This entry accepts only the OLF_NO_TEX slice. Skin callbacks are not
    // bypassed for a skinned object; that contract still requires porting.
    for (int i=0;i<m_Geometry.m_SurfacesCnt;++i)
        if (m_Geometry.m_Surfaces[i].skin) throw std::logic_error("UNSUPPORTED: legacy skin callbacks");
    DX_Prepare();
    m_RemindCore.Use(FREE_VO_TIME_PERIOD);
    m_VB->BeforeDraw();
    m_IB->BeforeDraw();
''' + selects + '''
    std::vector<srhd_awa::matrixgame::MeshRange> ranges;
    for (int surf=0;surf<m_Geometry.m_SurfacesCnt;++surf) {
''' + traversal + '''
                ranges.push_back(srhd_awa::matrixgame::VectorObjectRange(*gr,vbase,ibase));
            }
    }
    return ranges;
}
'''
    before_draw=function(c,'void CVectorObject::BeforeDraw(').replace('DTRACE();','')
    actual_draw=function(c,'void CVectorObject::Draw(int noframe,')
    actual_draw=re.sub(r'#ifdef SHOW_PICK_OBJECTS.*?#endif', '',actual_draw,flags=re.S)
    begin=actual_draw.index('#ifdef _DEBUG')
    end=actual_draw.index('#endif',begin)+len('#endif')
    actual_draw=actual_draw[:begin]+'''                srhd_awa::matrixgame::CurrentNativeDrawSink().Emit(
                    srhd_awa::matrixgame::VectorObjectRange(*gr,vbase,ibase));'''+actual_draw[end:]
    actual_draw=actual_draw.replace('bool *nextpass = (bool *)_alloca(m_Geometry.m_SurfacesCnt);',
        'std::vector<bool> nextpass(m_Geometry.m_SurfacesCnt);')
    actual_draw=actual_draw.replace('    int vbase =',
        '    if(!IsLoaded() || noframe>=GetFramesCnt())throw std::out_of_range("VO draw frame");\n    int vbase =',1)
    actual_draw=actual_draw.replace('    if (vbase < 0)\n        return;',
        '    if (vbase < 0)throw std::runtime_error("VO vertex preparation failed");')
    actual_draw=actual_draw.replace('    if (ibase < 0)\n        return;',
        '    if (ibase < 0)throw std::runtime_error("VO index preparation failed");')
    actual_draw=actual_draw.replace('            again |=',
        '            if(!ss || !ss->m_SetupStages || !ss->m_SetupTex || !ss->m_SetupClear)\n                throw std::logic_error("VO draw has no complete skin callbacks");\n            again |=',1)
    actual_draw=actual_draw.replace('DTRACE();','').replace('DWORD','std::uint32_t')
    selected+='\n'+before_draw+'\n'+actual_draw+'\n'
    # One identical CVectorObject declaration for cache/CPU implementation and
    # the full engine header. A macro guard also covers independent generated
    # copies; pragma-once alone cannot prevent cross-view ODR redefinitions.
    files['VectorObjectShared.hpp'] = ('''#ifndef MATRIXGAME_NATIVE_VECTOR_OBJECT_SHARED_HPP
#define MATRIXGAME_NATIVE_VECTOR_OBJECT_SHARED_HPP
'''+header+'\n#endif\n').encode()
    files['VectorObjectCpu.hpp'] = b'#pragma once\n#include "VectorObjectShared.hpp"\n'
    files['VectorObjectCpu.cpp'] = (c[:c.index('#include <new>')] + '''
#include "VectorObjectCpu.hpp"
#include "CStorage.hpp"
#include "vector_geometry.hpp"
#include <new>
#include <stdexcept>
''' + selected).encode()
