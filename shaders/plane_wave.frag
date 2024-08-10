#version 430

out vec2 o_psi;
in vec2 v_pos;
uniform vec2 u_momentum;

void main() {
    float angle = dot(u_momentum, v_pos);
    o_psi = vec2(cos(angle), sin(angle));
}
