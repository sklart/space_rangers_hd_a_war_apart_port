"""Original CVectorObjectAnim no-billboard path connected to native VO Draw.

Explicit selected class variant, not a claim that full VectorObject.cpp or
billboard/projected-shadow functionality has been ported.
"""
import re
from matrixgame_vector_object_overrides import function

def patch_animation(files):
    # VectorObject source/header pin hashes are checked by core preparation.
    h=files['VectorObject.hpp'].decode();c=files['VectorObject.cpp'].decode()
    begin=h.index('class CVectorObjectAnim :')
    end=h.index('\nclass CVectorObjectGroup;',begin)
    header=h[begin:end]
    header=header.replace('    SLightData *m_Lights;\n    int m_LightsCnt;','')
    header=header.replace('    void InitLights(CTextureManaged *tex_light);','')
    header=header.replace('    void DrawLights(bool now, const D3DXMATRIX &objma, const D3DXMATRIX *iview);','')
    shadow=function(header,'void CalcShadowProjMatrix(')
    header=header.replace(shadow,'')
    header=re.sub(r'    bool Pick\(.*?const;\n','',header,flags=re.S)
    header=re.sub(r'    bool PickFull\(.*?const;\n','',header,flags=re.S)
    constructor=function(c,'CVectorObjectAnim::CVectorObjectAnim()')
    constructor=constructor.replace(': CMain(), m_Lights(NULL), m_LightsCnt(0)',': CMain()')
    constructor=constructor.replace('    m_VO = NULL;','    m_VO = NULL;\n    m_Skin = NULL;')
    clear=function(c,'void CVectorObjectAnim::Clear()')
    clear=clear[:clear.index('    if (m_Lights)')]+ '    m_Skin = NULL;\n}\n'
    init=function(c,'void CVectorObjectAnim::Init(')
    init=init.replace('    Clear();','''    if(tex_light)throw std::logic_error("UNSUPPORTED: animated billboard lights");
    if(!vo || !vo->IsLoaded() || !vo->GetAnimCount())
        throw std::invalid_argument("animation requires a loaded VO with animations");
    Clear();''')
    init=init.replace('    if (tex_light)\n        InitLights(tex_light);','')
    takt=function(c,'bool CVectorObjectAnim::Takt(')
    begin=takt.index('    for (int i = 0; i < m_LightsCnt;')
    end=takt.index('    int old_frame',begin)
    takt=takt[:begin]+takt[end:]
    # Preserve the original clock/strict > and frame loop. Reject unsupported
    # elapsed inputs before the legacy signed-int addition can overflow.
    takt=takt.replace('    m_Time += cms;','''    if(!m_VO)throw std::logic_error("animation not initialized");
    if(cms<0 || cms>std::numeric_limits<int>::max()-m_Time)
        throw std::out_of_range("animation elapsed milliseconds");
    m_Time += cms;''')
    selected='\n'.join([constructor,function(c,'CVectorObjectAnim::~CVectorObjectAnim()'),clear,init,takt,
        function(c,'DWORD CVectorObjectAnim::NextFrame('),
        function(c,'void CVectorObjectAnim::GetBound('),
        function(c,'void CVectorObjectAnim::BeforeDraw(')])
    header=header.replace('CVectorObjectAnim','NativeVectorObjectAnim')
    selected=selected.replace('CVectorObjectAnim','NativeVectorObjectAnim')
    for text_name in ('header','selected'):
        text=locals()[text_name].replace('DTRACE();','')
        text=re.sub(r'\bDWORD\b','std::uint32_t',text)
        if text_name=='header':header=text
        else:selected=text
    files['VectorObjectAnimNative.hpp']=('''// Original no-billboard animation path; GPL-2.0-or-later.
#pragma once
#include "VectorObjectCpu.hpp"
// Borrowed VO/SSkin must remain alive until Clear/destruction. This class
// preserves original timing semantics; it is not the full billboard animator.
'''+header).encode()
    files['VectorObjectAnimNative.cpp']=('''// Original no-billboard animation methods; GPL-2.0-or-later.
#include "VectorObjectAnimNative.hpp"
#include <limits>
#include <stdexcept>
'''+selected+'\n').encode()
