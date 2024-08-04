#version 430
// Renders glyphs from a multi-channel signed distance field font atlas
// See https://github.com/Chlumsky/msdfgen for details, this code is
// basically directly lifted from there.

out vec4 o_color;
in vec2 v_pos;

uniform sampler2D u_atlas;
uniform float u_pxrange;
uniform vec4 u_color;

void main() {
    vec3 msdf = texture(u_atlas, v_pos).rgb;
    float median = max(min(msdf.r, msdf.g), min(max(msdf.r, msdf.g), msdf.b));

    vec2 unitRange = u_pxrange/vec2(textureSize(u_atlas, 0));
    vec2 drTexSize = 1./fwidth(v_pos);
    float drPxRange = max(0.5*dot(unitRange, drTexSize), 1.);

    float pxSDF = drPxRange * (median - 0.5);
    o_color = u_color;
    o_color.a *= clamp(pxSDF+0.5, 0., 1.);
}
