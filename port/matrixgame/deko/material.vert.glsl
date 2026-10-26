// SPDX-License-Identifier: GPL-2.0-or-later
#version 460
layout(location=0) in vec4 position;
layout(location=1) in vec4 normal;
layout(location=2) in vec4 color0;
layout(location=3) in vec4 color1;
layout(location=4) in vec4 inputUv[8];
layout(location=0) out vec4 diffuse;
layout(location=1) out vec2 texcoord[8];
layout(location=9) flat out vec4 flatDiffuse;
struct Light {vec4 direction;vec4 diffuse;vec4 ambient;};
layout(std140,binding=0) uniform VertexState {
    mat4 clipFromModel;mat4 cameraFromModel;mat4 normalFromModel;
    mat4 textureMatrix[8];uvec4 coordinates[8];
    vec4 materialDiffuse;vec4 materialAmbient;vec4 materialEmissive;vec4 globalAmbient;
    Light lights[8];uvec4 controls;uvec4 sources;vec4 viewport;
} u;
vec3 unitVector(vec3 v){float l=length(v);return l>0?v/l:vec3(0);}
vec4 materialSource(uint source,vec4 material){
    if(u.controls.z!=0u){if(source==1u && (u.controls.w&1u)!=0u)return color0;if(source==2u && (u.controls.w&2u)!=0u)return color1;}
    return material;
}
void main(){
    vec4 camera=u.cameraFromModel*vec4(position.xyz,1);
    vec3 n=mat3(u.normalFromModel)*normal.xyz;if(u.controls.y!=0u)n=unitVector(n);
    gl_Position=u.clipFromModel*vec4(position.xyz,1);
    if((u.controls.w&4u)!=0u){float w=1/position.w;vec2 ndc=(position.xy-u.viewport.xy)/u.viewport.zw*2-1;gl_Position=vec4(ndc.x*w,-ndc.y*w,position.z*w,w);}
    diffuse=color0;
    if(u.controls.x!=0u && (u.controls.w&4u)==0u){
        vec4 d=materialSource(u.sources.x,u.materialDiffuse),a=materialSource(u.sources.y,u.materialAmbient),e=materialSource(u.sources.z,u.materialEmissive);
        diffuse=vec4(e.rgb+a.rgb*u.globalAmbient.rgb,d.a);
        for(int i=0;i<8;++i)if(u.lights[i].direction.w!=0){float lambert=max(-dot(n,u.lights[i].direction.xyz),0);diffuse.rgb+=d.rgb*u.lights[i].diffuse.rgb*lambert+a.rgb*u.lights[i].ambient.rgb;}
        diffuse=clamp(diffuse,0,1);
    }
    flatDiffuse=diffuse;
    for(int i=0;i<8;++i){uint mode=u.coordinates[i].x&0xffff0000u,index=u.coordinates[i].x&0xffffu;
        vec4 t=inputUv[index];if(mode==0x10000u)t=vec4(n,1);else if(mode==0x20000u)t=camera;
        else if(mode==0x30000u){vec3 eye=unitVector(-camera.xyz);t=vec4(2*n*dot(n,eye)-eye,1);}
        if(u.coordinates[i].y!=0u){if(mode==0u && u.coordinates[index].z<=2u)t=vec4(t.xy,1,0);t=u.textureMatrix[i]*t;}
        texcoord[i]=t.xy;
    }
}
