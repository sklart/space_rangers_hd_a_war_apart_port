"""Original projected-shadow owner with explicit replacement/empty cleanup."""
import hashlib
from matrixgame_render_pipeline_overrides import adapt
from matrixgame_vector_object_overrides import function


def prepare_shadow_proj(output):
    changes={};name='MatrixLib/3G/ShadowProj.cpp';path=output/name
    text=path.read_text(encoding='utf-8')
    constructor='CVOShadowProj::CVOShadowProj(CHeap *heap)'
    eager='''    if (m_VB == NULL) {
        m_VB = CBigVB<SVOShadowProjVertex>::NewBigVB(m_Heap);
        m_IB = CBigIB::NewBigIB(m_Heap);
    }
'''
    if text.count(eager)!=1:raise ValueError('Shadow pool allocation changed')
    # ReadGeometry in the real derived map-shadow owner directly uses these
    # pools. Preserve eager construction and retain pools until the last owner.
    text=text.replace(eager,eager+'    ++m_OwnerCount;\n',1)
    destructor=function(text,'CVOShadowProj::~CVOShadowProj()')
    begin=destructor.index('    if (m_preVB.verts) {');end=destructor.index('\n    if (m_Tex)',begin)
    release=destructor[begin:end]
    start=release.index('        if (m_IB->DelSource(&m_preIB)) {')
    stop=release.index('        HFree(m_preVB.verts, m_Heap);',start)
    release=release[:start]+'''        m_IB->DelSource(&m_preIB);
        m_VB->DelSource(&m_preVB);
'''+release[stop:]
    text=text.replace(destructor,'''void CVOShadowProj::ReleaseGeometry() {
'''+release+'''
    m_preVB.verts = nullptr;
    m_preIB.inds = nullptr;
    m_preVB.size = m_preIB.size = 0;
    m_VertCnt = m_TriCnt = 0;
}

CVOShadowProj::~CVOShadowProj() {
    ReleaseGeometry();
    DestroyTexture();
    if (--m_OwnerCount == 0) {
        CBigIB::DestroyBigIB(m_IB); m_IB = nullptr;
        CBigVB<SVOShadowProjVertex>::DestroyBigVB(m_VB); m_VB = nullptr;
    }
}''',1)
    signature='void CVOShadowProj::Prepare('
    before=function(text,signature);after=before
    after=after.replace('    if (vertscnt <= 0) {','    ReleaseGeometry();\n    if (vertscnt <= 0) {\n        DestroyTexture();',1)
    # ReleaseGeometry already detached the old source and freed its combined block.
    old='''    if (m_preVB.verts)
        HFree(m_preVB.verts, m_Heap);
'''
    if after.count(old)!=1:raise ValueError('Shadow geometry replacement changed')
    after=after.replace(old,'',1);text=text.replace(before,after,1)
    text=adapt(text).replace('g_D3DD->DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP,','proj_device::DrawIndexedPrimitive(5u,')
    signature='void CVOShadowProj::RenderMin(void) {'
    if text.count(signature)!=1:raise ValueError('Shadow RenderMin entry changed')
    text=text.replace(signature,signature+'\n    if (!IsProjected()) return;',1)
    text=text.replace(constructor,'''#undef FAILED_DX
#define FAILED_DX(call) rp3g::CheckDeviceResult((call), #call)
#undef ASSERT_DX
#ifdef ASSERT_OFF
#define ASSERT_DX(call) (call)
#else
#define ASSERT_DX(call) FAILED_DX(call)
#endif
'''+constructor,1)
    prelude='''#include "3g.hpp"
#include "ShadowProj.hpp"
#include "map_device_backend.hpp"
namespace rp3g=srhd_awa::matrixgame::platform3g;
namespace proj_device=srhd_awa::matrixgame::map_device;
int CVOShadowProj::m_OwnerCount = 0;
'''
    path.write_bytes((prelude+text).encode());changes[name]=hashlib.sha256(path.read_bytes()).hexdigest()
    name='MatrixLib/3G/ShadowProj.hpp';path=output/name;text=path.read_text(encoding='utf-8')
    marker='    CBaseTexture *m_Tex;'
    if text.count(marker)!=1:raise ValueError('Shadow owner declaration changed')
    text=text.replace(marker,'    void ReleaseGeometry();\n    static int m_OwnerCount;\n'+marker,1)
    for entry in ['    void DX_Prepare(void) {','    void DX_Free(void) {']:
        if text.count(entry)!=1:raise ValueError('Shadow staging entry changed')
        text=text.replace(entry,entry+'\n        if (!IsProjected()) return;',1)
    path.write_bytes(text.encode());changes[name]=hashlib.sha256(path.read_bytes()).hexdigest()
    return changes
