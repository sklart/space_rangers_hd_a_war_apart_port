"""Pinned original Math3D CPU algorithms and alias-safe native preparation."""
import hashlib
import json
import re

INPUTS = {
 'Math3D.hpp':'f444325e3bfea812f94c8b2c1e8d06670686af65675c9ba6cad762b2278c7b14',
 'Math3D.cpp':'ce5c5be94a6cbce29ee766922c43d70940590541a639b0268638a6aa3fe21804',
}
SUBDIRS = {name:'../3G' for name in INPUTS}


def patch_math(files, root, body):
    deps=root/'port/matrixgame/third_party'
    manifest=json.loads((deps/'math-provenance.json').read_text())
    for name, expected in manifest['files_sha256'].items():
        path=(deps/name).resolve()
        if not path.is_relative_to(deps.resolve()) or hashlib.sha256(path.read_bytes()).hexdigest()!=expected:
            raise ValueError('Pinned math dependency mismatch: '+name)
    h=files['Math3D.hpp'].decode()
    h=h.replace('#include "d3d9.h"\n#include "d3dx9tex.h"','#include "math_types.hpp"')
    h=h.replace('#include <random.hpp>', '#include "random.hpp"\n#include <bit>\n#include <cstdint>\n#include <limits>')
    h=re.sub(r'\bDWORD\b','std::uint32_t',h);h=re.sub(r'\bBYTE\b','std::uint8_t',h)
    h=h.replace('#define M_PI        3.14159265358979323846','#ifndef M_PI\n#define M_PI        3.14159265358979323846\n#endif')
    start=h.index('#define FP_INV(');end=h.index('inline unsigned char FP_NORM_TO_BYTE2',start)
    h=h[:start]+'''#define FP_INV(r,p) { const float native_p=(p); const auto native_i=uint32_t(2*FP_ONE_BITS)-std::bit_cast<uint32_t>(native_p); (r)=std::bit_cast<float>(native_i); (r)=(r)*(2.0f-native_p*(r)); }

'''+h[end:]
    h=h.replace('((*(unsigned *)&fpTmp) >> 15)', '(std::bit_cast<uint32_t>(fpTmp) >> 15)')
    h=h.replace('((*(unsigned long *)&ftmp) & 0xFF)', '(std::bit_cast<uint32_t>(ftmp) & 0xFF)')
    start=h.index('#define IS_ZERO_VECTOR');end=h.index('inline bool IsVec3Equal',start)
    h=h[:start]+'''inline bool NativeZeroVector(const D3DXVECTOR3& v) {
    return ((std::bit_cast<uint32_t>(v.x)|std::bit_cast<uint32_t>(v.y)|std::bit_cast<uint32_t>(v.z))&0x7fffffff)==0;
}
#define IS_ZERO_VECTOR(v) NativeZeroVector(v)
#define NEG_FLOAT(x) { (x)=std::bit_cast<float>(std::bit_cast<uint32_t>(x)^0x80000000u); }
#define MAKE_ABS_FLOAT(x) { (x)=std::bit_cast<float>(std::bit_cast<uint32_t>(x)&0x7fffffffu); }
#define MAKE_ABS_DOUBLE(x) { (x)=std::bit_cast<double>(std::bit_cast<uint64_t>(x)&0x7fffffffffffffffull); }
#define COPY_SIGN_FLOAT(to,from) { (to)=std::bit_cast<float>((std::bit_cast<uint32_t>(to)&0x7fffffffu)|(std::bit_cast<uint32_t>(from)&0x80000000u)); }
#define SET_SIGN_FLOAT(to,sign) { (to)=std::bit_cast<float>((std::bit_cast<uint32_t>(to)&0x7fffffffu)|(uint32_t(sign)<<31)); }

'''+h[end:]
    h=h.replace('SinCosTable[ftmp.i & (SIN_TABLE_SIZE - 1)]','SinCosTable[std::bit_cast<int32_t>(ftmp.f) & (SIN_TABLE_SIZE - 1)]')
    h=h.replace('CHeap *m_Heap;', 'CHeap *m_Heap = nullptr;')
    # The separated original plane fields are not a union: synchronize both representations.
    h=h.replace('        out.UpdateSignBits();','        out.norm={out.dxplane.a,out.dxplane.b,out.dxplane.c};\n        out.dist=out.dxplane.d;\n        out.UpdateSignBits();')
    h=h.replace('    inline void UpdateSignBits(void) {', '    inline void UpdateSignBits(void) {\n        dxplane={norm.x,norm.y,norm.z,dist};')
    h=h.replace('    CTrajectory(void) :', '    CTrajectory(const CTrajectory&) = delete;\n    CTrajectory& operator=(const CTrajectory&) = delete;\n    CTrajectory(void) :')
    files['Math3D.hpp']=h.encode()
    c=files['Math3D.cpp'].decode().replace('#include "3g.hpp"','#include "pick_viewport.hpp"').replace('DTRACE();','')
    c=c.replace('memcpy(m_Segments, m_Segments +', 'memmove(m_Segments, m_Segments +')
    c=c.replace('    D3DXMatrixInverse(&m, NULL, &matView);','    if (g_ScreenX <= 0 || g_ScreenY <= 0 || matProj._11 == 0 || matProj._22 == 0 ||\n        !D3DXMatrixInverse(&m, NULL, &matView)) ERROR_E;')
    signature='void CalcPick(CPoint mp, D3DXMATRIX &matProj, D3DXMATRIX &matView, D3DXVECTOR3 *vpos, D3DXVECTOR3 *vdir) {'
    if c.count(signature)!=1:raise ValueError('Original CalcPick signature changed')
    c=c.replace(signature,signature+'\n    const auto viewport=srhd_awa::matrixgame::ReadPickViewport();\n    const int native_screen_x=viewport.width,native_screen_y=viewport.height;')
    c=c.replace('g_ScreenX','native_screen_x').replace('g_ScreenY','native_screen_y')
    for name in ('Init1','Init2'):
        signature=f'void CTrajectory::{name}(const D3DXVECTOR3 *points, int pcnt)'
        c=c.replace(signature+' {', signature+''' {
    if (!points || pcnt < 4 || size_t(pcnt) > (256u*1024u*1024u)/sizeof(STrajectorySegment)) ERROR_E;
    m_Len=-1; m_CurSegLen=-1;''')
    for name in ('Continue1','Continue2'):
        signature=f'void CTrajectory::{name}(const D3DXVECTOR3 *points, int pcnt)'
        c=c.replace(signature+' {',signature+''' {
    if (!points || pcnt < 1 || m_SegCnt < 1 || size_t(m_SegCnt)+size_t(pcnt)+1 > (256u*1024u*1024u)/sizeof(STrajectorySegment)) ERROR_E;''')
    for signature in ('void CTrajectory::Move(float dist)','void CTrajectory::CalcPoint(D3DXVECTOR3 &out, float t)','float CTrajectory::CalcLength(void)'):
        c=c.replace(signature+' {',signature+' {\n    if (m_SegCnt < 1) ERROR_E;')
    # Publish new segment counts/cache state only after successful allocation.
    for name in ('Init1','Init2'):
        signature=f'void CTrajectory::{name}(const D3DXVECTOR3 *points, int pcnt)'
        start=c.index(signature)
        end=c.index('\nvoid CTrajectory::',start+len(signature))
        chunk=c[start:end].replace('    m_Len=-1; m_CurSegLen=-1;', '')
        chunk=chunk.replace('m_SegCnt','new_count').replace('    new_count = pcnt - 1;', '    const int new_count = pcnt - 1;')
        chunk=chunk.replace('    m_CurSegT = 0;', '    m_CurSegT = 0;\n    m_SegCnt=new_count; m_Len=-1; m_CurSegLen=-1;')
        c=c[:start]+chunk+c[end:]
    c=body(c,'float CTrajectory::CutBefore(float t)', """float CTrajectory::CutBefore(float t) {
    if (m_SegCnt < 1 || !std::isfinite(t)) ERROR_E;
    if (m_SegCnt == 1) return t;
    const float tseg=t*float(m_SegCnt);
    if (tseg < 1) return t;
    const int seg=tseg >= float(m_SegCnt) ? m_SegCnt-1 : TruncFloat(tseg);
    m_SegCnt-=seg;
    memmove(m_Segments,m_Segments+seg,sizeof(STrajectorySegment)*size_t(m_SegCnt));
    m_CurSeg=0; m_CurSegT=0; m_CurSegLen=-1; m_Len=-1;
    return tseg >= float(m_SegCnt+seg) ? 1.0f : tseg-float(seg);
}""")
    files['Math3D.cpp']=c.encode()
