#version 440

/*****************************************************************************
 * Terrikon: a texture with a round hole, darkened (the background around
 * the round video of the player)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *****************************************************************************/

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor; // premultiplied

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 center; // 0..1 in the item
    vec2 size;   // of the item, in pixels
    float radius; // in pixels
    float dim;   // 0..1
};

layout(binding = 1) uniform sampler2D source;

void main() {
    vec4 texel = texture(source, qt_TexCoord0);
    texel.rgb *= (1.0 - dim);
    float d = length((qt_TexCoord0 - center) * size) - radius;
    fragColor = texel * smoothstep(-0.75, 0.75, d) * qt_Opacity;
}
