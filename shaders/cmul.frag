#version 430
// Complex multiplication of textures

out vec2 o_result;

uniform sampler2D u_left;
uniform sampler2D u_right;

void main() {
    ivec2 pos = ivec2(gl_FragCoord.xy);
    vec2 left = texelFetch(u_left, pos, 0).rg;
    vec2 right = texelFetch(u_right, pos, 0).rg;

    o_result = mat2(left, -left.g, left.r) * right;
}
