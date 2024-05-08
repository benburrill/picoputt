#version 330
// 2D Gaussian function centered at origin
// Intended to be shifted and scaled by surface.vert

out vec2 o_psi;

uniform float u_peak;
in vec2 v_pos;

void main() {
    o_psi = vec2(u_peak * exp(-0.5 * dot(v_pos, v_pos)), 0.);
}
