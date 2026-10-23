"""Complete original MatrixConfig TU: key data, typed gamma, safe resource lifetime."""
import hashlib
import re
from matrixgame_vector_object_overrides import function

def prepare_config(output):
    name='MatrixGame/src/MatrixConfig.cpp';path=output/name
    source=path.read_text(encoding='utf-8')
    # Serialized Windows key identifiers, not an emulated keyboard API.
    keys=dict(LBUTTON=1,RBUTTON=2,ESCAPE=27,PAUSE=19,SCROLL=145,CAPITAL=20,NUMLOCK=144,
        OEM_5=220,OEM_COMMA=188,OEM_PERIOD=190,OEM_4=219,OEM_6=221,OEM_MINUS=189,OEM_PLUS=187,
        DIVIDE=111,MULTIPLY=106,SUBTRACT=109,ADD=107,SEPARATOR=108,LEFT=37,RIGHT=39,UP=38,DOWN=40,
        BACK=8,TAB=9,RETURN=13,SPACE=32,INSERT=45,DELETE=46,HOME=36,END=35,PRIOR=33,NEXT=34,
        SHIFT=16,LSHIFT=160,RSHIFT=161,MENU=18,LMENU=164,RMENU=165,CONTROL=17,LCONTROL=162,RCONTROL=163,
        LWIN=91,RWIN=92,APPS=93)
    keys.update({'NUMPAD'+str(i):96+i for i in range(10)})
    for key,value in keys.items():source=re.sub(r'\bVK_'+key+r'\b',hex(value),source)
    if re.search(r'\bVK_\w+',source):raise ValueError('Unmapped original configuration key')
    source=re.sub(r'\bbyte\b','std::uint8_t',source)
    original=function(source,'void CMatrixConfig::Clear(void)')
    cleanup='''
    m_Cursors=nullptr;m_CursorsCnt=0;
    // These real config arrays were originally released by CGame::Deinit.
    // Clearing here also makes ReadParams retry and partial failure safe;
    // original Deinit's guarded releases then observe null pointers.
    if(m_Labels) {
        for(int i=0;i<LABELS_LAST;++i)m_Labels[i].~basic_string();
        HFree(m_Labels,g_MatrixHeap);m_Labels=nullptr;
    }
    if(m_Descriptions) {
        for(int i=0;i<DESCRIPTIONS_LAST;++i)m_Descriptions[i].~basic_string();
        HFree(m_Descriptions,g_MatrixHeap);m_Descriptions=nullptr;
    }
'''
    source=source.replace(original,original[:-1]+cleanup+'}')
    old='''    m_CursorsCnt = bp_tmp->ParCount();

    m_Cursors = (SStringPair *)HAlloc(sizeof(SStringPair) * m_CursorsCnt, g_CacheHeap);
    for (int i = 0; i < m_CursorsCnt; ++i) {
        new(&m_Cursors[i].key) std::wstring(bp_tmp->ParGetName(i));
        new(&m_Cursors[i].val) std::wstring(bp_tmp->ParGet(i));
    }'''
    new='''    const int requested_cursors = bp_tmp->ParCount();
    m_Cursors = (SStringPair *)HAlloc(sizeof(SStringPair) * requested_cursors, g_CacheHeap);
    m_CursorsCnt = 0;
    for (int i = 0; i < requested_cursors; ++i) {
        new(m_Cursors+i) SStringPair{};
        ++m_CursorsCnt; // Only live pairs may be destroyed if a string copy throws.
        m_Cursors[i].key = bp_tmp->ParGetName(i);
        m_Cursors[i].val = bp_tmp->ParGet(i);
    }'''
    if old not in source:raise ValueError('Original cursor construction changed')
    source=source.replace(old,new)
    source=source.replace('D3DGAMMARAMP newramp;','::srhd_awa::matrixgame::platform3g::GammaRamp newramp;')
    for channel in ('red','green','blue'):source=source.replace('GenRamp(newramp.'+channel+',','GenRamp(newramp.'+channel+'.data(),')
    old='g_D3DD->SetGammaRamp(0, D3DSGR_CALIBRATE, &newramp);'
    if source.count(old)!=1:raise ValueError('Original gamma submission changed')
    source=source.replace(old,'::srhd_awa::matrixgame::platform3g::ReadyDevice().ApplyCalibratedGamma(0, newramp);')
    prelude='''#include "platform_3g.hpp"
#include "math_types.hpp"
#include "memory_ops.hpp"
#include <type_traits>
static_assert(std::is_nothrow_default_constructible_v<std::wstring>);
'''
    path.write_bytes((prelude+source).encode())
    return {name:hashlib.sha256(path.read_bytes()).hexdigest()}
