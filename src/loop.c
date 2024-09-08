#include "loop.h"

#include <GL/glew.h>
#include <SDL.h>

#include "game.h"
#include "utils.h"
#include "resources.h"

#define PHYS_TURNS_PER_SECOND 300

// This is misleadingly named, FPS can go lower than this.
// But the number of physics turns per frame is throttled so that the
// physics computation time doesn't exceed 1/MIN_FPS seconds by more
// than the cost of 1 physics turn.
#define MIN_FPS 20

// Simulation parameters
float dt = 0.2f;
float dx = 1.f;
float mass = 1.f;

static float winThreshold = 0.5f;

static float clubSize = 0.25f;
static int gameWon = 0;
static int puttActive = 0;
static float puttPhase = 0.f;
static int paused = 0;
static int debugView = 0;
static int score = 0;
static int par = 5;
static int debugViewIdx = 0;
static SDL_Point puttStart;
static SDL_Rect drDisplayArea;
static float displayScale;
static float initialSigma;
static SDL_FPoint holePos;

// Note: when we make potential time-dep we will almost certainly want a
// separate variable to keep track of which potential buffer is current.
static int curBuf;

static GLuint perfQuery;
static double perfQueryTurns;  // Number of turns (nominally 4 qturns) recorded in last perfQuery
static double maxTurnsPerSecond = (double)PHYS_TURNS_PER_SECOND;

static GLuint totalProbBuffer;
static float totalProbability;
static GLuint winProbBuffer;
static float winProbability;
static int needStatsUpdate;

void setGaussianWavepacket(TexturedFrameBuffer *tfb, float x0, float y0, float sigma, float dx_) {
    // Initializes the tfb with a normalized gaussian wavepacket
    // psi = A*exp(-0.5((x-x0)^2+(y-y0)^2)/sigma^2)
    // (with normalization constant A = 1/(sigma*sqrt(pi)))
    // NOTE: The name sigma may be misleading: sigma is NOT the standard
    // deviation of the PDF!  It is sqrt(2) times the standard deviation
    // since the PDF is the wavefunction squared.
    glBindFramebuffer(GL_FRAMEBUFFER, tfb->fbo);
    glViewport(0, 0, tfb->width, tfb->height);

    glUseProgram(g_gaussian.prog.id);
    float width = dx_ * (float)g_simBuffers[0].width;
    float height = dx_ * (float)g_simBuffers[0].height;
    glUniform2f(g_gaussian.vert.u_scale, width/sigma, height/sigma);
    glUniform2f(g_gaussian.vert.u_shift, -x0/sigma, -y0/sigma);
    glUniform1f(g_gaussian.u_peak, 1.f/(1.7725f*sigma));
    drawQuad();
}


void pyramidReduce(ProgReduce *reduction, PaddedPyramidBuffer *pyramid, GLint texUnit) {
    glUseProgram(reduction->prog.id);
    glUniform1i(reduction->u_src, texUnit);
    glActiveTexture(GL_TEXTURE0 + texUnit);
    for (int i = 1; i < pyramid->numLayers; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, pyramid->layers[i].buf.fbo);
        glViewport(0, 0, pyramid->layers[i].dataWidth, pyramid->layers[i].dataHeight);
        glBindTexture(GL_TEXTURE_2D, pyramid->layers[i - 1].buf.texture);
        drawQuad();
    }
}

void beginComputingStats() {
    needStatsUpdate = 1;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_simBuffers[0].texture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, g_simBuffers[1].texture);

    // Begin computation of total probability
    glViewport(0, 0, g_simBuffers[0].width, g_simBuffers[0].height);
    glUseProgram(g_pdf.prog.id);
    glUniform1i(g_pdf.u_cur, 0 + curBuf);
    glUniform1i(g_pdf.u_prev, 0 + 1 - curBuf);
    glBindFramebuffer(GL_FRAMEBUFFER, g_pdfPyramid.layers[0].buf.fbo);
    drawQuad();
    pyramidReduce(&g_rsumReduce, &g_pdfPyramid, 2);

    if (!totalProbBuffer) {
        // TODO: totalProbBuffer and winProbBuffer are never deleted
        glGenBuffers(1, &totalProbBuffer);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, totalProbBuffer);
        glBufferData(GL_PIXEL_PACK_BUFFER, sizeof(float), NULL, GL_DYNAMIC_READ);
    } else {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, totalProbBuffer);
    }

    // Already bound: glBindFramebuffer(GL_FRAMEBUFFER, g_pdfPyramid.layers[g_pdfPyramid.numLayers - 1].buf.fbo);
    glReadPixels(0, 0, 1, 1, GL_RED, GL_FLOAT, NULL);


    // Begin computation of win probability
    glViewport(0, 0, g_simBuffers[0].width, g_simBuffers[0].height);
    glUseProgram(g_cmul.prog.id);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, g_goalState.texture);
    glUniform1i(g_cmul.u_left, 2);
    glUniform1i(g_cmul.u_right, 0 + curBuf);
    glBindFramebuffer(GL_FRAMEBUFFER, g_goalPyramid.layers[0].buf.fbo);
    drawQuad();
    pyramidReduce(&g_rgsumReduce, &g_goalPyramid, 2);

    if (!winProbBuffer) {
        glGenBuffers(1, &winProbBuffer);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, winProbBuffer);
        glBufferData(GL_PIXEL_PACK_BUFFER, 3 * sizeof(float), NULL, GL_DYNAMIC_READ);
    } else {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, winProbBuffer);
    }

    // Already bound: glBindFramebuffer(GL_FRAMEBUFFER, g_goalPyramid.layers[g_goalPyramid.numLayers - 1].buf.fbo);
    glReadPixels(0, 0, 1, 1, GL_RGB, GL_FLOAT, NULL);

    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}

void updateStats() {
    if (!needStatsUpdate) return;
    needStatsUpdate = 0;
    glBindBuffer(GL_PIXEL_PACK_BUFFER, totalProbBuffer);
    float *sumPDF = glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
    if (sumPDF) {
        totalProbability = sumPDF[0] * dx * dx;
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    }

    glBindBuffer(GL_PIXEL_PACK_BUFFER, winProbBuffer);
    float *goal = glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
    if (goal) {
        winProbability = (goal[0]*goal[0] + goal[1]*goal[1]) * dx * dx / totalProbability;
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    }

    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}


void initPhysics(float x0, float y0, float sigma) {
    setGaussianWavepacket(&g_simBuffers[0], x0, y0, sigma, dx);

    // Set drag potential to zero
    glBindFramebuffer(GL_FRAMEBUFFER, g_dragPot.fbo);
    glViewport(0, 0, g_dragPot.width, g_dragPot.height);
    glClearColor(0.f, 0.f, 0.0f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Expects perfQuery to be initialized
    glBindFramebuffer(GL_FRAMEBUFFER, g_simBuffers[0].fbo);
    // Note to self: translating glViewport with x and y doesn't affect
    // gl_FragCoord, it only affects NDC (-1..1) coordinates.
    // https://www.khronos.org/opengl/wiki/Vertex_Post-Processing#Viewport_transform
    // Also it's not guaranteed to clip.  You need scissor for that.
    glViewport(0, 0, g_simBuffers[0].width, g_simBuffers[0].height);

    glUseProgram(g_qturn.prog.id);
    glUniform1f(g_qturn.u_4m_dx2, 4.f * mass * dx * dx);
    glUniform1f(g_qturn.u_dt, 0.5f * dt);

    glBeginQuery(GL_TIME_ELAPSED, perfQuery);
    curBuf = 1;
    glBindFramebuffer(GL_FRAMEBUFFER, g_simBuffers[1].fbo);

    glUniform1i(g_qturn.u_prev, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_simBuffers[0].texture);

    glUniform1i(g_qturn.u_potential, 2);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, g_potentialBuffer.texture);

    glUniform1i(g_qturn.u_dragPot, 3);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, g_dragPot.texture);

    glUniform1i(g_qturn.u_wall, 4);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, g_wallBuffer.texture);

    drawQuad();
    glEndQuery(GL_TIME_ELAPSED);
    // 1 qturn = 1/4 turn is true physically speaking, although it's
    // pretty optimistic since it doesn't account for drag update, etc.
    // But it'll only affect the first frame, so doesn't really matter.
    perfQueryTurns = 0.25;

    glUniform1f(g_qturn.u_dt, dt);

    // Unnecessary, but it's nice to have an initial value in the buffer
    glBindFramebuffer(GL_FRAMEBUFFER, g_puttBuffer.fbo);
    glClearColor(1.f, 0.f, 0.0f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    updateDisplayInfo();
}


int doPhysics(int turnsNeeded, double maxTime) {
    // Assumed preconditions: g_qturn has u_dt and u_4m_dx2 already set
    glViewport(0, 0, g_simBuffers[0].width, g_simBuffers[0].height);

    // TODO: consider switching to use only image units (glBindImageTexture)
    //   rather than texture units (glActiveTexture)
    //   I'm not quite sure though if we're writing to the images with a
    //   framebuffer whether we need to use GL_READ_WRITE and whether we
    //   need glMemoryBarrier.
    glBindImageTexture(0, g_simBuffers[0].texture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RG32F);
    glBindImageTexture(1, g_simBuffers[1].texture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RG32F);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_simBuffers[0].texture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, g_simBuffers[1].texture);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, g_potentialBuffer.texture);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, g_dragPot.texture);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, g_wallBuffer.texture);

    GLint queryDone;
    glGetQueryObjectiv(perfQuery, GL_QUERY_RESULT_AVAILABLE, &queryDone);
    if (queryDone) {
        GLuint64 nsElapsedLastFrame;
        glGetQueryObjectui64v(perfQuery, GL_QUERY_RESULT, &nsElapsedLastFrame);
        maxTurnsPerSecond = perfQueryTurns / (1e-9 * (double)nsElapsedLastFrame);
    }

    int startNewQuery = queryDone && turnsNeeded > 0;
    if (startNewQuery) glBeginQuery(GL_TIME_ELAPSED, perfQuery);

    double maxTurns = maxTurnsPerSecond * maxTime;
    int turn = 0;
    for (; turn < turnsNeeded; turn++) {
        glViewport(0, 0, g_simBuffers[0].width, g_simBuffers[0].height);
        glUseProgram(g_qturn.prog.id);
        glUniform1i(g_qturn.u_potential, 2);
        glUniform1i(g_qturn.u_dragPot, 3);
        glUniform1i(g_qturn.u_wall, 4);

        for (int i = 0; i < 4; i++) {
            glUniform1i(g_qturn.u_prev, 0 + curBuf);
            curBuf = 1 - curBuf;
            glBindFramebuffer(GL_FRAMEBUFFER, g_simBuffers[curBuf].fbo);
            drawQuad();
        }

        glBindImageTexture(2, g_dragLIP.layers[0].texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RG32F);

        glUseProgram(g_initLIP.prog.id);
        glUniform1i(g_initLIP.u_cur, curBuf);
        glUniform1i(g_initLIP.u_prev, 1 - curBuf);
        glUniform1i(g_initLIP.u_lipOut, 2);
        glUniform2i(g_initLIP.u_simSize, g_simBuffers[0].width, g_simBuffers[0].height);

        glDispatchCompute((g_simBuffers[0].width + 5)/6, (g_simBuffers[0].height + 5)/6, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        glUseProgram(g_buildLIP.prog.id);
        int prevBound = 0;
        for (int i = 1; i < g_dragLIP.numLayers; i++) {
            // This first image bind *should* be redundant so far as I can tell from the OpenGL spec because the same
            // texture should already be bound to the same image unit.  However, in some OpenGL implementations, it is
            // necessary to rebind the image (presumably due to a bug).
            //
            // Tested implementations (GL_RENDERER):
            //  * AMD Radeon(TM) Graphics on Windows: rebind is needed
            //    - It seems that without the "redundant" bind, u_lipIn somehow stays stuck on g_dragLIP.layers[0].
            //    - u_lipOut still changes (as it should) to g_dragLIP.layers[i] though.
            //    - So basically the corner of g_dragLIP.layers[1] gets copied to all levels.
            //    - Qualitatively, this "disables drag" as the top level line integrals are too small to be noticeable.
            //  * Mesa Intel(R) UHD Graphics 620 (KBL GT2) on Linux: rebind is not needed
            //    - Works fine either way, and there's no measurable performance penalty to doing the redundant bind.
            glBindImageTexture(2 + prevBound, g_dragLIP.layers[i - 1].texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RG32F);
            glBindImageTexture(3 - prevBound, g_dragLIP.layers[i].texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RG32F);
            glUniform1i(g_buildLIP.u_lipIn, 2 + prevBound);
            glUniform1i(g_buildLIP.u_lipOut, 3 - prevBound);

            glDispatchCompute((g_dragLIP.layers[i - 1].width + 11)/12, (g_dragLIP.layers[i - 1].height + 11)/12, 1);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

            prevBound = 1 - prevBound;
        }

        int lipBind = 2 + prevBound;
        int potBind = 3 - prevBound;
        glBindImageTexture(lipBind, g_dragLIP.layers[g_dragLIP.numLayers - 1].texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RG32F);
        glBindImageTexture(potBind, g_dragPot.texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F);
        glUseProgram(g_LIPKiss.prog.id);
        glUniform1i(g_LIPKiss.u_lipIn, lipBind);
        glUniform1i(g_LIPKiss.u_potOut, potBind);

        glDispatchCompute(1, 1, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        for (int i = g_dragLIP.numLayers - 2; i >= 0; i--) {
            int scale = 1 << i;
            int numX, numY;
            glBindImageTexture(lipBind, g_dragLIP.layers[i].texture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RG32F);

            numX = (g_dragLIP.layers[i].width - 1)/2;
            numY = g_dragLIP.layers[i + 1].height;
            if (numX > 0) {
                glUseProgram(g_integrateLIP[0].prog.id);
                glUniform1i(g_integrateLIP[0].u_lipIn, lipBind);
                glUniform1i(g_integrateLIP[0].u_potOut, potBind);
                glUniform1i(g_integrateLIP[0].u_scale, scale);

                glDispatchCompute((numX + 7)/8, (numY + 7)/8, 1);
                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
            }

            numX = g_dragLIP.layers[i].width;
            numY = (g_dragLIP.layers[i].height - 1)/2;
            if (numY > 0) {
                glUseProgram(g_integrateLIP[1].prog.id);
                glUniform1i(g_integrateLIP[1].u_lipIn, lipBind);
                glUniform1i(g_integrateLIP[1].u_potOut, potBind);
                glUniform1i(g_integrateLIP[1].u_scale, scale);

                glDispatchCompute((numX + 7)/8, (numY + 7)/8, 1);
                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
            }
        }

        // Not sure if necessary
        glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

        // We allow at least one turn to run (assuming turns > 0) before
        // checking maxTurns.
        if ((double)turn > maxTurns) break;
    }

    if (startNewQuery) {
        glEndQuery(GL_TIME_ELAPSED);
        perfQueryTurns = (double)turn;
    }

    return turnsNeeded - turn;
}


void applyPutt() {
    // In addition to applying the putt, this function also effectively
    // advances the wavefunction by half a timestep by applying two half
    // size qturns.  I do not plan to actually advance time to match.
    // (I might change it to use negative timesteps if I decide I care)
    // This is done to appropriately stagger the putt, which otherwise
    // can leave behind some noticeable probability residue.
    // This effect is (almost) completely eliminated by the restaggering
    // technique used here.

    // Assumed preconditions: g_qturn has u_4m_dx2 already set
    glViewport(0, 0, g_simBuffers[0].width, g_simBuffers[0].height);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_simBuffers[0].texture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, g_simBuffers[1].texture);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, g_potentialBuffer.texture);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, g_dragPot.texture);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, g_wallBuffer.texture);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, g_puttBuffer.texture);

    // Do half a qturn to un-stagger the wavefunction
    glUseProgram(g_qturn.prog.id);
    glUniform1i(g_qturn.u_potential, 2);
    glUniform1i(g_qturn.u_dragPot, 3);
    glUniform1i(g_qturn.u_wall, 4);
    glUniform1f(g_qturn.u_dt, 0.5f * dt);
    glUniform1i(g_qturn.u_prev, 0 + curBuf);
    curBuf = 1 - curBuf;
    glBindFramebuffer(GL_FRAMEBUFFER, g_simBuffers[curBuf].fbo);
    drawQuad();

    glUseProgram(g_cmul.prog.id);
    glUniform1i(g_cmul.u_left, 5);
    glUniform1i(g_cmul.u_right, 0 + curBuf);
    curBuf = 1 - curBuf;
    glBindFramebuffer(GL_FRAMEBUFFER, g_simBuffers[curBuf].fbo);
    drawQuad();

    // Do another half qturn to re-stagger the wavefunction
    glUseProgram(g_qturn.prog.id);
    glUniform1i(g_qturn.u_prev, 0 + curBuf);
    curBuf = 1 - curBuf;
    glBindFramebuffer(GL_FRAMEBUFFER, g_simBuffers[curBuf].fbo);
    drawQuad();

    // Reset timestep back to normal
    glUniform1f(g_qturn.u_dt, dt);
}


void updateDisplayInfo() {
    double simAspect = (double)g_simBuffers[0].width / (double)g_simBuffers[0].height;
    drDisplayArea.w = (int) (g_drHeight * simAspect);
    drDisplayArea.h = g_drHeight;
    if (drDisplayArea.w > g_drWidth) {
        drDisplayArea.w = g_drWidth;
        drDisplayArea.h = (int) (g_drWidth / simAspect);
    }

    drDisplayArea.x = (g_drWidth - drDisplayArea.w) / 2;
    drDisplayArea.y = (g_drHeight - drDisplayArea.h) / 2;
    displayScale = (float)drDisplayArea.w / (float)g_simBuffers[0].width;
}

SDL_FPoint simPixelPos(SDL_Point drPos) {
    return (SDL_FPoint) {
        .x = (float)(drPos.x - drDisplayArea.x) / displayScale,
        .y = (float)(drDisplayArea.h - drPos.y - drDisplayArea.y) / displayScale
    };
}

// TODO: I don't like this.  Also, I need to be more careful about coordinate systems.
//  We have:
//   simPixel, corresponding to gl_FragCoord in sim buffers
//   phys, simPixel * dx + arbitrary shift
//   screen, SDL_GetWindowSize, origin top
//   draw, drawable pixel coords SDL_GL_GetDrawableSize, origin top?  or maybe origin bottom to correspond with gl_FragCoord
//   display, screen? position relative to display rect
//     but really I think display rect should be in draw coords
//     even though where possible, we really want to be working in
//     screen-scaled coordinates rather than draw-scaled coordinates
//     Maybe display rect should be floating point screen coords?
void uniformDisplayRelative(ProgSurface prog, float scale, SDL_Point drCenter) {
    glUniform2f(prog.u_scale, scale * (float)drDisplayArea.w, scale * (float)drDisplayArea.h);
    glUniform2f(
        prog.u_shift,
        -scale * (float)(drCenter.x - drDisplayArea.x),
        -scale * (float)(drDisplayArea.h - drCenter.y - drDisplayArea.y)
    );
}

void renderDebug(GLuint texture, float a, float b, float c, float d) {
    glViewport(0, 0, g_drWidth, g_drHeight);

    glClearColor(0.8f, 0.8f, 0.8f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    glViewport(drDisplayArea.x, drDisplayArea.y, drDisplayArea.w, drDisplayArea.h);
    glUseProgram(g_debugRenderer.prog.id);

    glUniform2f(g_debugRenderer.vert.u_scale, 1.f, 1.f);
    glUniform2f(g_debugRenderer.vert.u_shift, 0.f, 0.f);

    glUniform4f(g_debugRenderer.u_mode, a, b, c, d);
    glUniform1i(g_debugRenderer.u_data, 2);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, texture);

    glUniform1i(g_debugRenderer.u_colormap, 3);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, g_colormapTexture);

    drawQuad();
}

void renderGame(float seconds, int showClub) {
    static float contourProgress = 0.f;
    contourProgress += 0.5f * seconds;
    contourProgress -= floorf(contourProgress);

    glViewport(0, 0, g_drWidth, g_drHeight);

    glClearColor(0.8f, 0.8f, 0.8f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    glViewport(drDisplayArea.x, drDisplayArea.y, drDisplayArea.w, drDisplayArea.h);
    glUseProgram(g_renderer.prog.id);

    glUniform2f(
        g_renderer.vert.u_scale,
        (float)g_pdfPyramid.layers[0].dataWidth/(float)g_pdfPyramid.layers[0].buf.width,
        (float)g_pdfPyramid.layers[0].dataHeight/(float)g_pdfPyramid.layers[0].buf.height
    );
    glUniform2f(g_renderer.vert.u_shift, 0.f, 0.f);

    glUniform2f(g_renderer.u_simSize, (float)g_simBuffers[0].width, (float)g_simBuffers[0].height);
    glUniform1f(g_renderer.u_drContourThickness, 0.5f*(float)g_drWidth/(float)g_scWidth);
    glUniform1f(g_renderer.u_contourProgress, contourProgress);
    glUniform1f(g_renderer.u_contourSep, 0.01f);

    glUniform1i(g_renderer.u_cur, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_simBuffers[curBuf].texture);

    glUniform1i(g_renderer.u_pdf, 1);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, g_pdfPyramid.layers[0].buf.texture);

    glUniform1i(g_renderer.u_totalProb, 2);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, g_pdfPyramid.layers[g_pdfPyramid.numLayers - 1].buf.texture);

    glUniform1i(g_renderer.u_skybox, 3);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_CUBE_MAP, g_skyboxTexture);

    glUniform1i(g_renderer.u_colormap, 4);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, g_colormapTexture);

    glUniform1i(g_renderer.u_potential, 5);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, g_potentialBuffer.texture);

    glUniform1i(g_renderer.u_wall, 6);
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, g_wallBuffer.texture);

    glUniform1i(g_renderer.u_puttActive, puttActive);
    if (puttActive) {
        glUniform1i(g_renderer.u_putt, 7);
        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_2D, g_puttBuffer.texture);
    }

    SDL_Point mouse;
    SDL_GetMouseState(&mouse.x, &mouse.y);
    SDL_FPoint mouseUV = {
        (float)(mouse.x - drDisplayArea.x)/(float)drDisplayArea.w,
        (float)(drDisplayArea.h - mouse.y - drDisplayArea.y)/(float)drDisplayArea.h
    };
    glUniform2f(g_renderer.u_mouse, mouseUV.x, mouseUV.y);

    drawQuad();

    if (showClub) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(g_clubGfx.prog.id);
        uniformDisplayRelative(g_clubGfx.vert, 1.f, puttActive ? puttStart : mouse);
        glUniform1f(g_clubGfx.u_radius, clubPixSize() * displayScale);
        drawQuad();
        glDisable(GL_BLEND);
    }
}


SDL_Point samplePyramid(PaddedPyramidBuffer *pbuf) {
    // TODO: I get the sense from testing this that there's a bias
    //  towards (0, 0).  Holding down space to do a random walk tends to
    //  kiss the lower left corner more often than any other.
    int x = 0;
    int y = 0;
    // Based on this: "If type is GL_FLOAT, then each component is passed as is
    // (or converted to the client's single-precision floating-point format if
    // it is different from the one used by the GL).", my understanding is that
    // it's more proper to use float rather than GLfloat, but I'm a bit unsure.
    float prev;
    size_t layer = pbuf->numLayers - 1;
    glBindFramebuffer(GL_FRAMEBUFFER, pbuf->layers[layer].buf.fbo);
    glReadPixels(0, 0, 1, 1, GL_RED, GL_FLOAT, &prev);

    while (layer--) {
        // TODO: I'm feeling a bit paranoid about GL_PACK_ALIGNMENT.
        //  Confirm that that isn't a problem here.  I think it can't be
        //  or I would have noticed worse behavior, but still should
        //  look into it.  Also couldn't we get out of bounds for non
        //  power of 2 textures?  Or is that completely handled by the
        //  padding, I can't remember.
        float quad[4];
        glBindFramebuffer(GL_FRAMEBUFFER, pbuf->layers[layer].buf.fbo);
        glReadPixels(2*x, 2*y, 2, 2, GL_RED, GL_FLOAT, &quad);
        int r = rand();
        int i = 0;
        for (; i < 4; i++) {
            r -= (int) (RAND_MAX * quad[i] / prev);
            if (r <= 0) break;
        }

        x = 2*x + (i & 1);
        y = 2*y + ((i >> 1) & 1);
        prev = quad[i & 3];
    }

    return (SDL_Point) {.x = x, .y = y};
}


#define FPS_HISTORY 8
void renderFPS(double fps) {
    float pixels = (float)g_simBuffers[0].width * (float)g_simBuffers[0].height;

    static int histIdx = 0;
    static int needsInit = 1;
    static double fpsHist[FPS_HISTORY];
    static double mtpsHist[FPS_HISTORY];
    if (needsInit) {
        for (int i = 0; i < FPS_HISTORY; i++) {
            fpsHist[i] = fps;
            mtpsHist[i] = maxTurnsPerSecond;
        }
        needsInit = 0;
    } else {
        fpsHist[histIdx] = fps;
        mtpsHist[histIdx] = maxTurnsPerSecond;
        histIdx = (histIdx + 1)%FPS_HISTORY;
    }

    double avgFPS = 0.;
    double avgMTPS = 0.;
    for (int i = 0; i < FPS_HISTORY; i++) {
        avgFPS += fpsHist[i];
        avgMTPS += mtpsHist[i];
    }
    avgFPS /= FPS_HISTORY;
    avgMTPS /= FPS_HISTORY;

    char *text;
    if (SDL_asprintf(
        &text, "%.f FPS | max perf: %.f T/s (%.f MpxT/s)",
        avgFPS, avgMTPS, pixels * avgMTPS / 1e6
    ) == -1) return;

    glViewport(0, 0, g_drWidth, g_drHeight);
    useFont(&g_fontRegular, (ProgDrawGlyph*)&g_msdfGlyph, 0);
    glUniform4f(g_msdfGlyph.u_color, 0.f, 0.f, 0.f, 1.f);
    Cursor c = {
        .left=5.f, .x=5.f, .y=5.f, .size=15.f,
        .viewWidth=(float)g_scWidth, .viewHeight=(float)g_scHeight
    };

    drawStringFixedNum(&c, text);
    SDL_free(text);
}


void renderStatusBar() {
    glUseProgram(g_fillColor.prog.id);
    int headerHeight = 40 * g_drHeight / g_scHeight;
    glViewport(0, g_drHeight - headerHeight, g_drWidth, headerHeight);
    glUniform4f(g_fillColor.u_color, 1.f, 1.f, 1.f, 0.25f);
    drawQuad();
    glViewport(0, 0, g_drWidth, g_drHeight);

    useFont(&g_fontRegular, (ProgDrawGlyph*)&g_msdfGlyph, 0);
    Cursor c = {
        .left=5.f, .x=5.f, .y=(float)g_scHeight - 27.f, .size=22.f,
        .viewWidth=(float)g_scWidth, .viewHeight=(float)g_scHeight
    };

    GLfloat mainColor[4] = {0.f, 0.f, 0.f, 1.f};
    GLfloat winColor[4] = {0.2f, 1.f, 0.f, 1.f};

    glUniform4fv(g_msdfGlyph.u_color, 1, mainColor);
    drawString(&c, "P(");
    glUniform4fv(g_msdfGlyph.u_color, 1, winColor);
    drawString(&c, "win");
    glUniform4fv(g_msdfGlyph.u_color, 1, mainColor);
    drawString(&c, ") = ");
    c.size = 25.f;
    drawChar(&c, '|');
    c.size = 22.f;
    drawChar(&c, 0x27e8);
    c.size = 20.f;
    drawString(&c, "ball");
    c.size = 22.f;
    drawChar(&c, '|');
    c.size = 20.f;
    glUniform4fv(g_msdfGlyph.u_color, 1, winColor);
    drawString(&c, "hole");
    glUniform4fv(g_msdfGlyph.u_color, 1, mainColor);
    c.size = 22.f;
    drawChar(&c, 0x27e9);
    c.size = 25.f;
    drawChar(&c, '|');
    c.size = 22.f;
    {
        float prevY = c.y;
        float prevSize = c.size;
        c.y += c.size * 0.5f;
        c.size *= 0.66f;
        drawChar(&c, '2');
        c.y = prevY;
        c.size = prevSize;
    }

    char *text;
    if (SDL_asprintf(
        &text, " = %.f%% / ",
        floorf(100.f * winProbability)
    ) == -1) return;
    glUniform4fv(g_msdfGlyph.u_color, 1, mainColor);
    drawString(&c, text);
    SDL_free(text);

    if (SDL_asprintf(
        &text, "%.f%%",
        ceilf(100.f * winThreshold)
    ) == -1) return;
    glUniform4fv(g_msdfGlyph.u_color, 1, winColor);
    drawString(&c, text);
    SDL_free(text);

    if (SDL_asprintf(
        &text, "Score: %d%s",
        score%2 == 0? score / 2 : score,
        score%2 == 0? "" : "/2"
    ) == -1) return;

    float scoreWidth = emWidth(&g_fontRegular, text);
    c.x = c.left = (float)g_scWidth - scoreWidth * c.size - 5.f;
    glUniform4fv(g_msdfGlyph.u_color, 1, mainColor);
    drawString(&c, text);
    SDL_free(text);

    c.left = c.x = 5.f;
    c.y -= 30.f;
    c.size = 18.f;

    if (debugView) {
        glUniform4f(g_msdfGlyph.u_color, 0.75f, 0.75f, 0.75f, 1.f);
        drawString(&c, "Debug view.  Press [D] to return to normal view\n");
        drawString(&c, "Left and right arrows to change view\n");
    }

    if (paused) {
        drawString(&c, "Game is paused, press [P] to unpause\n");
    }
}

void renderWinScreen() {
    glUseProgram(g_fillColor.prog.id);
    glViewport(0, 0, g_drWidth, g_drHeight);
    glUniform4f(g_fillColor.u_color, 0.f, 0.2f, 0.1f, 0.75f);
    drawQuad();

    useFont(&g_fontRegular, (ProgDrawGlyph*)&g_msdfGlyph, 0);

    Cursor c = {
        .y=(float)g_scHeight * 0.75f, .size=75.f,
        .viewWidth=(float)g_scWidth, .viewHeight=(float)g_scHeight
    };

    char *text = "You probably won!";
    float width = emWidth(&g_fontRegular, text);
    c.left = c.x = (float)g_scWidth * 0.5f - c.size * width * 0.5f;
    glUniform4f(g_msdfGlyph.u_color, 1.f, 1.f, 1.f, 1.f);
    drawString(&c, text);
    c.size = 45.f;

    if (SDL_asprintf(
        &text, "\n\nHole in %d%s (par %d%s)",
        score%2 == 0? score / 2 : score,
        score%2 == 0? "" : "/2",
        par%2 == 0? par / 2 : par,
        par%2 == 0? "" : "/2"
    ) == -1) return;
    width = emWidth(&g_fontRegular, text);
    c.left = c.x = (float)g_scWidth * 0.5f - c.size * width * 0.5f;
    drawString(&c, text);
    SDL_free(text);

    c.size = 30.f;
    text = "\nPress [R] to restart";
    width = emWidth(&g_fontRegular, text);
    c.left = c.x = (float)g_scWidth * 0.5f - c.size * width * 0.5f;
    drawString(&c, text);
}


void renderHoleArrow(float seconds) {
    static float progress = 0.f;
    float opacity = 0.3f*cosf(2.f*M_PI*progress) + 0.7f;
    progress += 0.75f * seconds;
    progress -= floorf(progress);

    glViewport(drDisplayArea.x, drDisplayArea.y, drDisplayArea.w, drDisplayArea.h);
    Glyph *arrow = findGlyph(0x2193, g_fontRegular.numGlyphs, g_fontRegular.glyphs);
    if (!arrow) return;
    Glyph centeredArrow = *arrow;
    centeredArrow.bbox.x = -centeredArrow.bbox.w / 2.f;

    Cursor c = {
        .size=80.f * (float)g_simBuffers[0].width/(float)drDisplayArea.w * (float)g_drWidth/(float)g_scWidth,
        .viewWidth=(float)g_simBuffers[0].width,
        .viewHeight=(float)g_simBuffers[0].height
    };

    c.left = c.x = 0.5f + holePos.x/dx;
    c.y = holePos.y/dx;
    glUniform4f(g_msdfGlyph.u_color, 0.2f, 1.f, 0.f, opacity);
    drawGlyph(&c, &centeredArrow);
}


// origin and size are in simulation grid units
void setPutt(SDL_FPoint origin, float size, float px, float py, float phase) {
    glViewport(0, 0, g_puttBuffer.width, g_puttBuffer.height);
    glBindFramebuffer(GL_FRAMEBUFFER, g_puttBuffer.fbo);
    glUseProgram(g_putt.prog.id);

    glUniform2f(g_putt.vert.u_scale, dx * (float)g_puttBuffer.width, dx * (float)g_puttBuffer.height);
    glUniform2f(g_putt.vert.u_shift, -dx * origin.x, -dx * origin.y);

    glUniform1f(g_putt.u_clubRadius, dx * size);
    glUniform2f(g_putt.u_momentum, px, py);
    glUniform1f(g_putt.u_phase, phase);

    drawQuad();
}


void setPlaneWavePutt(float px, float py) {
    glViewport(0, 0, g_puttBuffer.width, g_puttBuffer.height);
    glBindFramebuffer(GL_FRAMEBUFFER, g_puttBuffer.fbo);
    glUseProgram(g_planeWave.prog.id);
    glUniform2f(g_planeWave.vert.u_scale, dx * (float)g_puttBuffer.width, dx * (float)g_puttBuffer.height);
    glUniform2f(g_planeWave.u_momentum, px, py);

    drawQuad();
}


#define MAX_MEASUREMENTS 100
static size_t activeMeasurements = 0;
static SDL_Point measurements[MAX_MEASUREMENTS];
void makeMeasurements() {
    for (size_t i = 0; i < MAX_MEASUREMENTS; i++) {
        measurements[i] = samplePyramid(&g_pdfPyramid);
    }
    activeMeasurements = MAX_MEASUREMENTS;
}

void showMeasurements() {
    if (!activeMeasurements) return;
    glViewport(drDisplayArea.x, drDisplayArea.y, drDisplayArea.w, drDisplayArea.h);
    useFont(&g_fontRegular, (ProgDrawGlyph*)&g_msdfGlyph, 0);
    glUniform4f(g_msdfGlyph.u_color, 0.f, 0.f, 0.f, 0.5f);

    Cursor c = {
        .size=15.f,
        .viewWidth=(float)g_simBuffers[0].width,
        .viewHeight=(float)g_simBuffers[0].height
    };

    Glyph *dot = findGlyph('.', g_fontRegular.numGlyphs, g_fontRegular.glyphs);
    if (!dot) return;
    Glyph centerDot = *dot;
    centerDot.bbox.x = -dot->bbox.w/2.f;
    centerDot.bbox.y = -dot->bbox.h/2.f;

    for (size_t i = 0; i < activeMeasurements; i++) {
        c.left = c.x = (float)measurements[i].x;
        c.y = (float)measurements[i].y;
        drawGlyph(&c, &centerDot);
    }
}

void doMeasurement(float sigma) {
    // This is meant to behave similarly to a partial measurement of
    // position.  The post measurement state is a gaussian wavepacket
    // with radius given by sigma (sqrt(2)*standard deviation, as in
    // setGaussianWavepacket).
    // It has a central position randomly sampled from the PDF and a
    // central momentum given by the phase gradient at the central
    // position.
    // Basically everything about this is wrong in some way:
    //  * When there are walls, it shouldn't necessarily be a gaussian
    //    (I believe more generally it should be a heat kernel)
    //  * Sampling position from the PDF only makes sense for an ideal
    //    measurement of position (delta functions).
    //  * Using the local phase gradient to set the momentum makes very
    //    little sense at all.  But hopefully the player won't notice.
    SDL_Point pos = samplePyramid(&g_pdfPyramid);
    glBindFramebuffer(GL_FRAMEBUFFER, g_simBuffers[curBuf].fbo);
    float quad[3*4];
    glReadPixels(
        SDL_min(pos.x, g_simBuffers[0].width-2), SDL_min(pos.y, g_simBuffers[0].height-2),
        2, 2, GL_RGB, GL_FLOAT, &quad
    );
    float r0 = quad[0*3 + 0];
    float i0 = quad[0*3 + 1];
    float rx = quad[1*3 + 0];
    float ix = quad[1*3 + 1];
    float ry = quad[2*3 + 0];
    float iy = quad[2*3 + 1];

    float px = atan2f(r0*ix - i0*rx, r0*rx + i0*ix)/dx;
    float py = atan2f(r0*iy - i0*ry, r0*ry + i0*iy)/dx;

    initPhysics(dx*(float)pos.x, dx*(float)pos.y, sigma);
    setPlaneWavePutt(px, py);
    applyPutt();
    beginComputingStats();
    // measurements[0] = pos;
    // activeMeasurements = 1;
}


void resetGame() {
    gameWon = 0;
    puttActive = 0;
    paused = 0;
    debugView = 0;
    score = 0;
    activeMeasurements = 0;

    float simWidth = dx * (float)g_simBuffers[0].width;
    float simHeight = dx * (float)g_simBuffers[0].height;
    initialSigma = 0.03f * simHeight;
    initPhysics(0.2f * simWidth, 0.5f * simHeight, initialSigma);

    float holeRadius = 0.2f*simHeight;
    float holeDepth = 0.05f;
    float holeSigma = sqrtf(holeRadius/M_PI)*powf(2.f/mass/holeDepth, 0.25f);
    holePos = (SDL_FPoint) {.x = simWidth-0.45f*simHeight, .y = 0.5f*simHeight};
    setGaussianWavepacket(&g_goalState, holePos.x, holePos.y, holeSigma, dx);
    // initPhysics(holePos.x, holePos.y, holeSigma);

    // setPlaneWavePutt(0.5f, 0.f);
    // applyPutt();
    beginComputingStats();
}

int gameLoop() {
    Uint64 prev = SDL_GetPerformanceCounter();
    double slopTime = 0.;
    unsigned frame = 0;
    // TODO: perfQuery never gets deleted, and probably should be
    //  created elsewhere (really should be part of physics system, but
    //  I need to separate that out)
    glGenQueries(1, &perfQuery);
    resetGame();

    while (1) {
        SDL_GL_SwapWindow(g_window);
        Uint64 cur = SDL_GetPerformanceCounter();
        double pfreq = (double)SDL_GetPerformanceFrequency();
        double frameDuration = (double)(cur - prev) / pfreq;
        slopTime += frameDuration;
        prev = cur;

        int skippedTurns;
        if (!paused && !puttActive) {
            skippedTurns = doPhysics(
                (int) (slopTime * PHYS_TURNS_PER_SECOND),
                1. / MIN_FPS
            );
            slopTime = fmod(slopTime, 1. / PHYS_TURNS_PER_SECOND);
            beginComputingStats();
        } else if (puttActive) {
            int mouseX, mouseY;
            SDL_GetMouseState(&mouseX, &mouseY);
            // TODO: actually choose momentum sensibly
            //  Also, possibly we may want the putt wave we show to be
            //  different than the putt wave we use.
            float px = 8e-4f*(float)(mouseX - puttStart.x);
            float py = 8e-4f*(float)(puttStart.y - mouseY);
            puttPhase += hypotf(px, py) * 0.5f * PHYS_TURNS_PER_SECOND * dt * (float)slopTime / mass;
            slopTime = 0.;  // slopTime is fully consumed by the putt animation
            puttPhase = fmodf(puttPhase, 2.*M_PI);
            setPutt(simPixelPos(puttStart), clubPixSize(), px, py, puttPhase);
        } else {
            // If slopTime isn't used, it still needs to be reset to 0
            // (fully consumed by the frame).
            // TODO: probably could do this a bit more cleanly, and it
            //  should probably belong entirely to the physics system
            //  rather than being used for multiple things.
            slopTime = 0.;
        }

        // if (frame == 0) paused = 1;

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        if (debugView) {
            if (debugViewIdx % 3 == 0) renderDebug(g_dragPot.texture, 5e-2f, 0.f, 0.f, 0.f);
            else if ((debugViewIdx-1) % 3 == 0) renderDebug(g_dragLIP.layers[0].texture, 1e-3f, 0.f, 0.f, 0.f);
            else renderDebug(g_simBuffers[curBuf].texture, 1e-3f, 0.f, 0.f, 0.f);
        } else renderGame(paused||puttActive? 0.f:(float)frameDuration, !gameWon);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        showMeasurements();
        updateStats();
        if (winProbability >= winThreshold) {
            paused = paused || !gameWon;
            gameWon = 1;
            puttActive = 0;  // currently unnecessary
        }
        renderStatusBar();

        if (!gameWon) {
            renderHoleArrow((float)frameDuration);
        }

        if (gameWon) {
            renderWinScreen();
        }

        renderFPS(1./frameDuration);
        glDisable(GL_BLEND);

        SDL_Event e;
        while (SDL_PollEvent(&e)) switch (e.type) {
            case SDL_QUIT:
                return 0;
            case SDL_MOUSEWHEEL:
                if (gameWon) break;
                clubSize += 0.1f*e.wheel.preciseY;
                clubSize = SDL_clamp(clubSize, 0.f, 1.f);
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (gameWon) break;
                // TODO: maybe use SDL_SetRelativeMouseMode(SDL_TRUE) in putt mode
                puttActive = 1;
                puttPhase = 0.f;
                // TODO: I believe that SDL_GetMouseState gives results in screen coordinates,
                //  but we treat them as if they are in draw coordinates
                SDL_GetMouseState(&puttStart.x, &puttStart.y);
                break;
            case SDL_MOUSEBUTTONUP:
                if (puttActive) {
                    puttActive = 0;
                    applyPutt();
                    beginComputingStats();  // Update stats even when paused
                    score += 2; // 2/2
                }
                break;
            case SDL_KEYDOWN:
                if (e.key.keysym.sym == SDLK_f) {
                    SDL_Log(
                        "fps:%f, skippedTurns:%d, turns per second: %f (actual) / %f (estimated max)",
                        1./frameDuration, skippedTurns, perfQueryTurns / frameDuration, maxTurnsPerSecond
                    );

                    SDL_Log("P(win): %f, P(total): %f", winProbability, totalProbability);
                } else if (e.key.keysym.sym == SDLK_p) {
                    paused = !paused;
                    activeMeasurements = 0;
                } else if (e.key.keysym.sym == SDLK_d) {
                    debugView = !debugView;
                } else if (e.key.keysym.sym == SDLK_LEFT) {
                    if (debugView) debugViewIdx--;
                } else if (e.key.keysym.sym == SDLK_RIGHT) {
                    if (debugView) debugViewIdx++;
                } else if (e.key.keysym.sym == SDLK_SPACE) {
                    if (gameWon) break;
                    doMeasurement(initialSigma);
                    score += 1; // 1/2
                } else if (e.key.keysym.sym == SDLK_m) {
                    makeMeasurements();
                    paused = 1;
                } else if (e.key.keysym.sym == SDLK_r) {
                    resetGame();
                } else if (e.key.keysym.sym == SDLK_ESCAPE) {
                    puttActive = 0;
                }
                break;
        }

        const Uint8 *keys = SDL_GetKeyboardState(NULL);
        if (keys[SDL_SCANCODE_LEFTBRACKET]) {
            clubSize -= (float)frameDuration;
            clubSize = SDL_max(clubSize, 0.f);
        } else if (keys[SDL_SCANCODE_RIGHTBRACKET]) {
            clubSize += (float)frameDuration;
            clubSize = SDL_min(clubSize, 1.f);
        }

        processGlErrors(NULL);
        frame++;
    }
}

float clubPixSize() {
    GLsizei minDim = SDL_min(g_simBuffers[0].width, g_simBuffers[0].height);
    GLsizei maxDim = SDL_max(g_simBuffers[0].width, g_simBuffers[0].height);

    float minPixSize = 0.1f * (float)minDim;
    float maxPixSize = 0.4f * (float)maxDim;

    return (1.f - clubSize) * minPixSize + clubSize * maxPixSize;
}
