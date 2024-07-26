#version 430
// The putt-wave.  Near the origin, it is approximately a plane wave
// with momentum u_momentum.  Away from the origin (r >> u_clubRadius),
// the wavelength increases proportional to r (and so local momentum
// decreases ~1/r).

out vec2 o_psi;

uniform vec2 u_momentum;
uniform float u_clubRadius;
uniform float u_phase;
in vec2 v_pos;

void main() {
    // vec2 psi = texelFetch(u_prev, ivec2(gl_FragCoord.xy), 0).rg;
    float magMomentum = length(u_momentum);
    float pinch = u_clubRadius * magMomentum;
    float angle = (magMomentum == 0.? 0. : pinch * atan(
        dot(u_momentum, v_pos) /
        length(vec2(pinch, dot(u_momentum, vec2(v_pos.y, -v_pos.x))))
    )) - u_phase;

    // uncomment for plane wave
    // angle = dot(u_momentum, v_pos) - u_phase;

    o_psi = vec2(cos(angle), sin(angle));
}
