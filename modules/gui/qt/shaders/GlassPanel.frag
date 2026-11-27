#version 440

/*****************************************************************************
 * Terrikon: "liquid glass" panel. A rounded box that shows the (blurred)
 * backdrop through a curved bevel: near the edge the picture is bent, and the
 * rim catches the light.
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
    vec4 tint;        // straight colour, alpha = strength
    vec2 size;        // of the panel, in pixels
    vec2 srcOrigin;   // where the panel's top-left is in the source, 0..1
    vec2 srcScale;    // source texture units per pixel of the panel
    float radius;     // corner radius, pixels
    float bevel;      // width of the curved rim, pixels
    float refraction; // how far the rim bends the picture, pixels
    float light;      // strength of the rim highlight, 0..1
};

layout(binding = 1) uniform sampler2D source;

float boxSdf(vec2 p, vec2 half_, float r) {
    vec2 q = abs(p) - half_ + vec2(r);
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

void main() {
    vec2 px = qt_TexCoord0 * size;
    vec2 p = px - 0.5 * size;
    vec2 h = 0.5 * size;
    float r = min(radius, min(h.x, h.y));
    float d = boxSdf(p, h, r);

    // gradient of the distance = outward normal (finite differences)
    const vec2 e = vec2(0.75, 0.0);
    vec2 n = vec2(boxSdf(p + e.xy, h, r) - boxSdf(p - e.xy, h, r),
                  boxSdf(p + e.yx, h, r) - boxSdf(p - e.yx, h, r));
    n = (dot(n, n) > 1e-6) ? normalize(n) : vec2(0.0);

    // 0 in the flat middle, 1 at the very edge; squared so the bend is
    // concentrated at the rim (like the profile of a lens)
    float t = clamp(1.0 + d / max(bevel, 1.0), 0.0, 1.0);
    float bend = t * t;
    vec2 sp = px - n * (refraction * bend);   // sample further in
    vec2 uv = srcOrigin + sp * srcScale;
    vec3 col = texture(source, clamp(uv, vec2(0.0), vec2(1.0))).rgb;

    // a touch of colour and tint, so text stays readable
    float l = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(vec3(l), col, 1.25);
    col = mix(col, tint.rgb, tint.a);
    // the rim is brighter, like thick glass
    col += vec3(0.10) * bend * light;

    // specular edge: light from the top-left, a weaker one from the bottom-right
    float rim = smoothstep(2.0, 0.0, -d);          // one-two pixels at the edge
    float s = max(dot(n, normalize(vec2(-0.6, -0.8))), 0.0)
            + 0.35 * max(dot(n, normalize(vec2(0.6, 0.8))), 0.0);
    col += vec3(1.0) * rim * (0.15 + 0.6 * s) * light;

    float a = smoothstep(0.5, -0.5, d);
    fragColor = vec4(clamp(col, 0.0, 1.0) * a, a) * qt_Opacity;
}
