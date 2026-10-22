"""Reach the pinned CVectorObject CPU lifecycle; GPU methods stay unimplemented.

This is an explicit subset of the actual class/TU, not a replacement loader.
The full engine continues to see the original GPU-dependent header.
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
    header = h[:end]
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
    header = header.replace(original, 'static void DrawBegin(void); // GPU prerequisite: no CPU definition')
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

    CStorage stor(g_CacheHeap);''', '''    if (olf != OLF_NO_TEX)
        throw std::runtime_error("UNSUPPORTED: CVectorObject material/GPU lifecycle");
    Unload();

    CStorage stor(g_CacheHeap);''')
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
    header = header.replace('#include "vector_object_cpu_math.hpp"', '#include "vector_object_cpu_math.hpp"\n#include "buffer_geometry.hpp"')
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
    files['VectorObjectCpu.hpp'] = header.encode()
    files['VectorObjectCpu.cpp'] = (c[:c.index('#include <new>')] + '''
#include "VectorObjectCpu.hpp"
#include "CStorage.hpp"
#include "vector_geometry.hpp"
#include <new>
#include <stdexcept>
''' + selected).encode()
