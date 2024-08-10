#version 430
// Just like rsum_reduce, but sums red and green components, not just
// red.  I doubt there's any significant performance advantage to having
// the two separate shaders.  Might even hurt because more state changes
// but I'm doing it anyway.

out vec2 o_sum;

uniform sampler2D u_src;

void main() {
    ivec2 srcPos = 2 * ivec2(gl_FragCoord.xy);
    o_sum = (
        texelFetch(u_src, srcPos, 0).rg +
        texelFetch(u_src, srcPos + ivec2(1, 0), 0).rg +
        texelFetch(u_src, srcPos + ivec2(0, 1), 0).rg +
        texelFetch(u_src, srcPos + ivec2(1, 1), 0).rg
    );
}
