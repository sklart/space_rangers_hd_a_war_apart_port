// SPDX-License-Identifier: GPL-2.0-or-later
#version 460
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 uv;
layout(location=0) out vec4 color;
layout(std140,binding=0) uniform Draw {
  mat4 clipFromModel;
  mat4 normalFromModel;
  vec4 diffuse;
  vec4 light;
} u;
void main() {
  gl_Position=u.clipFromModel*vec4(position,1.0);
  vec3 n=mat3(u.normalFromModel)*normal;
  float n2=dot(n,n);
  float lambert=n2>0.0 ? max(dot(n*inversesqrt(n2),u.light.xyz),0.0) : 0.0;
  float lighting=clamp(u.light.w+(1.0-u.light.w)*lambert,0.0,1.0);
  color=vec4(u.diffuse.rgb*lighting,u.diffuse.a);
}
