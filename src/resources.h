#ifndef PICOPUTT_RESOURCES_H
#define PICOPUTT_RESOURCES_H
#include <GL/glew.h>
#include <SDL.h>
#include "shaders.h"
#include "framebuffers.h"
#include "text.h"


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
    GLint u_wall;
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

    GLint u_cur;
    GLint u_pdf;
    GLint u_totalProb;
    GLint u_simSize;
    GLint u_puttActive;
    GLint u_putt;
    GLint u_colormap;
    GLint u_skybox;
    GLint u_light;
    GLint u_potential;
    GLint u_wall;
    GLint u_drContourThickness;
    GLint u_contourProgress;
    GLint u_contourSep;
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

    GLint u_momentum;
} ProgPlaneWave;
extern ProgPlaneWave g_planeWave;

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
} ProgReduce;
extern ProgReduce g_rsumReduce;
extern ProgReduce g_rgsumReduce;

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

typedef struct {
    union {
        Program prog;
        ProgDrawGlyph base;
    };

    GLint u_color;
} ProgDrawMSDFGlyph;
extern ProgDrawMSDFGlyph g_msdfGlyph;

typedef struct {
    union {
        Program prog;
        ProgIdentity vert;
    };

    GLint u_simSize;
} ProgCourse;
extern ProgCourse g_courseWall;
extern ProgCourse g_coursePotential;

typedef struct {
    union {
        Program prog;
        ProgIdentity vert;
    };
    GLint u_color;
} ProgFillColor;
extern ProgFillColor g_fillColor;

// TODO: for time-dep pots we probably want 2 buffers so we precompute
//  the next potential while we're using the current one
extern TexturedFrameBuffer g_potentialBuffer;
extern TexturedFrameBuffer g_wallBuffer;
extern TexturedFrameBuffer g_simBuffers[2];
extern TexturedFrameBuffer g_puttBuffer;
extern TexturedFrameBuffer g_pdfBuffer;
extern PaddedPyramidBuffer g_pdfPyramid;
extern TexturedFrameBuffer g_goalState;
extern PaddedPyramidBuffer g_goalPyramid;
extern TexturedFrameBuffer g_dragPot;
extern PyramidBuffer g_dragLIP;
extern GLuint g_skyboxTexture;
extern GLuint g_colormapTexture;

extern Font g_fontRegular;

int loadResources();
void freeResources();
void drawQuad();
#endif //PICOPUTT_RESOURCES_H
