"""Retain complete original BigVB/BigIB algorithms behind a typed staging boundary."""
import re
INPUTS={
    'BigVB.hpp':'6353b2587de9c06050ee23785977fc7b14f4d91184ebc056d2d55ff7a9d4646f',
    'BigIB.hpp':'1be8a0dea336834fce0e040ca27cd924e66feed31e5a040ccec818e95b629d24',
}
SUBDIRS={name:'../3G' for name in INPUTS}
def patch_big_buffers(files):
    for name in INPUTS:
        text=files[name].decode()
        if text.count('#include "D3DControl.hpp"')!=1: raise ValueError('Big buffer include changed')
        text=text.replace('#include "D3DControl.hpp"','#include "big_buffer_bridge.hpp"')
        # Original empty destructors leak partial AddSource allocations on OOM.
        # Successful DelSource already leaves a null array, so the normal path
        # and its packing/dirty/binding behavior remain unchanged.
        if name=='BigVB.hpp':
            old='~CBigVB(){};'
            new='''~CBigVB() {
        if (m_VB_array) {
            ReleaseBuffers();
            for (int i=0;i<m_VB_count;++i) HFree(m_VB_array[i].parts,m_Heap);
            HFree(m_VB_array,m_Heap);
        }
    };'''
        else:
            old='~CBigIB(){};'
            new='''~CBigIB() {
        if (m_IB_array) {
            ReleaseBuffers();
            for (int i=0;i<m_IB_count;++i) HFree(m_IB_array[i].parts,m_Heap);
            HFree(m_IB_array,m_Heap);
        }
    };'''
        if text.count(old)!=1: raise ValueError('Big buffer destructor changed: '+name)
        text=text.replace(old,new)
        if name=='BigVB.hpp':
            old='''                int cc = m_VB_array[i].count;
                m_VB_array[i].count = cc + 1;
                m_VB_array[i].parts =
                        (SBigVBPart<V> *)HAllocEx(m_VB_array[i].parts, sizeof(SBigVBPart<V>) * (cc + 1), m_Heap);
                m_VB_array[i].parts[cc].source = src;'''
            new=old.replace('                m_VB_array[i].count = cc + 1;\n','')+'\n                m_VB_array[i].count = cc + 1;'
            if text.count(old)!=1: raise ValueError('BigVB growth publication changed')
            text=text.replace(old,new)
        for kind,member in (('VB','vb'),('IB','ib')):
            token=f'DESTROY_{kind}(m_{kind}_array[i].{member});'
            if token in text:
                if text.count(token)!=1: raise ValueError('Big buffer growth invalidation changed')
                text=text.replace(token,token+f'\n                    m_{kind}_array[i].noneedcount = m_{kind}_array[i].count;')
        text=re.sub(r'\bDWORD\b','std::uint32_t',text)
        text=re.sub(r'\bWORD\b','std::uint16_t',text)
        text=re.sub(r'\bD3D_VB\b','::srhd_awa::matrixgame::buffers::VertexBuffer*',text)
        text=re.sub(r'\bD3D_IB\b','::srhd_awa::matrixgame::buffers::IndexBuffer*',text)
        text=re.sub(r'\bDTRACE\(\);','',text)
        calls={
            'ASSERT_DX(g_D3DD->SetStreamSource(0, GET_VB(bvb->m_VB_array[one].vb), 0, sizeof(V)));':
                '::srhd_awa::matrixgame::buffers::Current().BindVertex(NATIVE_BUF_GET(bvb->m_VB_array[one].vb), 0, sizeof(V));',
            'ASSERT_DX(g_D3DD->SetIndices(GET_IB(bib->m_IB_array[one].ib)));':
                '::srhd_awa::matrixgame::buffers::Current().BindIndex(NATIVE_BUF_GET(bib->m_IB_array[one].ib));',
        }
        expected=next(iter(calls)) if name=='BigVB.hpp' else list(calls)[1]
        if text.count(expected)!=1: raise ValueError('Big buffer selection changed: '+name)
        text=text.replace(expected,calls[expected])
        for original,native in {
            'CREATE_VB':'NATIVE_BUF_CREATE_VB','CREATE_IB16':'NATIVE_BUF_CREATE_IB16',
            'IS_VB':'NATIVE_BUF_IS','IS_IB':'NATIVE_BUF_IS',
            'DESTROY_VB':'NATIVE_BUF_DESTROY','DESTROY_IB':'NATIVE_BUF_DESTROY',
            'GET_VB':'NATIVE_BUF_GET','GET_IB':'NATIVE_BUF_GET',
            'LOCKP_VB':'NATIVE_BUF_LOCK_PART','LOCKP_IB':'NATIVE_BUF_LOCK_PART',
            'UNLOCK_VB':'NATIVE_BUF_UNLOCK','UNLOCK_IB':'NATIVE_BUF_UNLOCK',
        }.items(): text=re.sub(r'\b'+original+r'\b',native,text)
        files[name]=text.encode()
