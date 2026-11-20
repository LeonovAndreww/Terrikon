#version 440

/*****************************************************************************
 * Terrikon: a rounded corner between two colours, drawn without blending
 * (the top-left corner of the main area, next to the side bar)
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
    vec4 outer;   // premultiplied, outside the curve
    vec4 inner;   // premultiplied, inside it
    vec2 center;  // of the curve, in pixels of the item
    vec2 size;    // of the item, in pixels
    float radius; // in pixels
};

void main() {
    vec2 p = qt_TexCoord0 * size;
    float d = length(p - center) - radius;
    // only the quarter towards the corner is curved
    if (p.x >= center.x || p.y >= center.y)
        d = -1.0;
    fragColor = mix(inner, outer, smoothstep(-0.6, 0.6, d)) * qt_Opacity;
}
