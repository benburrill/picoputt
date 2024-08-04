#version 430

out vec2 v_pos;
in vec2 a_uv;

uniform vec2 u_ndcPos;
uniform vec2 u_ndcSize;
uniform vec2 u_atlasPos;
uniform vec2 u_atlasSize;

void main() {
    v_pos = u_atlasSize * a_uv + u_atlasPos;
    gl_Position = vec4(u_ndcSize * a_uv + u_ndcPos, 0., 1.);
}
