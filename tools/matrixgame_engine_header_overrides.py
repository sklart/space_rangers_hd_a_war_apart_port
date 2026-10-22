"""Separate complete engine declarations from concrete SDK implementation includes.

No SDK shim, invented device implementation, or missing game-class tail. The
cache/engine use one CVectorObject class. Billboard SDK resource pointers stay
opaque and unimplemented GPU methods remain link/compile blocked.
"""
import hashlib
import re

def patch_engine_headers(output):
    changed={}
    def write(name,text):
        path=output/name;path.write_bytes(text.encode())
        changed[name]=hashlib.sha256(path.read_bytes()).hexdigest()
    vector='MatrixLib/3G/VectorObject.hpp'
    h=(output/vector).read_text(encoding='utf-8')
    # These private light payloads are used only in VectorObject.cpp. The
    # animator still owns the same pointer/count fields; no lights are removed.
    first=h.index('struct SColorInterval {');last=h.index('class CVectorObjectAnim :',first)
    light_data=h[first:last]
    tail=h[last:]
    for old,new in [('DWORD','std::uint32_t'),('BYTE','std::uint8_t'),('WORD','std::uint16_t')]:
        tail=re.sub(r'\b'+old+r'\b',new,tail)
    tail=tail.replace('DTRACE();','')
    write(vector,'''// Complete original VO/animator/group declarations; shared native VO ABI.
#pragma once
#include "VectorObjectShared.hpp"
struct SLightData;
'''+tail)
    cpp='MatrixLib/3G/VectorObject.cpp'
    c=(output/cpp).read_text(encoding='utf-8')
    c=c.replace('#include "VectorObject.hpp"', '''#include "VectorObject.hpp"
#include "D3DControl.hpp"
#include "ShadowProj.hpp"
#include "CBillboard.hpp"
'''+light_data)
    write(cpp,c)
    billboard='MatrixLib/3G/CBillboard.hpp';b=(output/billboard).read_text(encoding='utf-8')
    b=b.replace('#include "D3DControl.hpp"','''#include "BaseDef.hpp"
#include "CMain.hpp"
#include "math_types.hpp"
// Same empty argument list as original Tracer.hpp release configuration.
#ifndef TRACE_PARAM_DEF
#if defined(_DEBUG) || defined(_TRACE)
#error Native billboard trace declarations require the original trace ABI.
#else
#define TRACE_PARAM_DEF
#endif
#endif
// Genuine SDK interfaces, opaque here. No fake methods or backend success.
struct IDirect3DVertexBuffer9;
struct IDirect3DIndexBuffer9;
class D3DResource;
class CTextureManaged;''')
    b=b.replace('    static D3D_VB m_VB;\n    static D3D_IB m_IB;', '''#ifdef _DEBUG
    static D3DResource* m_VB;
    static D3DResource* m_IB;
#else
    static IDirect3DVertexBuffer9* m_VB;
    static IDirect3DIndexBuffer9* m_IB;
#endif''')
    b=b.replace('#define BILLBOARD_FVF (D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1)',
                '#define BILLBOARD_FVF 0x142u // serialized XYZ | DIFFUSE | TEX1 vertex layout')
    b=b.replace('static void BeforeDraw(void) { m_SortableTex->Preload(); }','static void BeforeDraw(void);')
    for old,new in [('DWORD','std::uint32_t'),('BYTE','std::uint8_t')]:b=re.sub(r'\b'+old+r'\b',new,b)
    write(billboard,b)
    cpp='MatrixLib/3G/CBillboard.cpp';c=(output/cpp).read_text(encoding='utf-8')
    c=c.replace('#include "CBillboard.hpp"', '''#include "CBillboard.hpp"
#include "D3DControl.hpp"
#include "Texture.hpp"
void CBillboard::BeforeDraw(void) { m_SortableTex->Preload(); }
''')
    write(cpp,c)
    sound='MatrixGame/src/MatrixSoundManager.hpp';s=(output/sound).read_text(encoding='utf-8')
    # Sound declarations use only vector math, not a graphics device or texture.
    s=s.replace('#include "d3d9.h"\n#include "d3dx9tex.h"','#include "math_types.hpp"')
    write(sound,s)
    helper='MatrixLib/3G/Helper.hpp';h=(output/helper).read_text(encoding='utf-8')
    for include in ('#include <windows.h>','#include "d3d9.h"','#include "d3dx9tex.h"'):
        h=h.replace(include,'')
    h=h.replace('#if (defined _DEBUG) && !(defined _RELDEBUG)', '''#if (defined _DEBUG) && !(defined _RELDEBUG)
#include <windows.h>
#include "d3d9.h"
#include "d3dx9tex.h"''')
    write(helper,h)
    return changed
