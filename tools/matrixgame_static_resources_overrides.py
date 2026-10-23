"""Move genuine original engine static data to one shared native compilation unit."""
import hashlib
import re

# Canonical original definitions, never synthesized zero/fake implementations.
DEFINITIONS=(
 ('MatrixGame/src/MatrixGameDll.cpp',('g_RangersInterface','g_ExitState')),
 ('MatrixGame/src/Interface/CInterface.cpp',('g_IFaceList','g_PopupHead','g_PopupWeaponNormal','g_PopupWeaponExtern','g_PopupHull','g_PopupChassis','CInterface::m_ClearRects')),
 ('MatrixGame/src/Interface/CHistory.cpp',('g_ConfigHistory',)),
 ('MatrixGame/src/MatrixMapStatic.cpp',('CMatrixMapStatic::objects_left','CMatrixMapStatic::objects_rite','CMatrixMapStatic::m_FirstVisNew','CMatrixMapStatic::m_LastVisNew','CMatrixMapStatic::m_FirstVisOld','CMatrixMapStatic::m_LastVisOld','CMatrixMapStatic::m_FirstLogicTemp','CMatrixMapStatic::m_LastLogicTemp')),
 ('MatrixGame/src/MatrixProgressBar.cpp',('CMatrixProgressBar::m_First','CMatrixProgressBar::m_Last','CMatrixProgressBar::m_FirstClones','CMatrixProgressBar::m_LastClones','CMatrixProgressBar::m_Tex')),
 ('MatrixGame/src/MatrixFlyer.cpp',('CMatrixFlyer::m_VB','CMatrixFlyer::m_VB_ref')),
 ('MatrixGame/src/MatrixWater.cpp',('SInshorewave::m_Tex','SInshorewave::m_VB','SInshorewave::m_VB_ref')),
 ('MatrixGame/src/MatrixMapGroup.cpp',('CMatrixMapGroup::m_BigVB_bottom','CMatrixMapGroup::m_BigIB_bottom')),
 ('MatrixGame/src/MatrixMapTexture.cpp',('CBottomTextureUnion::m_Textures','CBottomTextureUnion::m_TexturesCount')),
 ('MatrixGame/src/MatrixInstantDraw.cpp',('CInstDraw::m_FVFs','CInstDraw::m_IB','CInstDraw::m_IB_Count')),
 ('MatrixGame/src/MatrixObject.cpp',('CMatrixMapObject::m_ShadowTextures','CMatrixMapObject::m_ShadowTexturesCount')),
 ('MatrixGame/src/MatrixTerSurface.cpp',('CTerSurface::m_BigIB','CTerSurface::m_BigVB','CTerSurface::m_BigVBM','CTerSurface::m_Surfaces','CTerSurface::m_SurfacesCnt')),
 ('MatrixLib/3G/ShadowStencil.cpp',('CVOShadowStencil::m_First','CVOShadowStencil::m_Last')),
 ('MatrixLib/3G/ShadowProj.cpp',('CVOShadowProj::m_VB','CVOShadowProj::m_IB')),
 ('MatrixGame/src/MatrixSkinManager.cpp',('CSkinManager::m_Skins','CSkinManager::m_SkinsCount')),
 ('MatrixGame/src/Interface/CIFaceMenu.cpp',('CIFaceMenu::m_MenuGraphics','g_PopupMenu')),
 ('MatrixGame/src/MatrixMultiSelection.cpp',('CMultiSelection::m_First','CMultiSelection::m_Last','CMultiSelection::m_Time','CMultiSelection::m_GameSelection')),
)

def prepare_static_resources(output):
    changed={};statements=[]
    def write(name,text):
        path=output/name;path.write_bytes(text.encode())
        changed[name]=hashlib.sha256(path.read_bytes()).hexdigest()
    game_name='MatrixGame/src/MatrixGame.cpp';game=(output/game_name).read_text(encoding='utf-8')
    for name,tokens in DEFINITIONS:
        text=(output/name).read_text(encoding='utf-8')
        for token in tokens:
            matches=list(re.finditer(r'^([^\n;{}()#]+?)\b'+re.escape(token)+r'\b[^\n;]*;',text,re.M))
            matches=[m for m in matches if '(' not in m.group() and not m.group().startswith((' ','\t','//','extern'))]
            if len(matches)!=1:raise ValueError('Original static definition changed: '+token)
            original=matches[0].group()
            statements.append('// Original '+name+'\n'+original)
            if '::' in token:replacement='// Definition moved intact to GameStaticResources.cpp: '+token
            else:replacement='extern '+original.split('=')[0].strip().rstrip(';')+';'
            text=text.replace(original,replacement,1)
        write(name,text)
    includes='\n'.join(re.findall(r'^#include[^\n]+',game[:game.index('static void static_init(void)')],re.M))
    write('MatrixGame/src/GameStaticResources.cpp',
        '// GPL-2.0-or-later. Genuine original definitions, one owner across TUs.\n'+includes+'\n#include "MatrixInstantDraw.hpp"\n'+'\n'.join(statements)+'\n')
    write(game_name,game+'''
// Native owner invokes the unchanged original production static-init stage.
namespace srhd_awa::matrixgame {
void InitializeOriginalGameStatics() {static_init();}
}
''')
    return changed
