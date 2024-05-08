#include "resources.h"
#include <GL/glew.h>
#include "game.h"
#include "shaders.h"
#include "utils.h"

ProgQTurn g_qturn = {.prog = {.name = "shaders/qturn.frag"}};
ProgGaussian g_gaussian = {.prog = {.name = "shaders/gaussian.frag"}};
ProgPDF g_pdf = {.prog = {.name = "shaders/pdf.frag"}};
ProgRenderer g_renderer = {.prog = {.name = "shaders/graphics/renderer.frag"}};
ProgClubGraphics g_clubGfx = {.prog = {.name = "shaders/graphics/club.frag"}};
ProgPutt g_putt = {.prog = {.name = "shaders/putt.frag"}};
ProgCMul g_cmul = {.prog = {.name = "shaders/cmul.frag"}};
ProgRSumReduce g_rsumReduce = {.prog = {.name = "shaders/rsum_reduce.frag"}};

TexturedFrameBuffer g_potentialBuffer;
TexturedFrameBuffer g_simBuffers[2];
TexturedFrameBuffer g_puttBuffer;
TexturedFrameBuffer g_pdfBuffer;
PyramidBuffer g_pdfPyramid;

static GLuint quadVAO = 0;
static GLuint quadVBO = 0;

GLuint g_skyboxTexture = 0;
GLuint g_colormapTexture = 0;

#define ATTR_IDX_NDC 0
#define ATTR_IDX_UV 1
VariableBinding defaultAttribVars[] = {
    {"a_ndc", ATTR_IDX_NDC},
    {"a_uv", ATTR_IDX_UV}
};

static Shader identityShader = {
    .id = 0, .name = "shaders/identity.vert",
    .numReqVars = 1, .reqVars = defaultAttribVars
};

static Shader surfaceShader = {
    .id = 0, .name = "shaders/surface.vert",
    .numReqVars = 2, .reqVars = defaultAttribVars
};

static void initQuad();
int loadResources() {
    int err;
    initQuad();

    identityShader.id = loadShader(GL_VERTEX_SHADER, g_basePath, identityShader.name);
    if (identityShader.id == 0) return 1;

    surfaceShader.id = loadShader(GL_VERTEX_SHADER, g_basePath, surfaceShader.name);
    if (surfaceShader.id == 0) return 1;

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
    // TODO: move common vertex uniform initialization elsewhere
    EXPECT_UNIFORM(&g_renderer.vert, u_scale);
    EXPECT_UNIFORM(&g_renderer.vert, u_shift);
    EXPECT_UNIFORM(&g_renderer, u_pdf);
    EXPECT_UNIFORM(&g_renderer, u_totalProb);
    EXPECT_UNIFORM(&g_renderer, u_simSize);
    EXPECT_UNIFORM(&g_renderer, u_puttActive);
    EXPECT_UNIFORM(&g_renderer, u_putt);
    FIND_UNIFORM(&g_renderer, u_colormap);
    FIND_UNIFORM(&g_renderer, u_skybox);
    FIND_UNIFORM(&g_renderer, u_light);


    g_clubGfx.prog.id = compileAndLinkFragProgram(
        &surfaceShader, g_basePath, g_clubGfx.prog.name, "o_color"
    );
    if (g_clubGfx.prog.id == 0) return 1;
    // TODO: move common vertex uniform initialization elsewhere
    EXPECT_UNIFORM(&g_clubGfx.vert, u_scale);
    EXPECT_UNIFORM(&g_clubGfx.vert, u_shift);
    EXPECT_UNIFORM(&g_clubGfx, u_radius);

    g_putt.prog.id = compileAndLinkFragProgram(
        &surfaceShader, g_basePath, g_putt.prog.name, "o_psi"
    );
    if (g_putt.prog.id == 0) return 1;
    // TODO: move common vertex uniform initialization elsewhere
    EXPECT_UNIFORM(&g_putt.vert, u_scale);
    EXPECT_UNIFORM(&g_putt.vert, u_shift);
    EXPECT_UNIFORM(&g_putt, u_clubRadius);
    EXPECT_UNIFORM(&g_putt, u_momentum);
    EXPECT_UNIFORM(&g_putt, u_phase);

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

    double aspect = 1.6;
    int simHeight = 512;
    int simWidth = (int)(simHeight*aspect);

    for (int i = 0; i < 2; i++) {
        err = initTexturedFrameBuffer(&g_simBuffers[i], simWidth, simHeight, GL_RG32F);
        if (err != 0) return err;
    }


    err = initTexturedFrameBuffer(&g_potentialBuffer, simWidth, simHeight, GL_R32F);
    if (err != 0) return err;
    // zero potential
    glBindFramebuffer(GL_FRAMEBUFFER, g_potentialBuffer.fbo);
    glClearColor(0.f, 0.f, 0.0f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    err = initTexturedFrameBuffer(&g_puttBuffer, simWidth, simHeight, GL_RG32F);
    if (err != 0) return err;

    err = initTexturedFrameBuffer(&g_pdfBuffer, simWidth, simHeight, GL_R32F);
    if (err != 0) return err;

    err = initPyramidBuffer(&g_pdfPyramid, simWidth, simHeight, GL_R32F);
    if (err != 0) return err;

    // for (int i = 0; i < g_pdfPyramid.numLevels; i++) {
    //     SDL_Log("%d, %d, %d, %d", g_pdfPyramid.layers[i].dataWidth, g_pdfPyramid.layers[i].dataHeight, g_pdfPyramid.layers[i].buf.width, g_pdfPyramid.layers[i].buf.height);
    // }


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

    return 0;
}

void freeResources() {
    glDeleteTextures(1, &g_colormapTexture);
    g_colormapTexture = 0;
    glDeleteTextures(1, &g_skyboxTexture);
    g_skyboxTexture = 0;

    deletePyramidBuffer(&g_pdfPyramid);
    deleteTexturedFrameBuffer(&g_pdfBuffer);
    deleteTexturedFrameBuffer(&g_puttBuffer);
    deleteTexturedFrameBuffer(&g_potentialBuffer);
    for (int i = 0; i < 2; i++) deleteTexturedFrameBuffer(&g_simBuffers[i]);

    glDeleteProgram(g_cmul.prog.id);
    glDeleteProgram(g_putt.prog.id);
    glDeleteProgram(g_qturn.prog.id);
    glDeleteProgram(g_gaussian.prog.id);
    glDeleteShader(identityShader.id);
    glDeleteShader(surfaceShader.id);
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &quadVBO);
}


void drawQuad() {
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

static void initQuad() {
    GLfloat vertAttribs[] = {
        // NDC coordinates       UV coordinates
        // Triangle 1
        -1.f, -1.f,            0.f, 0.f,
        +1.f, -1.f,            1.f, 0.f,
        +1.f, +1.f,            1.f, 1.f,

        // Triangle 2
        -1.f, -1.f,            0.f, 0.f,
        +1.f, +1.f,            1.f, 1.f,
        -1.f, +1.f,            0.f, 1.f
    };

    GLsizei stride = 4 * sizeof(GLfloat);
    void *offsetNDC = (void *) 0;
    void *offsetUV = (void *) (2 * sizeof(GLfloat));

    glGenBuffers(1, &quadVBO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof vertAttribs, vertAttribs, GL_STATIC_DRAW);

    glGenVertexArrays(1, &quadVAO);
    glBindVertexArray(quadVAO);

    glVertexAttribPointer(ATTR_IDX_NDC, 2, GL_FLOAT, GL_FALSE, stride, offsetNDC);
    glEnableVertexAttribArray(ATTR_IDX_NDC);

    glVertexAttribPointer(ATTR_IDX_UV, 2, GL_FLOAT, GL_FALSE, stride, offsetUV);
    glEnableVertexAttribArray(ATTR_IDX_UV);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}
