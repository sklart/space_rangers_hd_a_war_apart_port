// SPDX-License-Identifier: GPL-2.0-or-later
#include "native_form_backend.hpp"
#include "MatrixFormGame.hpp"
#include "MatrixMap.hpp"
namespace srhd_awa::matrixgame::native_form {
Session::Session(){
    if(!g_MatrixMap || !g_MatrixHeap || g_FormCur || g_FormFirst || g_FormLast)
        throw std::logic_error("original native form requires ready game and empty form chain");
    form_=HNew(nullptr) CFormMatrixGame();
    try{FormChange(form_);}catch(...){
        if(g_FormCur==form_)g_FormCur=nullptr;
        HDelete(CFormMatrixGame,form_,nullptr);form_=nullptr;throw;
    }
}
Session::~Session(){
    if(g_FormCur==form_)FormChange(nullptr);
    HDelete(CFormMatrixGame,form_,nullptr);
}
}
