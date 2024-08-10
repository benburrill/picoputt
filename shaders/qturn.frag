#version 430
// Update the wavefunction by half a timestep.
//   A qturn (short for quarter turn) is 1 full Euler step of the real component
//   of psi, followed by a complex rotation by pi/2.  This acts effectively as
//   half a timestep because it allows us to update both the real and imaginary
//   components in a staggered way by simply iterating this single shader.
//   4 qturns (1 "turn") is exactly equivalent to 2 timesteps of the Visscher
//   staggered-time method.
//   See Visscher 1991: "A fast explicit algorithm for the time‐dependent Schrödinger equation"

out vec2 o_psi;

uniform float u_4m_dx2;           // 4*m*dx^2, where dx is texel size and m is mass
uniform float u_dt;               // Timestep
uniform sampler2D u_prev;         // Previous wavefunction state
uniform sampler2D u_potential;    // Potential function at current time
uniform sampler2D u_wall;
uniform sampler2D u_dragPot;


#define FETCH_G(coord, cond) ((cond)? texelFetch(u_prev, (coord), 0).g : 0.)
// This is consistently a bit faster than the ternary on my machine:
// #define FETCH_G(coord, cond) (float(cond)*texelFetch(u_prev, (coord), 0).g)
// However, I don't think it's safe because we could get infinity or NaN

// Some other options to get 0 when out of bounds without needing to do
// the bounds check myself (which may or may not be faster) would be:
//  * image2D
//  * robust buffer access
//  * adding a 1 pixel boundary to the buffers
//  * texture() with GL_CLAMP_TO_BORDER (could do 4 interpolated calls
//    to texture() in place of 8 calls to texelFetch())

void main() {
    ivec2 pos = ivec2(gl_FragCoord.xy);
    if (texelFetch(u_wall, pos, 0).r > 0.5) {
        o_psi = vec2(0., 0.);
        return;
    }

    ivec2 edge = textureSize(u_prev, 0) - 1;
    ivec2 flip = ivec2(pos.x, edge.y - pos.y);
    vec2 prevPsi = texelFetch(u_prev, pos, 0).rg;  // redal and gremaginary components

    float neigh = (
        FETCH_G(pos + ivec2( 1,  0),    pos.x < edge.x) +
        FETCH_G(pos + ivec2( 0,  1),    pos.y < edge.y) +
        FETCH_G(pos + ivec2(-1,  0),    pos.x > 0) +
        FETCH_G(pos + ivec2( 0, -1),    pos.y > 0)
    );

    // I'm under the assumption that all(lessThan(pos, edge)) will be faster
    // than pos.x < edge.x && pos.y < edge.y, but I haven't benchmarked.
    float corn = (
        FETCH_G(pos + ivec2( 1,  1),    all(lessThan(pos, edge))) +
        FETCH_G(pos + ivec2( 1, -1),    all(lessThan(flip, edge))) +
        FETCH_G(pos + ivec2(-1, -1),    all(greaterThan(pos, ivec2(0)))) +
        FETCH_G(pos + ivec2(-1,  1),    all(greaterThan(flip, ivec2(0))))
    );

    float V = texelFetch(u_potential, pos, 0).r + texelFetch(u_dragPot, pos, 0).r;
    // Laplacian of prevPsi.g is neigh/2 + corn/4 - 3*prevPsi.g
    // 9-point stencil is needed to get Visscher's stability conditions.
    // With 5-point stencil, stability region of dt is halved!
    float H_g = V * prevPsi.g - (neigh + 0.5 * corn - 6. * prevPsi.g) / u_4m_dx2;
    o_psi = vec2(-prevPsi.g, prevPsi.r + u_dt * H_g);
}
