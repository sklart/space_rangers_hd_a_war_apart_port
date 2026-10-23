"""Original configuration stage from CGame::Init, called by the shared owner."""
import hashlib
import re
from matrixgame_vector_object_overrides import function

def prepare_game_defaults(output):
    source=(output/'MatrixGame/src/MatrixConfig.cpp').read_text(encoding='utf-8')
    function(source,'void CMatrixConfig::SetDefaults(void)')
    function(source,'void CMatrixConfig::Clear(void)')
    # Real original statics, no replacement config class or fake settings.
    if 'CMatrixConfig g_Config;' not in source:raise ValueError('Config owner declaration changed')
    effect=(output/'MatrixGame/src/Effects/MatrixEffectKonus.cpp').read_text(encoding='utf-8')
    statics='\n'.join(line for line in effect.splitlines()if re.match(r'^(?:::srhd_awa::matrixgame::buffers::VertexBuffer\*|int) CMatrixEffectKonus::m_VB(?:_ref)?;',line))
    if len(statics.splitlines())!=2:raise ValueError('Original effect buffer statics changed')
    data=('''// Original MatrixConfig.cpp methods and resource statics; GPL-2.0-or-later.
// Full config methods are compiled from MatrixConfig.cpp, not duplicated here.
#include "MatrixConfig.hpp"
#include "Effects/MatrixEffectKonus.hpp"
#include "game_defaults.hpp"
#include "3g.hpp"
int g_MaxFPS;
'''+statics+'''
namespace srhd_awa::matrixgame {
void InitializeOriginalGameDefaults() {
    if(g_Config.m_Cursors || g_Config.m_Labels || g_Config.m_Descriptions)throw std::logic_error("existing configuration resources require cleanup");
    g_Config.SetDefaults();
}
void ClearOriginalGameDefaults() {
    g_Config.Clear();
}
}
''').encode()
    name='MatrixGame/src/GameDefaultsNative.cpp';(output/name).write_bytes(data)
    return {name:hashlib.sha256(data).hexdigest()}
