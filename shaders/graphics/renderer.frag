#version 430
// Produces a visualization of the wavefunction.
// In case you find my fancy graphics scientifically uncouth (or if they
// run too slow), you can disable them by setting FANCY to 0 to get a
// simple colormapped visualization of |psi| (colorbars not included).
#define FANCY 1

out vec4 o_color;

uniform sampler2D u_cur;
uniform sampler2D u_pdf;
uniform sampler2D u_totalProb;  // 1x1 texture, sum of u_pdf
uniform sampler2D u_potential;
uniform sampler2D u_wall;
uniform vec2 u_simSize;  // can't use textureSize(u_pdf, 0) because it may be padded.
uniform bool u_puttActive;
uniform sampler2D u_putt;
uniform sampler2D u_colormap;
uniform samplerCube u_skybox;
uniform vec3 u_light;
uniform float u_drContourThickness;// = 0.5;
uniform float u_contourProgress;// = 0.;
uniform float u_contourSep;// = 0.01;
in vec2 v_pos;  // texture uv coordinates provided by surface.vert

void main() {
    if (textureLod(u_wall, v_pos, 0).r > 0.5) {
        discard;
    }

    float totalProb = texelFetch(u_totalProb, ivec2(0, 0), 0).r;

    // Height map from |psi|^2, rescaled to have a fixed average height
    float avgVal = 0.08;
    float scale = avgVal * u_simSize.x * u_simSize.y / totalProb;
    float height = scale * textureLod(u_pdf, v_pos, 0).r;

#if FANCY
    // gradient has units of height/pixel of the wavefunction texture
    vec2 d = 1./u_simSize;
    vec2 grad = scale * vec2(
        (textureLod(u_pdf, v_pos + vec2(d.x, 0), 0).r - textureLod(u_pdf, v_pos + vec2(-d.x, 0), 0).r) / 2.,
        (textureLod(u_pdf, v_pos + vec2(0, d.y), 0).r - textureLod(u_pdf, v_pos + vec2(0, -d.y), 0).r) / 2.
    );

    vec3 norm = normalize(vec3(-grad, 50./(u_simSize.x+u_simSize.y)));

    // Direction from eye to fragment
    // vec3 eye = vec3(0., 0., -1.);
    vec3 eye = normalize(vec3((v_pos - 0.5)*u_simSize, -u_simSize.x));

    float lightness = 5. * min(height + 0.05, 0.2);

    // Yes, we're using sqrt(psi) here (height^0.25) for the colormap,
    // which is weird.  I think it looks a bit nicer than psi or psi^2.
    vec3 ambCol = texture(u_colormap, vec2(0.6 * pow(height, 0.25), 0.)).rgb;

    vec3 reflCol = texture(u_skybox, reflect(eye, norm)).rgb;

    float diffuse = max(0., dot(-u_light, norm));
    vec3 diffuseCol = mix(ambCol, vec3(1., 1., 1.), 0.2) * lightness;

    float spec = pow(max(0., dot(reflect(u_light, norm), -eye)), 10.);
    vec3 specCol = mix(ambCol, vec3(1., 1., 1.), 0.8) * lightness;
    //spec=diffuse; specCol=diffuseCol;

    o_color = vec4(
        0.5 * ambCol +
        0.4 * reflCol +
        0.3 * diffuse * diffuseCol +
        0.2 * spec * specCol,
        1.
    );

    float V = textureLod(u_potential, v_pos, 0).r;
    vec3 Vcolor = o_color.rgb;

    float dV = length(vec2(dFdx(V), dFdy(V)));
    if (dV != 0.) {
        float contourDist = u_contourSep*abs(fract(V/u_contourSep + u_contourProgress) - 0.5) / dV;
        Vcolor = mix(vec3(0.3, 0.1, 0.1), Vcolor, clamp(contourDist - u_drContourThickness, 0, 1.));
    }

    o_color = vec4(mix(Vcolor, o_color.rgb, clamp(height + 0.3, 0., 0.9)), 1.);


    if (u_puttActive) {
        vec2 putt = textureLod(u_putt, v_pos, 0).rg;
        float c = putt.r;
        float s = putt.g;
        vec3 ax = normalize(vec3(1., 1., 1.));
        //vec3 ax = normalize(vec3(0.2126, 0.7152, 0.0722));
        mat3 rot = (1. - c) * outerProduct(ax, ax) + mat3(
            c,          +ax.z * s,  -ax.y * s,
            -ax.z * s,  c,          +ax.x * s,
            +ax.y * s,  -ax.x * s,  c
        );

        o_color.rgb = mix(o_color.rgb, abs(rot * o_color.rgb), 0.4);
    }
#else
    o_color = texture(u_colormap, vec2(0.3 * sqrt(height), 0.));
    if (u_puttActive) {
        float putt = textureLod(u_putt, v_pos, 0).r;
        o_color.rgb *= putt * 0.1 + 0.9;
    }
#endif
}
