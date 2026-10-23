"""Keep complete interface/shadow declarations while isolating concrete draw."""
import hashlib
from matrixgame_vector_object_overrides import function
def prepare_interface_headers(output):
    changed={}
    def write(name,data):
        (output/name).write_bytes(data);changed[name]=hashlib.sha256(data).hexdigest()
    name='MatrixGame/src/MatrixObjectRobot.hpp'
    data=(output/name).read_bytes()
    write(name,b'class CVOShadowProj; // Real implementation remains in ShadowProj.hpp.\n'+data)
    name='MatrixGame/src/MatrixMultiSelection.hpp';h=(output/name).read_text(encoding='utf-8')
    body=function(h,'static void DrawAll()')
    h=h.replace(body,'static void DrawAll();')
    write(name,h.encode())
    name='MatrixGame/src/MatrixMultiSelection.cpp';c=(output/name).read_text(encoding='utf-8')
    c+='\n// Original DrawAll body moved from the declaration header.\n'+body.replace('static void DrawAll()','void CMultiSelection::DrawAll()',1)+'\n'
    write(name,c.encode())
    name='MatrixLib/3G/ShadowProj.hpp';h=(output/name).read_text(encoding='utf-8')
    cpath=output/'MatrixLib/3G/ShadowProj.cpp';c=cpath.read_text(encoding='utf-8')
    h=h.replace('#include "Cache.hpp"','#include "Cache.hpp"\n#include "Texture.hpp"')
    for signature,definition in [('static void BeforeRenderAll(void)','void CVOShadowProj::BeforeRenderAll(void)'),
        ('    void Render(void)','void CVOShadowProj::Render(void)'),
        ('void RenderCustom(IDirect3DTexture9 *tex)','void CVOShadowProj::RenderCustom(IDirect3DTexture9 *tex)')]:
        body=function(h,signature);h=h.replace(body,signature+';')
        c+='\n// Original SDK-dependent body moved from ShadowProj.hpp.\n'+body.replace(signature,definition,1)+'\n'
    write(name,h.encode());write('MatrixLib/3G/ShadowProj.cpp',c.encode())
    name='MatrixGame/src/MatrixGame.cpp';c=(output/name).read_text(encoding='utf-8')
    c=c.replace('#include "ShadowStencil.hpp"','#include "ShadowStencil.hpp"\n#include "ShadowProj.hpp"\n#include "platform_3g.hpp"')
    c=c.replace('settings.FDirect3D,','reinterpret_cast<std::uintptr_t>(settings.FDirect3D),')
    c=c.replace('settings.FD3DDevice\n','reinterpret_cast<std::uintptr_t>(settings.FD3DDevice)\n')
    c=c.replace('ShowWindow(g_Wnd, SW_SHOWNORMAL);','srhd_awa::matrixgame::platform3g::ReadyWindow().Show(srhd_awa::matrixgame::platform3g::Visibility::Normal);')
    c=c.replace('UpdateWindow(g_Wnd);','srhd_awa::matrixgame::platform3g::ReadyWindow().Update();')
    write(name,c.encode())
    return changed
