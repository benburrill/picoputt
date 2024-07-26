#version 430
in vec2 a_ndc;

void main() {
    gl_Position = vec4(a_ndc, 0., 1.);
}
