#version 330
// Sum the red channel of the 4 child pixels from the previous layer of
// the pyramid to produce a new layer of half the size.
// For non-power-of-two textures, the layers are expected to be padded
// with 0s to have even dimensions, ie rounding up (rather than down
// like for mipmaps).

out float o_sum;

uniform sampler2D u_src;

void main() {
    ivec2 srcPos = 2 * ivec2(gl_FragCoord.xy);
    o_sum = (
        texelFetch(u_src, srcPos, 0).r +
        texelFetch(u_src, srcPos + ivec2(1, 0), 0).r +
        texelFetch(u_src, srcPos + ivec2(0, 1), 0).r +
        texelFetch(u_src, srcPos + ivec2(1, 1), 0).r
    );
}
