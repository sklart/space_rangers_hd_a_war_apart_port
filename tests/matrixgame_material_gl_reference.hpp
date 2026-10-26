// SPDX-License-Identifier: GPL-2.0-or-later
// Host GPU execution of the same GLSL. Never a Switch hardware substitute.
#pragma once
#include <GL/gl.h>
#include "alpha_target.hpp"
#include "pixel_fog.hpp"
#include "legacy_raster_viewport.hpp"
#include "stencil_state.hpp"
#include <fstream>
#include <iterator>
#include <string>
#include <cmath>
#include <filesystem>
struct MaterialMeshOptions {
    srhd_awa::matrixgame::MeshStencilState stencil;
    unsigned shade{2},cull{1},primitive{4};bool preserve{},last_pixel{true};
    float clear_depth{.5f};unsigned clear_stencil{};
};
class MaterialGlReference {
    HWND window_{};HDC dc_{};HGLRC context_{};
    unsigned program_{},ubo_{},fbo_{},target_{},textures_[8]{};
    unsigned vertex_program_{},vertex_buffer_{},feedback_buffer_{};std::filesystem::path vertex_path_;
    unsigned fog_buffer_{},mesh_program_{},mesh_buffer_{};std::filesystem::path fragment_path_;
    unsigned raster_buffer_{},depth_stencil_{},target_width_{1},target_height_{1},depth_width_{},depth_height_{};
    static PROC Proc(const char* name){
        auto p=wglGetProcAddress(name);
        if(!p || std::uintptr_t(p)<=3 || std::uintptr_t(p)==std::uintptr_t(-1))throw std::runtime_error(std::string("OpenGL function unavailable: ")+name);
        return p;
    }
#define MG_GL(ret,name,...) using name##Type=ret(APIENTRY*)(__VA_ARGS__);name##Type name=reinterpret_cast<name##Type>(Proc(#name))
    MG_GL(unsigned,glCreateShader,unsigned);MG_GL(void,glShaderSource,unsigned,int,const char* const*,const int*);
    MG_GL(void,glCompileShader,unsigned);MG_GL(void,glGetShaderiv,unsigned,unsigned,int*);
    MG_GL(void,glGetShaderInfoLog,unsigned,int,int*,char*);MG_GL(void,glDeleteShader,unsigned);
    MG_GL(unsigned,glCreateProgram);MG_GL(void,glAttachShader,unsigned,unsigned);MG_GL(void,glLinkProgram,unsigned);
    MG_GL(void,glGetProgramiv,unsigned,unsigned,int*);MG_GL(void,glGetProgramInfoLog,unsigned,int,int*,char*);
    MG_GL(void,glBlendEquation,unsigned);
    MG_GL(void,glUseProgram,unsigned);MG_GL(void,glDeleteProgram,unsigned);
    MG_GL(int,glGetUniformLocation,unsigned,const char*);MG_GL(void,glUniform4fv,int,int,const float*);
    MG_GL(void,glGenBuffers,int,unsigned*);MG_GL(void,glBindBuffer,unsigned,unsigned);
    MG_GL(void,glBufferData,unsigned,std::ptrdiff_t,const void*,unsigned);MG_GL(void,glBindBufferBase,unsigned,unsigned,unsigned);
    MG_GL(void,glDeleteBuffers,int,const unsigned*);MG_GL(void,glActiveTexture,unsigned);
    MG_GL(void,glGenFramebuffers,int,unsigned*);MG_GL(void,glBindFramebuffer,unsigned,unsigned);
    MG_GL(void,glFramebufferTexture2D,unsigned,unsigned,unsigned,unsigned,int);
    MG_GL(unsigned,glCheckFramebufferStatus,unsigned);MG_GL(void,glDeleteFramebuffers,int,const unsigned*);
    MG_GL(void,glTransformFeedbackVaryings,unsigned,int,const char* const*,unsigned);
    MG_GL(void,glBeginTransformFeedback,unsigned);MG_GL(void,glEndTransformFeedback);
    MG_GL(void,glGetBufferSubData,unsigned,std::ptrdiff_t,std::ptrdiff_t,void*);
    MG_GL(void,glVertexAttribPointer,unsigned,int,unsigned,unsigned char,int,const void*);
    MG_GL(void,glEnableVertexAttribArray,unsigned);MG_GL(void,glDisableVertexAttribArray,unsigned);
    MG_GL(void,glClipControl,unsigned,unsigned);MG_GL(void,glViewportIndexedf,unsigned,float,float,float,float);
    MG_GL(void,glGenRenderbuffers,int,unsigned*);MG_GL(void,glBindRenderbuffer,unsigned,unsigned);
    MG_GL(void,glRenderbufferStorage,unsigned,unsigned,int,int);MG_GL(void,glFramebufferRenderbuffer,unsigned,unsigned,unsigned,unsigned);
    MG_GL(void,glDeleteRenderbuffers,int,const unsigned*);MG_GL(void,glStencilFuncSeparate,unsigned,unsigned,int,unsigned);
    MG_GL(void,glStencilOpSeparate,unsigned,unsigned,unsigned,unsigned);MG_GL(void,glStencilMaskSeparate,unsigned,unsigned);
    MG_GL(void,glProvokingVertex,unsigned);
#undef MG_GL
    unsigned Shader(unsigned stage,const std::string& source){
        unsigned shader=glCreateShader(stage);const char* text=source.c_str();glShaderSource(shader,1,&text,nullptr);glCompileShader(shader);
        int ok{};glGetShaderiv(shader,0x8b81,&ok);
        if(!ok){char error[4096]{};glGetShaderInfoLog(shader,sizeof(error),nullptr,error);glDeleteShader(shader);throw std::runtime_error(error);}return shader;
    }
    static HGLRC Open(HWND& window,HDC& dc){
        window=CreateWindowW(L"STATIC",L"MatrixGame GLSL reference",WS_OVERLAPPEDWINDOW,0,0,32,32,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        if(!window)throw std::runtime_error("GL reference window failed");dc=GetDC(window);
        PIXELFORMATDESCRIPTOR pf{};pf.nSize=sizeof(pf);pf.nVersion=1;pf.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL;pf.iPixelType=PFD_TYPE_RGBA;pf.cColorBits=32;
        int index=ChoosePixelFormat(dc,&pf);if(!index || !SetPixelFormat(dc,index,&pf))throw std::runtime_error("GL reference pixel format failed");
        auto context=wglCreateContext(dc);if(!context || !wglMakeCurrent(dc,context))throw std::runtime_error("GL reference context failed");return context;
    }
public:
    explicit MaterialGlReference(const char* fragment_path):context_(Open(window_,dc_)){
        fragment_path_=fragment_path;
        vertex_path_=std::filesystem::path(fragment_path).parent_path()/"material.vert.glsl";
        std::ifstream input(fragment_path);if(!input)throw std::runtime_error("material GLSL source unavailable");
        const std::string fragment{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
        const std::string vertex=R"(#version 460
layout(location=0) out vec4 diffuse;
layout(location=1) out vec2 texcoord[8];
layout(location=9) flat out vec4 flatDiffuse;
uniform vec4 vertexDiffuse;
void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2-1,0,1);diffuse=vertexDiffuse;flatDiffuse=vertexDiffuse;for(int i=0;i<8;++i)texcoord[i]=vec2(.5);}
)";
        auto vs=Shader(0x8b31,vertex),fs=Shader(0x8b30,fragment);
        program_=glCreateProgram();glAttachShader(program_,vs);glAttachShader(program_,fs);glLinkProgram(program_);glDeleteShader(vs);glDeleteShader(fs);
        int ok{};glGetProgramiv(program_,0x8b82,&ok);if(!ok){char error[4096]{};glGetProgramInfoLog(program_,sizeof(error),nullptr,error);throw std::runtime_error(error);}
        glGenBuffers(1,&ubo_);glGenBuffers(1,&fog_buffer_);
        const mat::PixelFogUniforms disabled;glBindBuffer(0x8a11,fog_buffer_);glBufferData(0x8a11,sizeof(disabled),&disabled,0x88e0);glBindBufferBase(0x8a11,1,fog_buffer_);
        glGenBuffers(1,&raster_buffer_);const std::array<unsigned,4> raster{2,0,0,0};glBindBuffer(0x8a11,raster_buffer_);glBufferData(0x8a11,sizeof(raster),raster.data(),0x88e0);glBindBufferBase(0x8a11,2,raster_buffer_);
        glGenFramebuffers(1,&fbo_);glBindFramebuffer(0x8d40,fbo_);
        glGenTextures(1,&target_);glBindTexture(GL_TEXTURE_2D,target_);glTexImage2D(GL_TEXTURE_2D,0,0x8058,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        glFramebufferTexture2D(0x8d40,0x8ce0,GL_TEXTURE_2D,target_,0);
        if(glCheckFramebufferStatus(0x8d40)!=0x8cd5)throw std::runtime_error("GL reference framebuffer incomplete");
        glGenTextures(8,textures_);glViewport(0,0,1,1);glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);
        std::printf("GLSL reference GPU=%s version=%s\n",glGetString(GL_RENDERER),glGetString(GL_VERSION));
    }
    ~MaterialGlReference(){
        glDeleteBuffers(1,&raster_buffer_);glDeleteRenderbuffers(1,&depth_stencil_);
        glDeleteProgram(mesh_program_);glDeleteBuffers(1,&mesh_buffer_);glDeleteBuffers(1,&fog_buffer_);
        glDeleteProgram(vertex_program_);glDeleteBuffers(1,&vertex_buffer_);glDeleteBuffers(1,&feedback_buffer_);
        glDeleteTextures(8,textures_);glDeleteTextures(1,&target_);glDeleteFramebuffers(1,&fbo_);glDeleteBuffers(1,&ubo_);glDeleteProgram(program_);
        wglMakeCurrent(nullptr,nullptr);wglDeleteContext(context_);ReleaseDC(window_,dc_);DestroyWindow(window_);
    }
    mat::VertexResult Transform(const mat::VertexUniforms& u,const mat::Vertex& vertex){
        static_assert(sizeof(mat::VertexResult)==96);
        if(!vertex_program_){
            std::ifstream input(vertex_path_);if(!input)throw std::runtime_error("material vertex GLSL unavailable");
            std::string source{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};auto vs=Shader(0x8b31,source);
            vertex_program_=glCreateProgram();glAttachShader(vertex_program_,vs);
            const char* outputs[]{"gl_Position","diffuse","texcoord"};glTransformFeedbackVaryings(vertex_program_,3,outputs,0x8c8c);glLinkProgram(vertex_program_);glDeleteShader(vs);
            int ok{};glGetProgramiv(vertex_program_,0x8b82,&ok);if(!ok){char error[4096]{};glGetProgramInfoLog(vertex_program_,sizeof(error),nullptr,error);throw std::runtime_error(error);}
            glGenBuffers(1,&vertex_buffer_);glGenBuffers(1,&feedback_buffer_);
        }
        glUseProgram(vertex_program_);glBindBuffer(0x8a11,ubo_);glBufferData(0x8a11,sizeof(u),&u,0x88e0);glBindBufferBase(0x8a11,0,ubo_);
        glBindBuffer(0x8892,vertex_buffer_);glBufferData(0x8892,sizeof(vertex),&vertex,0x88e0);
        for(unsigned i=0;i<12;++i){glEnableVertexAttribArray(i);glVertexAttribPointer(i,4,GL_FLOAT,FALSE,sizeof(vertex),reinterpret_cast<const void*>(std::uintptr_t(i*16)));}
        glBindBuffer(0x8c8e,feedback_buffer_);glBufferData(0x8c8e,sizeof(mat::VertexResult),nullptr,0x88e0);glBindBufferBase(0x8c8e,0,feedback_buffer_);
        glEnable(0x8c89);glBeginTransformFeedback(GL_POINTS);glDrawArrays(GL_POINTS,0,1);glEndTransformFeedback();glDisable(0x8c89);
        mat::VertexResult output;glGetBufferSubData(0x8c8e,0,sizeof(output),&output);
        for(unsigned i=0;i<12;++i)glDisableVertexAttribArray(i);
        if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("fixed vertex transform feedback GL error");
        return output;
    }
    // Both production shader stages, real geometry/interpolation and framebuffer.
    // No full-screen vertex substitute in the fog oracle.
    std::vector<unsigned> DrawMesh(const mat::VertexUniforms& vertex,const mat::Uniforms& material,const mat::PixelFogUniforms& fog,std::span<const mat::Vertex> vertices,const srhd_awa::matrixgame::MeshRasterState& raster,unsigned width=16,unsigned height=16,const MaterialMeshOptions& options={}){
        if(!mesh_program_){
            auto load=[](const std::filesystem::path& p){std::ifstream in(p);if(!in)throw std::runtime_error("mesh shader source unavailable");return std::string{std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};};
            auto vs=Shader(0x8b31,load(vertex_path_)),fs=Shader(0x8b30,load(fragment_path_));mesh_program_=glCreateProgram();glAttachShader(mesh_program_,vs);glAttachShader(mesh_program_,fs);glLinkProgram(mesh_program_);glDeleteShader(vs);glDeleteShader(fs);
            int ok{};glGetProgramiv(mesh_program_,0x8b82,&ok);if(!ok){char error[4096]{};glGetProgramInfoLog(mesh_program_,sizeof(error),nullptr,error);throw std::runtime_error(error);}glGenBuffers(1,&mesh_buffer_);
        }
        // OpenGL's one namespace of UBO bindings corresponds to two separate
        // vertex/fragment namespaces on deko. Assign fragment Material to 3.
        auto blockIndex=reinterpret_cast<unsigned(APIENTRY*)(unsigned,const char*)>(Proc("glGetUniformBlockIndex"));
        auto blockBinding=reinterpret_cast<void(APIENTRY*)(unsigned,unsigned,unsigned)>(Proc("glUniformBlockBinding"));
        blockBinding(mesh_program_,blockIndex(mesh_program_,"Material"),3);
        unsigned vertex_ubo{};glGenBuffers(1,&vertex_ubo);
        glBindBuffer(0x8a11,vertex_ubo);glBufferData(0x8a11,sizeof(vertex),&vertex,0x88e0);glBindBufferBase(0x8a11,0,vertex_ubo);
        glBindBuffer(0x8a11,ubo_);glBufferData(0x8a11,sizeof(material),&material,0x88e0);glBindBufferBase(0x8a11,3,ubo_);
        const std::array<unsigned,4> shade{options.shade,0,0,0};glBindBuffer(0x8a11,raster_buffer_);glBufferData(0x8a11,sizeof(shade),shade.data(),0x88e0);glBindBufferBase(0x8a11,2,raster_buffer_);
        glBindBuffer(0x8a11,fog_buffer_);glBufferData(0x8a11,sizeof(fog),&fog,0x88e0);glBindBufferBase(0x8a11,1,fog_buffer_);
        glBindFramebuffer(0x8d40,fbo_);glActiveTexture(0x84c0);glBindTexture(GL_TEXTURE_2D,target_);
        if(target_width_!=width || target_height_!=height){glTexImage2D(GL_TEXTURE_2D,0,0x8058,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);target_width_=width;target_height_=height;}
        if(!depth_stencil_)glGenRenderbuffers(1,&depth_stencil_);
        if(depth_width_!=width || depth_height_!=height){glBindRenderbuffer(0x8d41,depth_stencil_);glRenderbufferStorage(0x8d41,0x88f0,width,height);glFramebufferRenderbuffer(0x8d40,0x821a,0x8d41,depth_stencil_);depth_width_=width;depth_height_=height;}
        if(glCheckFramebufferStatus(0x8d40)!=0x8cd5)throw std::runtime_error("mesh framebuffer incomplete");
        const auto vp=srhd_awa::matrixgame::LegacyRasterViewport(vertex.viewport);
        // Match the native device's UpperLeft + ZeroToOne convention, including
        // edge ownership. LowerLeft plus flipped readback is insufficient at ties.
        glClipControl(0x8ca2,0x935f);glViewportIndexedf(0,vp[0],vp[1],vp[2],vp[3]);
        glDisable(GL_CULL_FACE);glDisable(GL_DEPTH_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
          glDisable(GL_BLEND);glDisable(GL_ALPHA_TEST);glDisable(GL_SCISSOR_TEST);glDepthMask(GL_TRUE);glStencilMaskSeparate(GL_FRONT_AND_BACK,255);
        if(!options.preserve){glClearColor(73.f/255,39.f/255,149.f/255,112.f/255);glClearDepth(options.clear_depth);glClearStencil(options.clear_stencil);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);}
          const auto clip=srhd_awa::matrixgame::LegacyRasterClip(vertex.viewport);
          glEnable(GL_SCISSOR_TEST);glScissor(int(clip[0]),int(clip[1]),int(clip[2]),int(clip[3]));
        glProvokingVertex(0x8e4d); // FIRST_VERTEX_CONVENTION, including strips/fans.
        glFrontFace(GL_CW); // glClipControl adjusts area sign for UpperLeft.
        if(options.cull!=1){glEnable(GL_CULL_FACE);glCullFace(options.cull==2?GL_FRONT:GL_BACK);}
        if(raster.depth_test)glEnable(GL_DEPTH_TEST);glDepthFunc(GL_NEVER+unsigned(raster.depth_compare)-1);glDepthMask(raster.depth_write);
        if(options.stencil.enabled)glEnable(GL_STENCIL_TEST);else glDisable(GL_STENCIL_TEST);
        const unsigned stencil_ops[]{0,GL_KEEP,GL_ZERO,GL_REPLACE,GL_INCR,GL_DECR,GL_INVERT,0x8507,0x8508};
        for(unsigned side=0;side<2;++side){const auto& f=options.stencil.Face(side==0);const auto face=side==0?GL_FRONT:GL_BACK;
            glStencilFuncSeparate(face,GL_NEVER+f.compare-1,options.stencil.Reference8(),options.stencil.read_mask&255);
            glStencilMaskSeparate(face,options.stencil.write_mask&255);glStencilOpSeparate(face,stencil_ops[f.fail],stencil_ops[f.depth_fail],stencil_ops[f.pass]);}
        glColorMask(raster.color_write&1,raster.color_write&2,raster.color_write&4,raster.color_write&8);
        if(raster.blend_enable)glEnable(GL_BLEND);
        const unsigned factors[]{0,GL_ZERO,GL_ONE,GL_SRC_COLOR,GL_ONE_MINUS_SRC_COLOR,GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_DST_ALPHA,GL_ONE_MINUS_DST_ALPHA,GL_DST_COLOR,GL_ONE_MINUS_DST_COLOR,GL_SRC_ALPHA_SATURATE};
        const unsigned ops[]{0,0x8006,0x800a,0x800b,0x8007,0x8008};glBlendFunc(factors[unsigned(raster.source)],factors[unsigned(raster.destination)]);glBlendEquation(ops[unsigned(raster.operation)]);
        glAlphaFunc(GL_NEVER+unsigned(raster.alpha_compare)-1,float(raster.alpha_reference)/255);if(raster.alpha_test)glEnable(GL_ALPHA_TEST);
        glUseProgram(mesh_program_);glBindBuffer(0x8892,mesh_buffer_);glBufferData(0x8892,vertices.size_bytes(),vertices.data(),0x88e0);
        for(unsigned i=0;i<12;++i){glEnableVertexAttribArray(i);glVertexAttribPointer(i,4,GL_FLOAT,FALSE,sizeof(mat::Vertex),reinterpret_cast<const void*>(std::uintptr_t(i*16)));}
        if(options.primitive==2 || options.primitive==3){glLineWidth(1);glDisable(GL_LINE_SMOOTH);glDisable(GL_LINE_STIPPLE);}
        glDrawArrays(options.primitive==1?GL_POINTS:options.primitive==2?GL_LINES:options.primitive==3?GL_LINE_STRIP:options.primitive==4?GL_TRIANGLES:options.primitive==5?GL_TRIANGLE_STRIP:GL_TRIANGLE_FAN,0,int(vertices.size()));std::vector<unsigned> bottom(width*height);glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,bottom.data());
        for(unsigned i=0;i<12;++i)glDisableVertexAttribArray(i);
        std::vector<unsigned> out(width*height);for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){auto rgba=bottom[y*width+x];out[y*width+x]=(rgba&0xff00ff00u)|((rgba&255)<<16)|((rgba>>16)&255);}
        glDeleteBuffers(1,&vertex_ubo);glDisable(GL_ALPHA_TEST);glDisable(GL_BLEND);glDisable(GL_SCISSOR_TEST);glBlendEquation(0x8006);glClipControl(0x8ca1,0x935e);
        glDisable(GL_STENCIL_TEST);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDepthMask(GL_TRUE);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
        const mat::PixelFogUniforms disabled;glBindBuffer(0x8a11,fog_buffer_);glBufferData(0x8a11,sizeof(disabled),&disabled,0x88e0);glBindBufferBase(0x8a11,1,fog_buffer_);
        if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("production mesh/fog GL error");return out;
    }
    std::vector<unsigned char> ReadStencil(unsigned width=16,unsigned height=16){
        std::vector<unsigned char> bottom(width*height),out(width*height);glReadPixels(0,0,width,height,GL_STENCIL_INDEX,GL_UNSIGNED_BYTE,bottom.data());
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x)out[y*width+x]=bottom[y*width+x];
        if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("actual GL stencil readback failed");return out;
    }
    unsigned char DrawAlpha(mat::Uniforms u,mat::Color diffuse,const srhd_awa::matrixgame::MeshRasterState& logical,unsigned char initial){
        namespace m=srhd_awa::matrixgame;auto r=m::AlphaTargetRaster(logical);mat::AlphaTarget(u,true);
        glActiveTexture(0x84c0);glBindTexture(GL_TEXTURE_2D,target_);glTexImage2D(GL_TEXTURE_2D,0,0x8229,1,1,0,0x1903,GL_UNSIGNED_BYTE,&initial);
        if(glCheckFramebufferStatus(0x8d40)!=0x8cd5)throw std::runtime_error("R8 framebuffer incomplete");
        const unsigned factors[]{0,GL_ZERO,GL_ONE,GL_SRC_COLOR,GL_ONE_MINUS_SRC_COLOR,GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_DST_ALPHA,GL_ONE_MINUS_DST_ALPHA,GL_DST_COLOR,GL_ONE_MINUS_DST_COLOR,GL_SRC_ALPHA_SATURATE};
        const unsigned ops[]{0,0x8006,0x800a,0x800b,0x8007,0x8008};
        if(r.blend_enable)glEnable(GL_BLEND);else glDisable(GL_BLEND);glBlendFunc(factors[unsigned(r.source)],factors[unsigned(r.destination)]);glBlendEquation(ops[unsigned(r.operation)]);
        glColorMask(r.color_write&1,GL_FALSE,GL_FALSE,GL_FALSE);glAlphaFunc(GL_NEVER+unsigned(r.alpha_compare)-1,float(r.alpha_reference)/255);if(r.alpha_test)glEnable(GL_ALPHA_TEST);else glDisable(GL_ALPHA_TEST);
        auto color=Draw(u,{}, {},diffuse);unsigned char value=static_cast<unsigned char>(std::lround(color[0]*255));
        glDisable(GL_ALPHA_TEST);glDisable(GL_BLEND);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glBlendEquation(0x8006);
        glActiveTexture(0x84c0);glBindTexture(GL_TEXTURE_2D,target_);glTexImage2D(GL_TEXTURE_2D,0,0x8058,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);target_width_=target_height_=1;return value;
    }
    mat::Color Draw(const mat::Uniforms& u,const std::array<mat::Color,8>& samples,const std::array<bool,8>& bound,mat::Color diffuse){
        glUseProgram(program_);glUniform4fv(glGetUniformLocation(program_,"vertexDiffuse"),1,diffuse.data());
        glBindBuffer(0x8a11,ubo_);glBufferData(0x8a11,sizeof(u),&u,0x88e0);glBindBufferBase(0x8a11,0,ubo_);
        for(unsigned i=0;i<8;++i){glActiveTexture(0x84c0+i);glBindTexture(GL_TEXTURE_2D,bound[i]?textures_[i]:0);
            if(bound[i]){std::array<unsigned char,4> pixel;for(unsigned c=0;c<4;++c)pixel[c]=static_cast<unsigned char>(std::lround(samples[i][c]*255));
                glTexImage2D(GL_TEXTURE_2D,0,0x8058,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,pixel.data());glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);}
        }
        glDrawArrays(GL_TRIANGLES,0,3);std::array<unsigned char,4> pixel;glReadPixels(0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel.data());
        if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("material GLSL reference GL error");
        return {pixel[0]/255.f,pixel[1]/255.f,pixel[2]/255.f,pixel[3]/255.f};
    }
};
