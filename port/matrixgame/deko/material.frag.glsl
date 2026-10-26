// SPDX-License-Identifier: GPL-2.0-or-later
#version 460
layout(location=0) in vec4 diffuse;
layout(location=1) in vec2 texcoord[8];
layout(location=9) flat in vec4 flatDiffuse;
layout(binding=0) uniform sampler2D images[8];
struct Stage {uvec4 color;uvec4 alpha;};
layout(std140,binding=0) uniform Material {Stage stages[8];vec4 factor;} u;
layout(std140,binding=1) uniform PixelFog {uvec4 controls;vec4 parameters;vec4 color;} fog;
layout(std140,binding=2) uniform RasterControls {uvec4 words;} raster;
vec4 vertexDiffuse;
layout(location=0) out vec4 outputColor;
vec4 argument(uint a,vec4 current,vec4 texel,vec4 temp){
    vec4 v=vec4(0);
    switch(a&15u){case 0u:v=vertexDiffuse;break;case 1u:v=current;break;case 2u:v=texel;break;case 3u:v=u.factor;break;case 5u:v=temp;break;}
    if((a&32u)!=0u)v=vec4(v.a);
    if((a&16u)!=0u)v=vec4(1)-v;
    return v;
}
vec4 equation(uint op,vec4 a,vec4 b,float ta,float ca){
    switch(op){
    case 2u:return a;case 3u:return b;case 4u:return a*b;case 5u:return 2*a*b;case 6u:return 4*a*b;
    case 7u:return a+b;case 8u:return a+b-.5;case 9u:return 2*(a+b-.5);case 10u:return a-b;
    case 11u:return a+b-a*b;case 12u:return mix(b,a,vertexDiffuse.a);case 13u:return mix(b,a,ta);
    case 14u:return mix(b,a,u.factor.a);case 15u:return a+b*(1-ta);case 16u:return mix(b,a,ca);
    }
    // Unreachable for validated uniforms. No guessed disabled-alpha behavior.
    return vec4(0);
}
void main(){
    vertexDiffuse=raster.words.x==1u?flatDiffuse:diffuse;
    vec4 current=vertexDiffuse,temp=vec4(0),last=vertexDiffuse;
    if((u.stages[0].alpha.w&4u)!=0u)current.a=1;
    for(int i=0;i<8;++i){
        Stage s=u.stages[i];if(s.color.x==1u)break;
        vec4 t=vec4(0);
        if((s.alpha.w&1u)!=0u)t=texture(images[i],texcoord[i]);
        else if((s.alpha.w&2u)!=0u)t=vec4(1);
        vec4 rgb=equation(s.color.x,argument(s.color.y,current,t,temp),argument(s.color.z,current,t,temp),t.a,current.a);
        float alpha=s.color.w==5u?temp.a:current.a;
        if(s.alpha.x!=1u)alpha=equation(s.alpha.x,argument(s.alpha.y,current,t,temp),argument(s.alpha.z,current,t,temp),t.a,current.a).a;
        vec4 next=clamp(vec4(rgb.rgb,alpha),0,1);
        if(s.color.w==5u)temp=next;else current=next;
        last=next;
    }
    if(fog.controls.x!=0u){
        float depth=fog.controls.z!=0u?1.0/gl_FragCoord.w:gl_FragCoord.z;
        float f=1;
        if(fog.controls.y==1u)f=exp(-fog.parameters.z*depth);
        else if(fog.controls.y==2u){float d=fog.parameters.z*depth;f=exp(-d*d);}
        else if(fog.controls.y==3u)f=(fog.parameters.y-depth)/(fog.parameters.y-fog.parameters.x);
        last.rgb=mix(fog.color.rgb,last.rgb,clamp(f,0,1));
    }
    outputColor=(u.stages[0].alpha.w&8u)!=0u?vec4(last.a,0,0,last.a):last;
}
