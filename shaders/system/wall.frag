#version 430

out float o_wall;
in vec2 v_pos;
uniform vec2 u_simSize;

void main() {
    bool wall = abs(gl_FragCoord.x - 0.4 * u_simSize.x) < 2.;
    bool slit = abs(abs(v_pos.y - 0.5) - 0.075) < 0.06;

    o_wall = float(wall && !slit);

    // Wall as SDF:
    // o_wall = 0.05*u_simSize.y - length(gl_FragCoord.xy - vec2(0.4, 0.5)*u_simSize);
}
