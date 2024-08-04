#version 430
// This is a general purpose alternative renderer, intended to be
// modified to plot whatever is of interest for debugging purposes
out vec4 o_color;
in vec2 v_pos;

uniform sampler2D u_colormap;
uniform sampler2D u_data;  // The data to be rendered
uniform vec4 u_mode;       // Generic contextual information

void main() {
    vec2 data = texture(u_data, v_pos).rg/u_mode.r;
    o_color = vec4(-data.x, data.x, abs(data.y), 1.);
}
