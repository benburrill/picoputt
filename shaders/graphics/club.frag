#version 430
// Draws a glowing circle to visualize putt.frag's u_clubRadius.

out vec4 o_color;

uniform float u_radius;
in vec2 v_pos;

void main() {
    float brightness = exp(-0.1*abs(length(v_pos) - u_radius));
    o_color = vec4(0.4, 1., 1., 0.6*brightness);
}
