#include "resources.h"
#include <GL/glew.h>
#include "game.h"
#include "shaders.h"
#include "utils.h"
#include "text.h"

ProgQTurn g_qturn = {.prog = {.name = "shaders/qturn.frag"}};
ProgGaussian g_gaussian = {.prog = {.name = "shaders/gaussian.frag"}};
ProgPDF g_pdf = {.prog = {.name = "shaders/pdf.frag"}};
ProgRenderer g_renderer = {.prog = {.name = "shaders/graphics/renderer.frag"}};
ProgDebugRenderer g_debugRenderer = {.prog = {.name = "shaders/graphics/debug.frag"}};
ProgClubGraphics g_clubGfx = {.prog = {.name = "shaders/graphics/club.frag"}};
ProgPutt g_putt = {.prog = {.name = "shaders/putt.frag"}};
ProgPlaneWave g_planeWave = {.prog = {.name = "shaders/plane_wave.frag"}};
ProgCMul g_cmul = {.prog = {.name = "shaders/cmul.frag"}};
ProgReduce g_rsumReduce = {.prog = {.name = "shaders/rsum_reduce.frag"}};
ProgReduce g_rgsumReduce = {.prog = {.name = "shaders/rgsum_reduce.frag"}};
ProgInitLIP g_initLIP = {.prog = {.name = "shaders/drag/init_lip.comp"}};
ProgBuildLIP g_buildLIP = {.prog = {.name = "shaders/drag/build_lip.comp"}};
ProgLIPKiss g_LIPKiss = {.prog = {.name = "shaders/drag/lip_kiss.comp"}};
ProgIntegrateLIP g_integrateLIP[2] = {
    {.prog = {.name = "shaders/drag/integrate_lip_x.comp"}},
    {.prog = {.name = "shaders/drag/integrate_lip_y.comp"}}
};
ProgDrawMSDFGlyph g_msdfGlyph = {.prog = {.name = "shaders/text/msdf.frag"}};
ProgCourse g_courseWall = {.prog = {.name = "shaders/system/wall.frag"}};
ProgCourse g_coursePotential = {.prog = {.name = "shaders/system/potential.frag"}};
ProgFillColor g_fillColor = {.prog = {.name = "shaders/graphics/fill_color.frag"}};

TexturedFrameBuffer g_potentialBuffer;
TexturedFrameBuffer g_wallBuffer;
TexturedFrameBuffer g_simBuffers[2];
TexturedFrameBuffer g_puttBuffer;
TexturedFrameBuffer g_pdfBuffer;
PaddedPyramidBuffer g_pdfPyramid;
TexturedFrameBuffer g_goalState;
PaddedPyramidBuffer g_goalPyramid;
TexturedFrameBuffer g_dragPot;
PyramidBuffer g_dragLIP;

Font g_fontRegular;

GLuint g_skyboxTexture = 0;
GLuint g_colormapTexture = 0;

static Shader identityShader = {
    .name = "shaders/identity.vert",
    .numReqVars = 1, .reqVars = (VariableBinding[]) {VAR_BIND_NDC}
};

static Shader surfaceShader = {
    .name = "shaders/surface.vert",
    .numReqVars = 2, .reqVars = (VariableBinding[]) {
        VAR_BIND_NDC, VAR_BIND_UV
    }
};

static Shader glyphVertShader = {
    .name = "shaders/text/glyph.vert",
    .numReqVars = 1, .reqVars = (VariableBinding[]) {VAR_BIND_UV}
};

int loadResources() {
    int err;
    initQuad();

    identityShader.id = loadShader(GL_VERTEX_SHADER, g_basePath, identityShader.name);
    if (identityShader.id == 0) return 1;

    surfaceShader.id = loadShader(GL_VERTEX_SHADER, g_basePath, surfaceShader.name);
    if (surfaceShader.id == 0) return 1;

    glyphVertShader.id = loadShader(GL_VERTEX_SHADER, g_basePath, glyphVertShader.name);
    if (glyphVertShader.id == 0) return 1;

    g_gaussian.prog.id = compileAndLinkFragProgram(
        &surfaceShader, g_basePath, g_gaussian.prog.name, "o_psi"
    );
    if (g_gaussian.prog.id == 0) return 1;
    // TODO: move common vertex uniform initialization elsewhere
    EXPECT_UNIFORM(&g_gaussian.vert, u_scale);
    EXPECT_UNIFORM(&g_gaussian.vert, u_shift);
    EXPECT_UNIFORM(&g_gaussian, u_peak);


    g_qturn.prog.id = compileAndLinkFragProgram(
        &identityShader, g_basePath, g_qturn.prog.name, "o_psi"
    );
    if (g_qturn.prog.id == 0) return 1;
    glUseProgram(g_qturn.prog.id);
    EXPECT_UNIFORM(&g_qturn, u_4m_dx2);
    EXPECT_UNIFORM(&g_qturn, u_dt);
    EXPECT_UNIFORM(&g_qturn, u_prev);
    EXPECT_UNIFORM(&g_qturn, u_potential);
    EXPECT_UNIFORM(&g_qturn, u_dragPot);
    EXPECT_UNIFORM(&g_qturn, u_wall);

    g_pdf.prog.id = compileAndLinkFragProgram(
        &identityShader, g_basePath, g_pdf.prog.name, "o_psi2"
    );
    if (g_pdf.prog.id == 0) return 1;
    EXPECT_UNIFORM(&g_pdf, u_cur);
    EXPECT_UNIFORM(&g_pdf, u_prev);

    g_renderer.prog.id = compileAndLinkFragProgram(
        &surfaceShader, g_basePath, g_renderer.prog.name, "o_color"
    );
    if (g_renderer.prog.id == 0) return 1;
    EXPECT_UNIFORM(&g_renderer.vert, u_scale);
    EXPECT_UNIFORM(&g_renderer.vert, u_shift);
    FIND_UNIFORM(&g_renderer, u_cur);
    FIND_UNIFORM(&g_renderer, u_pdf);
    FIND_UNIFORM(&g_renderer, u_totalProb);
    FIND_UNIFORM(&g_renderer, u_simSize);
    FIND_UNIFORM(&g_renderer, u_puttActive);
    FIND_UNIFORM(&g_renderer, u_putt);
    FIND_UNIFORM(&g_renderer, u_colormap);
    FIND_UNIFORM(&g_renderer, u_skybox);
    FIND_UNIFORM(&g_renderer, u_light);
    FIND_UNIFORM(&g_renderer, u_potential);
    FIND_UNIFORM(&g_renderer, u_wall);
    FIND_UNIFORM(&g_renderer, u_drContourThickness);
    FIND_UNIFORM(&g_renderer, u_contourProgress);
    FIND_UNIFORM(&g_renderer, u_contourSep);

    g_debugRenderer.prog.id = compileAndLinkFragProgram(
        &surfaceShader, g_basePath, g_debugRenderer.prog.name, "o_color"
    );
    if (g_debugRenderer.prog.id == 0) return 1;
    EXPECT_UNIFORM(&g_debugRenderer.vert, u_scale);
    EXPECT_UNIFORM(&g_debugRenderer.vert, u_shift);
    EXPECT_UNIFORM(&g_debugRenderer, u_data);
    FIND_UNIFORM(&g_debugRenderer, u_colormap);
    FIND_UNIFORM(&g_debugRenderer, u_mode);

    g_clubGfx.prog.id = compileAndLinkFragProgram(
        &surfaceShader, g_basePath, g_clubGfx.prog.name, "o_color"
    );
    if (g_clubGfx.prog.id == 0) return 1;
    EXPECT_UNIFORM(&g_clubGfx.vert, u_scale);
    EXPECT_UNIFORM(&g_clubGfx.vert, u_shift);
    EXPECT_UNIFORM(&g_clubGfx, u_radius);

    g_putt.prog.id = compileAndLinkFragProgram(
        &surfaceShader, g_basePath, g_putt.prog.name, "o_psi"
    );
    if (g_putt.prog.id == 0) return 1;
    EXPECT_UNIFORM(&g_putt.vert, u_scale);
    EXPECT_UNIFORM(&g_putt.vert, u_shift);
    EXPECT_UNIFORM(&g_putt, u_clubRadius);
    EXPECT_UNIFORM(&g_putt, u_momentum);
    EXPECT_UNIFORM(&g_putt, u_phase);

    g_planeWave.prog.id = compileAndLinkFragProgram(
        &surfaceShader, g_basePath, g_planeWave.prog.name, "o_psi"
    );
    if (g_planeWave.prog.id == 0) return 1;
    EXPECT_UNIFORM(&g_planeWave.vert, u_scale);
    EXPECT_UNIFORM(&g_planeWave.vert, u_shift);
    EXPECT_UNIFORM(&g_planeWave, u_momentum);

    g_cmul.prog.id = compileAndLinkFragProgram(
        &identityShader, g_basePath, g_cmul.prog.name, "o_result"
    );
    if (g_cmul.prog.id == 0) return 1;
    EXPECT_UNIFORM(&g_cmul, u_left);
    EXPECT_UNIFORM(&g_cmul, u_right);

    g_rsumReduce.prog.id = compileAndLinkFragProgram(
        &identityShader, g_basePath, g_rsumReduce.prog.name, "o_sum"
    );
    if (g_rsumReduce.prog.id == 0) return 1;
    EXPECT_UNIFORM(&g_rsumReduce, u_src);

    g_rgsumReduce.prog.id = compileAndLinkFragProgram(
        &identityShader, g_basePath, g_rgsumReduce.prog.name, "o_sum"
    );
    if (g_rgsumReduce.prog.id == 0) return 1;
    EXPECT_UNIFORM(&g_rgsumReduce, u_src);

    g_initLIP.prog.id = compileAndLinkCompProgram(g_basePath, g_initLIP.prog.name);
    if (g_initLIP.prog.id == 0) return 1;
    EXPECT_UNIFORM(&g_initLIP, u_cur);
    EXPECT_UNIFORM(&g_initLIP, u_prev);
    EXPECT_UNIFORM(&g_initLIP, u_lipOut);
    EXPECT_UNIFORM(&g_initLIP, u_simSize);

    g_buildLIP.prog.id = compileAndLinkCompProgram(g_basePath, g_buildLIP.prog.name);
    if (g_buildLIP.prog.id == 0) return 1;
    EXPECT_UNIFORM(&g_buildLIP, u_lipIn);
    EXPECT_UNIFORM(&g_buildLIP, u_lipOut);

    g_LIPKiss.prog.id = compileAndLinkCompProgram(g_basePath, g_LIPKiss.prog.name);
    if (g_LIPKiss.prog.id == 0) return 1;
    EXPECT_UNIFORM(&g_LIPKiss, u_lipIn);
    EXPECT_UNIFORM(&g_LIPKiss, u_potOut);

    for (int i = 0; i < 2; i++) {
        g_integrateLIP[i].prog.id = compileAndLinkCompProgram(g_basePath, g_integrateLIP[i].prog.name);
        if (g_integrateLIP[i].prog.id == 0) return 1;
        EXPECT_UNIFORM(&g_integrateLIP[i], u_lipIn);
        EXPECT_UNIFORM(&g_integrateLIP[i], u_potOut);
        EXPECT_UNIFORM(&g_integrateLIP[i], u_scale);
    }

    g_msdfGlyph.prog.id = compileAndLinkFragProgram(&glyphVertShader, g_basePath, g_msdfGlyph.prog.name, "o_color");
    if (g_msdfGlyph.prog.id == 0) return 1;
    EXPECT_UNIFORM(&g_msdfGlyph.base, u_atlas);
    EXPECT_UNIFORM(&g_msdfGlyph.base, u_ndcPos);
    EXPECT_UNIFORM(&g_msdfGlyph.base, u_ndcSize);
    EXPECT_UNIFORM(&g_msdfGlyph.base, u_atlasPos);
    EXPECT_UNIFORM(&g_msdfGlyph.base, u_atlasSize);
    FIND_UNIFORM(&g_msdfGlyph.base, u_pxrange);
    EXPECT_UNIFORM(&g_msdfGlyph, u_color);

    g_courseWall.prog.id = compileAndLinkFragProgram(&surfaceShader, g_basePath, g_courseWall.prog.name, "o_wall");
    if (g_courseWall.prog.id == 0) return 1;
    FIND_UNIFORM(&g_courseWall, u_simSize);

    g_coursePotential.prog.id = compileAndLinkFragProgram(&surfaceShader, g_basePath, g_coursePotential.prog.name, "o_potential");
    if (g_coursePotential.prog.id == 0) return 1;
    FIND_UNIFORM(&g_coursePotential, u_simSize);

    g_fillColor.prog.id = compileAndLinkFragProgram(&identityShader, g_basePath, g_fillColor.prog.name, "o_color");
    if (g_fillColor.prog.id == 0) return 1;
    EXPECT_UNIFORM(&g_fillColor, u_color);

    double aspect = 1.5;
    int simHeight = 257;
    int simWidth = (int)(simHeight*aspect);

    for (int i = 0; i < 2; i++) {
        err = initTexturedFrameBuffer(&g_simBuffers[i], simWidth, simHeight, GL_RG32F, 1);
        if (err != 0) return err;
    }


    err = initTexturedFrameBuffer(&g_potentialBuffer, simWidth, simHeight, GL_R32F, 1);
    if (err != 0) return err;
    glBindFramebuffer(GL_FRAMEBUFFER, g_potentialBuffer.fbo);
    glViewport(0, 0, simWidth, simHeight);
    glUseProgram(g_coursePotential.prog.id);
    glUniform2f(g_coursePotential.u_simSize, (float)simWidth, (float)simHeight);
    drawQuad();
    glViewport(0, 0, g_drWidth, g_drHeight);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    err = initTexturedFrameBuffer(&g_wallBuffer, simWidth, simHeight, GL_RED, 1);
    if (err != 0) return err;
    glBindFramebuffer(GL_FRAMEBUFFER, g_wallBuffer.fbo);
    glViewport(0, 0, simWidth, simHeight);
    glUseProgram(g_courseWall.prog.id);
    glUniform2f(g_courseWall.u_simSize, (float)simWidth, (float)simHeight);
    drawQuad();
    glViewport(0, 0, g_drWidth, g_drHeight);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    err = initTexturedFrameBuffer(&g_puttBuffer, simWidth, simHeight, GL_RG32F, 1);
    if (err != 0) return err;

    err = initTexturedFrameBuffer(&g_pdfBuffer, simWidth, simHeight, GL_R32F, 1);
    if (err != 0) return err;

    err = initCeilPyramidBuffer(&g_pdfPyramid, simWidth, simHeight, GL_R32F, 1);
    if (err != 0) return err;

    err = initRoofPyramidBuffer(&g_dragLIP, simWidth, simHeight, GL_RG32F, 0);
    if (err != 0) return err;

    err = initTexturedFrameBuffer(&g_dragPot, simWidth, simHeight, GL_R32F, 1);
    if (err != 0) return err;

    err = initTexturedFrameBuffer(&g_goalState, simWidth, simHeight, GL_RG32F, 1);
    if (err != 0) return err;

    err = initCeilPyramidBuffer(&g_goalPyramid, simWidth, simHeight, GL_RG32F, 1);
    if (err != 0) return err;

    if (g_renderer.u_skybox != -1) {
        char *filename;
        if (SET_ERR_IF_TRUE(SDL_asprintf(&filename, "%s%s", g_basePath, "images/pattern.bmp") == -1)) return 1;
        SDL_Surface *surf = SDL_LoadBMP(filename);
        SDL_free(filename);
        if (surf == NULL) return 1;
        GLenum format = GL_BGR;
        if (surf->format->BytesPerPixel == 4) format = GL_BGRA;
        else if (SET_ERR_IF_TRUE(surf->format->BytesPerPixel != 3)) return 1;

        glGenTextures(1, &g_skyboxTexture);

        glBindTexture(GL_TEXTURE_CUBE_MAP, g_skyboxTexture);
        for (int i = 0; i < 6; i++) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, surf->w, surf->h, 0, format, GL_UNSIGNED_BYTE, surf->pixels);
        }
        SDL_FreeSurface(surf);

        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    }

    if (g_renderer.u_colormap != -1) {
        char *filename;
        if (SET_ERR_IF_TRUE(SDL_asprintf(&filename, "%s%s", g_basePath, "images/colormap.bmp") == -1)) return 1;
        SDL_Surface *surf = SDL_LoadBMP(filename);
        SDL_free(filename);
        if (surf == NULL) return 1;

        GLenum format = GL_BGR;
        if (surf->format->BytesPerPixel == 4) format = GL_BGRA;
        else if (SET_ERR_IF_TRUE(surf->format->BytesPerPixel != 3)) return 1;

        glGenTextures(1, &g_colormapTexture);
        glBindTexture(GL_TEXTURE_2D, g_colormapTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, surf->w, surf->h, 0, format, GL_UNSIGNED_BYTE, surf->pixels);
        SDL_FreeSurface(surf);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    if ((err = loadFont(&g_fontRegular, g_basePath, "images/fonts/regular", '?', 8.f)))
        return err;

    return 0;
}

void freeResources() {
    destroyFont(&g_fontRegular);

    glDeleteTextures(1, &g_colormapTexture);
    g_colormapTexture = 0;
    glDeleteTextures(1, &g_skyboxTexture);
    g_skyboxTexture = 0;

    deleteTexturedFrameBuffer(&g_goalState);
    deletePaddedPyramidBuffer(&g_goalPyramid);
    deleteTexturedFrameBuffer(&g_dragPot);
    deletePyramidBuffer(&g_dragLIP);
    deletePaddedPyramidBuffer(&g_pdfPyramid);
    deleteTexturedFrameBuffer(&g_pdfBuffer);
    deleteTexturedFrameBuffer(&g_puttBuffer);
    deleteTexturedFrameBuffer(&g_wallBuffer);
    deleteTexturedFrameBuffer(&g_potentialBuffer);
    for (int i = 0; i < 2; i++) deleteTexturedFrameBuffer(&g_simBuffers[i]);

    glDeleteProgram(g_coursePotential.prog.id);
    glDeleteProgram(g_courseWall.prog.id);

    for (int i = 0; i < 2; i++) glDeleteProgram(g_integrateLIP[i].prog.id);
    glDeleteProgram(g_LIPKiss.prog.id);
    glDeleteProgram(g_buildLIP.prog.id);
    glDeleteProgram(g_initLIP.prog.id);
    glDeleteProgram(g_rgsumReduce.prog.id);
    glDeleteProgram(g_rsumReduce.prog.id);
    glDeleteProgram(g_planeWave.prog.id);
    glDeleteProgram(g_clubGfx.prog.id);
    glDeleteProgram(g_debugRenderer.prog.id);
    glDeleteProgram(g_renderer.prog.id);
    glDeleteProgram(g_msdfGlyph.prog.id);
    glDeleteProgram(g_fillColor.prog.id);
    glDeleteProgram(g_pdf.prog.id);
    glDeleteProgram(g_cmul.prog.id);
    glDeleteProgram(g_putt.prog.id);
    glDeleteProgram(g_qturn.prog.id);
    glDeleteProgram(g_gaussian.prog.id);
    glDeleteShader(identityShader.id);
    glDeleteShader(surfaceShader.id);
    glDeleteShader(glyphVertShader.id);
}
