#version 430

out float o_potential;
uniform vec2 u_simSize;

#define PI 3.141592653589793
float cosWell(vec2 pos) {
    return -0.5*(cos(PI*min(length(pos), 1.)) + 1.);
}

void main() {
    vec2 fromBack = (vec2(u_simSize.x - gl_FragCoord.x, gl_FragCoord.y))/u_simSize.y;

    o_potential = 0.05*cosWell((fromBack - vec2(0.45, 0.5))/0.2);
    o_potential += 0.05*cosWell(
        (vec2(fromBack.x, abs(fromBack.y - 0.5)) - vec2(0.2, 0.3))/0.2
    );
}
