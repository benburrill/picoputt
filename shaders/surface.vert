#version 430
// Outputs surface position, based on scale and shift of vertex UV coordinates

out vec2 v_pos;

uniform vec2 u_scale;
uniform vec2 u_shift;
in vec2 a_ndc;
in vec2 a_uv;

void main() {
    v_pos = u_scale * a_uv + u_shift;
    gl_Position = vec4(a_ndc, 0., 1.);
}
