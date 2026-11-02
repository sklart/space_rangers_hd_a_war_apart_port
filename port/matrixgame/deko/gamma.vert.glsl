// SPDX-License-Identifier: GPL-2.0-or-later
#version 460
layout(location=0) out vec2 uv;
void main(){
    vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);
    uv=p;
    // The device uses OriginUpperLeft: texel row 0 must reach target row 0.
    gl_Position=vec4(p.x*2.0-1.0,1.0-p.y*2.0,0.0,1.0);
}
