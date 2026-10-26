// Retain original map font loss/recreation in the full SDK entry, after real Init.
#include "MatrixGame.h"
#include "MatrixLogic.hpp"
#include <stdexcept>
void VerifyOriginalNativeDebugFontCycle(){
    if(!g_MatrixMap)throw std::logic_error("original map required for font recreation");
    g_MatrixMap->m_DI.OnLostDevice();
    g_MatrixMap->m_DI.OnResetDevice();
}
