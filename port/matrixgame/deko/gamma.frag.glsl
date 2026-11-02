// SPDX-License-Identifier: GPL-2.0-or-later
#version 460
layout(location=0) in vec2 uv;
layout(binding=0) uniform sampler2D sourceImage;
layout(std140,binding=0) uniform Gamma {vec4 entries[256];} lut;
layout(location=0) out vec4 outputColor;
void main(){
    ivec2 size=textureSize(sourceImage,0);
    vec4 source=texelFetch(sourceImage,clamp(ivec2(uv*vec2(size)),ivec2(0),size-1),0);
    vec3 x=clamp(source.rgb,vec3(0),vec3(1))*255.0;
    ivec3 lo=ivec3(x),hi=min(lo+1,ivec3(255));
    vec3 low=vec3(lut.entries[lo.r].r,lut.entries[lo.g].g,lut.entries[lo.b].b);
    vec3 high=vec3(lut.entries[hi.r].r,lut.entries[hi.g].g,lut.entries[hi.b].b);
    outputColor=vec4(mix(low,high,x-vec3(lo)),source.a);
}
