"""Keep full original loading-progress algorithms and close published-pointer lifetime."""
import hashlib

def prepare_load_progress(output):
    changed={}
    def write(name,text):
        path=output/name;path.write_bytes(text.encode())
        changed[name]=hashlib.sha256(path.read_bytes()).hexdigest()
    name='MatrixGame/src/MatrixLoadProgress.hpp';h=(output/name).read_text(encoding='utf-8')
    if h.count('~CLoadProgress(void) {}')!=1:raise ValueError('Original progress destructor changed')
    h=h.replace('class CLoadProgress : public CMain','class CLoadProgress : public Base::CMain')
    h=h.replace('~CLoadProgress(void) {}','~CLoadProgress(void);')
    write(name,h)
    name='MatrixGame/src/MatrixLoadProgress.cpp';c=(output/name).read_text(encoding='utf-8')
    c='#include "MatrixGame.h"\n'+c+'''
// CGame::Init publishes its stack-local progress object. Unwind/return must
// invalidate that borrowed pointer, while preserving a different publisher.
CLoadProgress::~CLoadProgress(void) {
    if(g_LoadProgress==this)g_LoadProgress=nullptr;
}
'''
    write(name,c)
    return changed
