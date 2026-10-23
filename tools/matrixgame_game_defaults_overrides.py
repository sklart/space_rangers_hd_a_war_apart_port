"""Original configuration stage from CGame::Init, called by the shared owner."""
import hashlib
import re
from matrixgame_vector_object_overrides import function

def prepare_game_defaults(output):
    source=(output/'MatrixGame/src/MatrixConfig.cpp').read_text(encoding='utf-8')
    defaults=function(source,'void CMatrixConfig::SetDefaults(void)')
    clear=function(source,'void CMatrixConfig::Clear(void)')
    # These are original serialized key identifiers, not native input functions.
    keys={'VK_HOME':0x24,'VK_END':0x23,'VK_PRIOR':0x21,'VK_NEXT':0x22,'VK_UP':0x26,'VK_DOWN':0x28,
        'VK_LEFT':0x25,'VK_RIGHT':0x27,'VK_LBUTTON':1,'VK_RBUTTON':2,'VK_SHIFT':0x10,'VK_CONTROL':0x11,
        'VK_OEM_4':0xdb,'VK_OEM_6':0xdd,'VK_OEM_PLUS':0xbb,'VK_OEM_MINUS':0xbd,'VK_OEM_5':0xdc,
        'VK_OEM_COMMA':0xbc,'VK_OEM_PERIOD':0xbe,'VK_RETURN':0x0d,'VK_SPACE':0x20}
    for old,new in keys.items():defaults=re.sub(r'\b'+old+r'\b',hex(new),defaults)
    if re.search(r'\bVK_\w+',defaults):raise ValueError('Unmapped original default key')
    # Real original statics, no replacement config class or fake settings.
    if 'CMatrixConfig g_Config;' not in source:raise ValueError('Config owner declaration changed')
    effect=(output/'MatrixGame/src/Effects/MatrixEffectKonus.cpp').read_text(encoding='utf-8')
    statics='\n'.join(line for line in effect.splitlines()if re.match(r'^(?:::srhd_awa::matrixgame::buffers::VertexBuffer\*|int) CMatrixEffectKonus::m_VB(?:_ref)?;',line))
    if len(statics.splitlines())!=2:raise ValueError('Original effect buffer statics changed')
    data=('''// Original MatrixConfig.cpp methods and resource statics; GPL-2.0-or-later.
// Selected methods, not the full config/effect TU. ReadParams is not provided.
#include "MatrixConfig.hpp"
#include "Effects/MatrixEffectKonus.hpp"
#include "game_defaults.hpp"
#include "3g.hpp"
int g_MaxFPS;
CMatrixConfig g_Config;
'''+statics+'\n'+defaults+'\n'+clear+'''
namespace srhd_awa::matrixgame {
void InitializeOriginalGameDefaults() {
    if(g_Config.m_Cursors)throw std::logic_error("existing configuration resources require cleanup");
    g_Config.SetDefaults();
}
void ClearOriginalGameDefaults() {
    g_Config.Clear();
    // Original Clear frees the array but leaves its pointer published. The
    // shared owner invalidates it before a second Deinit/reentry can see it.
    g_Config.m_Cursors=nullptr;g_Config.m_CursorsCnt=0;
}
}
''').encode()
    name='MatrixGame/src/GameDefaultsNative.cpp';(output/name).write_bytes(data)
    return {name:hashlib.sha256(data).hexdigest()}
