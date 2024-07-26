#ifndef PICOPUTT_RESOURCES_H
#define PICOPUTT_RESOURCES_H
#include <GL/glew.h>
#include <SDL.h>
#include "shaders.h"
#include "framebuffers.h"

#define FIND_UNIFORM(p, u) \
    ((p)->u = glGetUniformLocation((p)->prog.id, #u))

// For similar reasons as buildProgramFromShaders, I don't actually want
// to produce an error if expected uniforms are inactive, but this macro
// will load the uniform location into the program struct and warn if it
// is inactive.
#define EXPECT_UNIFORM(p, u) \
    ((void) (FIND_UNIFORM(p, u) == -1 && \
    (SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, \
        "Uniform %s is not active in program %s", \
        #u, (p)->prog.name), 1)))

typedef struct {
    GLuint id;
    const char *name;
} Program;

typedef struct {
    Program prog;
} ProgIdentity;

typedef struct {
    Program prog;
    GLint u_scale;
    GLint u_shift;
} ProgSurface;

typedef struct {
    union {
        Program prog;
        ProgIdentity vert;
    };

    GLint u_4m_dx2;
    GLint u_dt;
    GLint u_prev;
    GLint u_potential;
    GLint u_dragPot;
} ProgQTurn;
extern ProgQTurn g_qturn;


typedef struct {
    union {
        Program prog;
        ProgSurface vert;
    };

    GLint u_peak;
} ProgGaussian;
extern ProgGaussian g_gaussian;

typedef struct {
    union {
        Program prog;
        ProgSurface vert;
    };

    GLint u_pdf;
    GLint u_totalProb;
    GLint u_simSize;
    GLint u_puttActive;
    GLint u_putt;
    GLint u_colormap;
    GLint u_skybox;
    GLint u_light;
} ProgRenderer;
extern ProgRenderer g_renderer;

typedef struct {
    union {
        Program prog;
        ProgSurface vert;
    };

    GLint u_colormap;
    GLint u_data;
    GLint u_mode;
} ProgDebugRenderer;
extern ProgDebugRenderer g_debugRenderer;

typedef struct {
    union {
        Program prog;
        ProgIdentity vert;
    };

    GLint u_cur;
    GLint u_prev;
} ProgPDF;
extern ProgPDF g_pdf;

typedef struct {
    union {
        Program prog;
        ProgSurface vert;
    };

    GLint u_momentum;
    GLint u_clubRadius;
    GLint u_phase;
} ProgPutt;
extern ProgPutt g_putt;

typedef struct {
    union {
        Program prog;
        ProgSurface vert;
    };

    GLint u_radius;
} ProgClubGraphics;
extern ProgClubGraphics g_clubGfx;

typedef struct {
    union {
        Program prog;
        ProgIdentity vert;
    };

    GLint u_left;
    GLint u_right;
} ProgCMul;
extern ProgCMul g_cmul;

typedef struct {
    union {
        Program prog;
        ProgIdentity vert;
    };

    GLint u_src;
} ProgRSumReduce;
extern ProgRSumReduce g_rsumReduce;

typedef struct {
    Program prog;
    GLint u_cur;
    GLint u_prev;
    GLint u_lipOut;
    GLint u_simSize;
} ProgInitLIP;
extern ProgInitLIP g_initLIP;

typedef struct {
    Program prog;
    GLint u_lipOut;
    GLint u_lipIn;
} ProgBuildLIP;
extern ProgBuildLIP g_buildLIP;

typedef struct {
    Program prog;
    GLint u_potOut;
    GLint u_lipIn;
} ProgLIPKiss;
extern ProgLIPKiss g_LIPKiss;

typedef struct {
    Program prog;
    GLint u_potOut;
    GLint u_lipIn;
    GLint u_scale;
} ProgIntegrateLIP;
extern ProgIntegrateLIP g_integrateLIP[2];

// TODO: for time-dep pots we probably want 2 buffers so we precompute
//  the next potential while we're using the current one
extern TexturedFrameBuffer g_potentialBuffer;
extern TexturedFrameBuffer g_simBuffers[2];
extern TexturedFrameBuffer g_puttBuffer;
extern TexturedFrameBuffer g_pdfBuffer;
extern PaddedPyramidBuffer g_pdfPyramid;
extern PyramidBuffer g_dragLIP;
extern TexturedFrameBuffer g_dragPot;
extern GLuint g_skyboxTexture;
extern GLuint g_colormapTexture;

int loadResources();
void freeResources();
void drawQuad();
#endif //PICOPUTT_RESOURCES_H
