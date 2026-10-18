// SPDX-License-Identifier: GPL-2.0-or-later
#version 460
layout(location=0) in vec4 color;
layout(location=1) in vec2 texcoord;
layout(binding=0) uniform sampler2D texture0;
layout(location=0) out vec4 outputColor;
void main() { outputColor=color*texture(texture0,texcoord); }
