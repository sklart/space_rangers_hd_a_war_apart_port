"""Prepare the pinned Microsoft CPU BC1/2/3 codec, without a DirectX device shim."""
import hashlib
import json
from pathlib import Path
HASHES={'BC.cpp':'7554b64224f39e68ff99f9b070394dfddee811593a1b3d7c8004c5c2f94a47b0',
        'BC.h':'498e86defa9e46f207a54aff2fbda814467c2d6d608e9d678ffff5e1cd142a49',
        'LICENSE':'c2cfccb812fe482101a8f04597dfc5a9991a6b2748266c47ac91b6a5aae15383'}


def prepare_bc(root,files):
    source=root/'port/matrixgame/third_party/directxtex_bc'
    metadata=json.loads((source/'provenance.json').read_text())
    if metadata['commit']!='4234f77df9c6c8bb2880607b82d5d597b54d1fa9' or metadata['sha256']!=HASHES:raise ValueError('BC source pin changed')
    data={name:(source/name).read_bytes().replace(b'\r\n',b'\n') for name in HASHES}
    for name,b in data.items():
        if hashlib.sha256(b).hexdigest()!=HASHES[name]:raise ValueError('BC source hash changed: '+name)
    if data['BC.cpp'].count(b'#include "DirectXTexP.h"')!=1:raise ValueError('BC private include changed')
    code=data['BC.cpp'].decode().replace('#include "DirectXTexP.h"','#include "bc_native_prelude.hpp"')
    # The pinned codec uses nearest rounding throughout. The original D3DX9
    # encoder brackets these regions with a toward-zero FPU guard instead.
    # Keep the vendor bytes intact; fail if the adaptation anchors drift.
    edits=[
        ('        uint16_t w;', '        BCRoundToZero encode_rounding;\n        uint16_t w;'),
        ('        for (size_t iIteration = 0; iIteration < 8;',
         '        BCRoundToZero optimize_rounding;\n        BCRefreshForRounding(X,Y);\n        for (size_t iIteration = 0; iIteration < 8;'),
        ('        pX->r = X.r;\n        pX->g = X.g;',
         '        optimize_rounding.Restore();\n        pX->r = X.r;\n        pX->g = X.g;'),
        ('        size_t i;\n        for (i = 0;',
         '        BCRoundToZero quantize_rounding;\n        size_t i;\n        for (i = 0;'),
        ('        // Perform 6D root finding',
         '        quantize_rounding.Restore();\n        // Perform 6D root finding'),
        ('        // Encode colors\n',
         '        BCRoundToZero index_rounding;\n        // Encode colors\n'),
        ('    float fError[NUM_PIXELS_PER_BLOCK] = {};\n    for (size_t i = 0;',
         '    BCRoundToZero alpha_rounding;\n    float fError[NUM_PIXELS_PER_BLOCK] = {};\n    for (size_t i = 0;'),
        ('    // RGB part\n#ifdef COLOR_WEIGHTS',
         '    alpha_rounding.Restore();\n    // RGB part\n#ifdef COLOR_WEIGHTS'),
        ('    float fMinAlpha = Color[0].a;',
         '    BCRoundToZero alpha_quantize_rounding;\n    float fMinAlpha = Color[0].a;'),
        ('#ifdef COLOR_WEIGHTS\n    if (0.0f == fMaxAlpha)',
         '    alpha_quantize_rounding.Restore();\n#ifdef COLOR_WEIGHTS\n    if (0.0f == fMaxAlpha)'),
        ('    const auto bAlphaA =',
         '    BCRoundToZero alpha_endpoint_rounding;\n    const auto bAlphaA ='),
        ('    fAlphaA = static_cast<float>(bAlphaA)',
         '    alpha_endpoint_rounding.Restore();\n    fAlphaA = static_cast<float>(bAlphaA)'),
        ('    for (size_t iSet = 0; iSet < 2; iSet++)',
         '    BCRoundToZero alpha_index_rounding;\n    for (size_t iSet = 0; iSet < 2; iSet++)'),
    ]
    for before,after in edits:
        if code.count(before)!=1:raise ValueError('BC rounding adaptation anchor changed: '+before)
        code=code.replace(before,after)
    for divisor,count in [(31,4),(63,2),(15,3),(255,5)]:
        before=f'(1.0f / {divisor}.0f)'
        if code.count(before)!=count:raise ValueError('BC reciprocal anchor changed: '+before)
        code=code.replace(before,f'BCInverse{divisor}')
    files['BC.cpp']=code.encode()
    header=data['BC.h'].decode().replace('#pragma once', '#pragma once\n#include "bc_native_prelude.hpp"')
    alpha_edits=[
        ('        for (size_t iIteration = 0; iIteration < 8;',
         '        BCRoundToZero alpha_optimize_rounding;\n        for (size_t iIteration = 0; iIteration < 8;'),
        ('        *pX = (fX < MIN_VALUE)',
         '        alpha_optimize_rounding.Restore();\n        *pX = (fX < MIN_VALUE)'),
        ('                    // D3DX had this computation backwards (pPoints[iPoint] - pSteps[iStep])\n'
         '                    // this fix improves RMS of the alpha component\n'
         '                    const float fDiff = pSteps[iStep] - pPoints[iPoint];',
         '                    // Preserve the original D3DX9 alpha endpoint update.\n'
         '                    const float fDiff = pPoints[iPoint] - pSteps[iStep];'),
    ]
    for before,after in alpha_edits:
        if header.count(before)!=1:raise ValueError('BC alpha adaptation anchor changed: '+before)
        header=header.replace(before,after)
    files['BC.h']=header.encode();return metadata
