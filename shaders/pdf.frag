#version 330
// Outputs probability density, |psi|^2.  Following Visscher, since psi
// is staggered, we define |psi|^2 as R(t)^2 + I(t+dt/2)I(t-dt/2).
// If u_cur = R(t) + I(t+dt/2)i, then due to the rotation of qturn.frag,
// u_prev = I(t-dt/2) - R(t)i.  So we undo the rotation and dot them
// together to get the probability density.

out float o_psi2;

// Wavefunction state from current and previous qturns
uniform sampler2D u_cur;
uniform sampler2D u_prev;

void main() {
    ivec2 pos = ivec2(gl_FragCoord.xy);
    vec2 psi = texelFetch(u_cur, pos, 0).rg;
    vec2 psiPrev = texelFetch(u_prev, pos, 0).gr * vec2(-1., 1.);
    o_psi2 = abs(dot(psi, psiPrev));
}
