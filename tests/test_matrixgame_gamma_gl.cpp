// SPDX-License-Identifier: GPL-2.0-or-later
// Runs the exact presentation GLSL on local Mesa/EGL, never a Switch shader.
#define GL_GLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glcorearb.h>
#include "native_gamma.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
namespace p=srhd_awa::matrixgame::platform3g;
std::string Read(const char* path){std::ifstream f(path);assert(f);return {std::istreambuf_iterator<char>(f),{}};}
GLuint Shader(GLenum stage,const char* path){const auto s=Read(path);const auto* ptr=s.c_str();const auto shader=glCreateShader(stage);glShaderSource(shader,1,&ptr,nullptr);glCompileShader(shader);GLint ok;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);if(!ok){char log[8192];glGetShaderInfoLog(shader,sizeof(log),nullptr,log);std::puts(log);}assert(ok);return shader;}
int main(int argc,char** argv){
    assert(argc==4);auto platform=reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));assert(platform);
    auto display=platform(EGL_PLATFORM_SURFACELESS_MESA,EGL_DEFAULT_DISPLAY,nullptr);EGLint major,minor;assert(eglInitialize(display,&major,&minor));assert(eglBindAPI(EGL_OPENGL_API));
    const EGLint attrs[]{EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_NONE};
    EGLConfig cfg;EGLint count;assert(eglChooseConfig(display,attrs,&cfg,1,&count) && count);
    const EGLint ctxattrs[]{EGL_CONTEXT_MAJOR_VERSION,4,EGL_CONTEXT_MINOR_VERSION,5,EGL_CONTEXT_OPENGL_PROFILE_MASK,EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,EGL_NONE};
    auto context=eglCreateContext(display,cfg,EGL_NO_CONTEXT,ctxattrs);assert(context!=EGL_NO_CONTEXT);assert(eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,context));
    std::printf("GL_RENDERER=%s GL_VERSION=%s\n",glGetString(GL_RENDERER),glGetString(GL_VERSION));
    const auto vertex=Shader(GL_VERTEX_SHADER,argv[1]),fragment=Shader(GL_FRAGMENT_SHADER,argv[2]);const auto program=glCreateProgram();glAttachShader(program,vertex);glAttachShader(program,fragment);glLinkProgram(program);GLint ok;glGetProgramiv(program,GL_LINK_STATUS,&ok);assert(ok);glUseProgram(program);
    constexpr unsigned W=256,H=13;std::vector<unsigned char> source(W*H*4),output(source.size()),unchanged(source.size());
    for(unsigned y=0;y<H;++y)for(unsigned x=0;x<W;++x){const auto i=(y*W+x)*4;source[i]=x;source[i+1]=(x+17*y)&255;source[i+2]=(255-x+5*y)&255;source[i+3]=(x*13+y*7)&255;}
    GLuint texture,target,fbo,vao,ubo;glGenTextures(1,&texture);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,texture);glTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA8,W,H);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,source.data());glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glGenTextures(1,&target);glBindTexture(GL_TEXTURE_2D,target);glTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA8,W,H);glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,target,0);assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);glBindTexture(GL_TEXTURE_2D,texture);
    glGenVertexArrays(1,&vao);glBindVertexArray(vao);glGenBuffers(1,&ubo);glBindBuffer(GL_UNIFORM_BUFFER,ubo);glBufferData(GL_UNIFORM_BUFFER,sizeof(p::GammaUniforms),nullptr,GL_DYNAMIC_DRAW);glBindBufferBase(GL_UNIFORM_BUFFER,0,ubo);glViewport(0,0,W,H);glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);glDisable(GL_STENCIL_TEST);glDisable(GL_DITHER);glDisable(GL_FRAMEBUFFER_SRGB);
    std::ifstream data(argv[3],std::ios::binary);assert(data);p::GammaRamp ramp;unsigned ramps=0;std::uint64_t pixels=0;
    while(data.read(reinterpret_cast<char*>(&ramp),sizeof(ramp))){
        const auto uniforms=p::PrepareGamma(ramp);glBufferSubData(GL_UNIFORM_BUFFER,0,sizeof(uniforms),&uniforms);glDrawArrays(GL_TRIANGLES,0,3);glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,output.data());assert(glGetError()==GL_NO_ERROR);
        const std::array<const std::array<std::uint16_t,256>*,3> tables{&ramp.red,&ramp.green,&ramp.blue};
        for(std::size_t i=0;i<source.size();i+=4){for(unsigned c=0;c<3;++c){const auto expected=(unsigned((*tables[c])[source[i+c]])*255u+32767u)/65535u;assert(std::abs(int(output[i+c])-int(expected))<=1);}assert(output[i+3]==source[i+3]);++pixels;}
        glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,unchanged.data());assert(source==unchanged);++ramps;
    }
    assert(data.eof() && ramps==21);std::printf("EXACT production GLSL ramps=%u pixels=%llu alpha/source/framebuffer orientation PASS; Mesa only SWITCH_NOT_RUN\n",ramps,static_cast<unsigned long long>(pixels));
    glDeleteBuffers(1,&ubo);glDeleteVertexArrays(1,&vao);glDeleteFramebuffers(1,&fbo);glDeleteTextures(1,&target);glDeleteTextures(1,&texture);glDeleteProgram(program);glDeleteShader(vertex);glDeleteShader(fragment);eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroyContext(display,context);eglTerminate(display);
}
