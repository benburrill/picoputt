#include "loop.h"

#include <GL/glew.h>
#include <SDL.h>

#include "game.h"
#include "utils.h"
#include "resources.h"

#define PHYS_TURNS_PER_SECOND 400
// #define PHYS_TURNS_PER_SECOND 1

// This is misleadingly named, FPS can go lower than this.
// But the number of physics turns per frame is throttled so that the
// physics computation time doesn't exceed 1/MIN_FPS seconds by more
// than the cost of 1 physics turn.
#define MIN_FPS 20

// Simulation parameters
float dt = 0.1f;
float dx = 1.f;
float mass = 1.f;

static float clubSize = 0.25f;
static int puttActive = 0;
static float puttPhase = 0.f;
static int paused = 0;
static int debugView = 0;
static size_t debugViewIdx = 0;
static SDL_Point puttStart;
static SDL_Rect displayArea;
float displayScale;

// Note: when we make potential time-dep we will almost certainly want a
// separate variable to keep track of which potential buffer is current.
static int curBuf;

GLuint perfQuery;
double perfQueryTurns;  // Number of turns (nominally 4 qturns) recorded in last perfQuery
double maxTurnsPerSecond = (double)PHYS_TURNS_PER_SECOND;


void initPhysics(float x0, float y0) {
    // Expects perfQuery to be initialized
    glBindFramebuffer(GL_FRAMEBUFFER, g_simBuffers[0].fbo);
    // Note to self: translating glViewport with x and y doesn't affect
    // gl_FragCoord, it only affects NDC (-1..1) coordinates.
    // https://www.khronos.org/opengl/wiki/Vertex_Post-Processing#Viewport_transform
    // Also it's not guaranteed to clip.  You need scissor for that.
    glViewport(0, 0, g_simBuffers[0].width, g_simBuffers[0].height);
    glUseProgram(g_gaussian.prog.id);

    float width = dx * (float)g_simBuffers[0].width;
    float height = dx * (float)g_simBuffers[0].height;
    float sigma = 0.03f * height;

    glUniform2f(g_gaussian.vert.u_scale, width/sigma, height/sigma);
    glUniform2f(g_gaussian.vert.u_shift, -x0/sigma, -y0/sigma);
    glUniform1f(g_gaussian.u_peak, 1.f/(1.7725f*sigma));
    drawQuad();

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

    // TODO: u_dragPot

    drawQuad();
    glEndQuery(GL_TIME_ELAPSED);
    // In the future, I'll be doing more stuff than just 4 qturns in a
    // turn, so although 1 qturn = 1/4 turn is true physically speaking,
    // it may be a bit optimistic from a performance perspective.
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

        for (int i = 0; i < 4; i++) {
            glUniform1i(g_qturn.u_potential, 2);
            glUniform1i(g_qturn.u_dragPot, 3);
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
            // TODO: why this no work...
            // glBindImageTexture(3 - prevBound, g_dragLIP.layers[i].texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RG32F);
            // glUniform1i(g_buildLIP.u_lipIn, 2 + prevBound);
            // glUniform1i(g_buildLIP.u_lipOut, 3 - prevBound);

            // ... but this does:
            // glBindImageTexture(2, g_dragLIP.layers[i - 1].texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RG32F);
            // glBindImageTexture(3, g_dragLIP.layers[i].texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RG32F);
            // glUniform1i(g_buildLIP.u_lipIn, 2);
            // glUniform1i(g_buildLIP.u_lipOut, 3);

            // Even this works... wtf?  It should be literally the same, just with a redundant bind
            // It seems that without the "redundant" bind, u_lipIn stays stuck on g_dragLIP.layers[0]
            // u_lipOut changes (as it should) to g_dragLIP.layers[i] though
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

        processGlErrors("after blah");

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
                if (processGlErrors("after integrate_lip_x")) printf("i: %d, w: %d, h: %d, wgx: %d, wgy: %d, numX: %d, numY: %d\n", i, g_dragLIP.layers[i].width, g_dragLIP.layers[i].height, (numX + 7)/8, (numY + 7)/8, numX, numY);
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
                if (processGlErrors("after integrate_lip_y")) printf("i: %d, w: %d, h: %d, wgx: %d, wgy: %d\n", i, g_dragLIP.layers[i].width, g_dragLIP.layers[i].height, (numX + 7)/8, (numY + 7)/8);
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

    glViewport(0, 0, g_simBuffers[0].width, g_simBuffers[0].height);
    glUseProgram(g_pdf.prog.id);
    glUniform1i(g_pdf.u_cur, 0 + curBuf);
    glUniform1i(g_pdf.u_prev, 0 + 1 - curBuf);
    glBindFramebuffer(GL_FRAMEBUFFER, g_pdfPyramid.layers[0].buf.fbo);
    drawQuad();

    glUseProgram(g_rsumReduce.prog.id);
    glUniform1i(g_rsumReduce.u_src, 2);
    glActiveTexture(GL_TEXTURE2);
    for (int i = 1; i < g_pdfPyramid.numLayers; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, g_pdfPyramid.layers[i].buf.fbo);
        glViewport(0, 0, g_pdfPyramid.layers[i].dataWidth, g_pdfPyramid.layers[i].dataHeight);
        glBindTexture(GL_TEXTURE_2D, g_pdfPyramid.layers[i - 1].buf.texture);
        drawQuad();
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
    glBindTexture(GL_TEXTURE_2D, g_puttBuffer.texture);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, g_dragPot.texture);

    // Do half a qturn to un-stagger the wavefunction
    glUseProgram(g_qturn.prog.id);
    glUniform1i(g_qturn.u_potential, 2);
    glUniform1i(g_qturn.u_dragPot, 4);
    glUniform1f(g_qturn.u_dt, 0.5f * dt);
    glUniform1i(g_qturn.u_prev, 0 + curBuf);
    curBuf = 1 - curBuf;
    glBindFramebuffer(GL_FRAMEBUFFER, g_simBuffers[curBuf].fbo);
    drawQuad();

    glUseProgram(g_cmul.prog.id);
    glUniform1i(g_cmul.u_left, 3);
    glUniform1i(g_cmul.u_right, 0 + curBuf);
    curBuf = 1 - curBuf;
    glBindFramebuffer(GL_FRAMEBUFFER, g_simBuffers[curBuf].fbo);
    drawQuad();

    // Do another half qturn to re-stagger the wavefunction
    glUseProgram(g_qturn.prog.id);
    glUniform1i(g_qturn.u_potential, 2);
    glUniform1i(g_qturn.u_dragPot, 4);
    glUniform1i(g_qturn.u_prev, 0 + curBuf);
    curBuf = 1 - curBuf;
    glBindFramebuffer(GL_FRAMEBUFFER, g_simBuffers[curBuf].fbo);
    drawQuad();

    // Reset timestep back to normal
    glUniform1f(g_qturn.u_dt, dt);
}


void updateDisplayInfo() {
    double simAspect = (double)g_simBuffers[0].width / (double)g_simBuffers[0].height;
    displayArea.w = (int) (g_height * simAspect);
    displayArea.h = g_height;
    if (displayArea.w > g_width) {
        displayArea.w = g_width;
        displayArea.h = (int) (g_width / simAspect);
    }

    displayArea.x = (g_width - displayArea.w)/2;
    displayArea.y = (g_height - displayArea.h)/2;
    displayScale = (float)displayArea.w / (float)g_simBuffers[0].width;
}

SDL_FPoint simPixelPos(SDL_Point screenPos) {
    return (SDL_FPoint) {
        .x = (float)(screenPos.x - displayArea.x) / displayScale,
        .y = (float)(displayArea.h - screenPos.y - displayArea.y) / displayScale
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
void uniformDisplayRelative(ProgSurface prog, float scale, SDL_Point centerScreen) {
    glUniform2f(prog.u_scale, scale * (float)displayArea.w, scale * (float)displayArea.h);
    glUniform2f(
        prog.u_shift,
        -scale * (float)(centerScreen.x - displayArea.x),
        -scale * (float)(displayArea.h - centerScreen.y - displayArea.y)
    );
}

void renderDebug(GLuint texture, float a, float b, float c, float d) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glClearColor(0.8f, 0.8f, 0.8f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    glViewport(displayArea.x, displayArea.y, displayArea.w, displayArea.h);
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

void render() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glClearColor(0.8f, 0.8f, 0.8f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    glViewport(displayArea.x, displayArea.y, displayArea.w, displayArea.h);
    glUseProgram(g_renderer.prog.id);

    glUniform2f(
        g_renderer.vert.u_scale,
        (float)g_pdfPyramid.layers[0].dataWidth/(float)g_pdfPyramid.layers[0].buf.width,
        (float)g_pdfPyramid.layers[0].dataHeight/(float)g_pdfPyramid.layers[0].buf.height
    );
    glUniform2f(g_renderer.vert.u_shift, 0.f, 0.f);

    glUniform2f(g_renderer.u_simSize, (float)g_simBuffers[0].width, (float)g_simBuffers[0].height);

    glUniform1i(g_renderer.u_pdf, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_pdfPyramid.layers[0].buf.texture);

    glUniform1i(g_renderer.u_totalProb, 1);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, g_pdfPyramid.layers[g_pdfPyramid.numLayers - 1].buf.texture);

    glUniform1i(g_renderer.u_skybox, 2);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_CUBE_MAP, g_skyboxTexture);

    glUniform1i(g_renderer.u_colormap, 3);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, g_colormapTexture);

    glUniform1i(g_renderer.u_puttActive, puttActive);
    if (puttActive) {
        glUniform1i(g_renderer.u_putt, 4);
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, g_puttBuffer.texture);
    }

    SDL_Point mouse;
    SDL_GetMouseState(&mouse.x, &mouse.y);
    float lx = (float)g_width/2 - (float)mouse.x;
    float ly = (float)mouse.y - (float)g_height/2;
    float lz = -0.2f*(float)g_width;
    float mag = sqrtf(lx*lx + ly*ly + lz*lz);
    glUniform3f(g_renderer.u_light, lx/mag, ly/mag, lz/mag);

    drawQuad();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(g_clubGfx.prog.id);
    uniformDisplayRelative(g_clubGfx.vert, 1.f, puttActive? puttStart : mouse);
    glUniform1f(g_clubGfx.u_radius, clubPixSize()*displayScale);
    drawQuad();
    glDisable(GL_BLEND);
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


int gameLoop() {
    Uint64 prev = SDL_GetPerformanceCounter();
    double slopTime = 0.;
    unsigned frame = 0;
    puttActive = 0;

    // TODO: perfQuery never gets deleted, and probably should be
    //  created elsewhere (really should be part of physics system, but
    //  I need to separate that out)
    glGenQueries(1, &perfQuery);
    initPhysics(0.5f * dx * (float)g_simBuffers[0].width, 0.5f * dx * (float)g_simBuffers[0].height);

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
        } else if (puttActive) {
            int mouseX, mouseY;
            SDL_GetMouseState(&mouseX, &mouseY);
            glViewport(0, 0, g_puttBuffer.width, g_puttBuffer.height);

            glBindFramebuffer(GL_FRAMEBUFFER, g_puttBuffer.fbo);
            glUseProgram(g_putt.prog.id);

            glUniform2f(g_putt.vert.u_scale, dx * (float)g_puttBuffer.width, dx * (float)g_puttBuffer.height);
            SDL_FPoint puttPix = simPixelPos(puttStart);

            glUniform2f(g_putt.vert.u_shift, -dx * puttPix.x, -dx * puttPix.y);
            glUniform1f(g_putt.u_clubRadius, dx * clubPixSize());

            // TODO: actually choose momentum sensibly
            //  Also, possibly we may want the putt wave we show to be
            //  different than the putt wave we use.
            float px = 5e-4f*(float)(mouseX - puttStart.x);
            float py = 5e-4f*(float)(puttStart.y - mouseY);
            glUniform2f(g_putt.u_momentum, px, py);
            puttPhase += hypotf(px, py) * 0.5f * PHYS_TURNS_PER_SECOND * dt * (float)slopTime / mass;
            slopTime = 0.;  // slopTime is fully consumed by the putt animation
            puttPhase = fmodf(puttPhase, 2.*M_PI);
            glUniform1f(g_putt.u_phase, puttPhase);

            drawQuad();
        } else {
            // If slopTime isn't used, it still needs to be reset to 0
            // (fully consumed by the frame).
            // TODO: probably could do this a bit more cleanly, and it
            //  should probably belong entirely to the physics system
            //  rather than being used for multiple things.
            slopTime = 0.;
        }

        // if (frame == 0) paused = 1;
        if (debugView) {
            if (debugViewIdx % 2 == 0) renderDebug(g_dragPot.texture, 5e-2f, 0.f, 0.f, 0.f);
            else renderDebug(g_dragLIP.layers[0].texture, 1e-3f, 0.f, 0.f, 0.f);
        } else render();

        SDL_Event e;
        while (SDL_PollEvent(&e)) switch (e.type) {
            case SDL_QUIT:
                return 0;
            case SDL_MOUSEWHEEL:
                clubSize += 0.1f*e.wheel.preciseY;
                clubSize = SDL_clamp(clubSize, 0.f, 1.f);
                break;
            case SDL_MOUSEBUTTONDOWN:
                // TODO: maybe use SDL_SetRelativeMouseMode(SDL_TRUE) in putt mode
                puttActive = 1;
                puttPhase = 0.f;
                SDL_GetMouseState(&puttStart.x, &puttStart.y);
                break;
            case SDL_MOUSEBUTTONUP:
                puttActive = 0;
                applyPutt();
                break;
            case SDL_KEYDOWN:
                if (e.key.keysym.sym == SDLK_f) {
                    SDL_Log(
                        "fps:%f, skippedTurns:%d, turns per second: %f (actual) / %f (estimated max)",
                        1./frameDuration, skippedTurns, perfQueryTurns / frameDuration, maxTurnsPerSecond
                    );

                    // float totalProb;
                    // glBindFramebuffer(GL_FRAMEBUFFER, g_pdfPyramid.layers[g_pdfPyramid.numLayers - 1].buf.fbo);
                    // glReadPixels(0, 0, 1, 1, GL_RED, GL_FLOAT, &totalProb);
                    // SDL_Log("totalProb: %f", totalProb);
                } else if (e.key.keysym.sym == SDLK_p) {
                    paused = !paused;
                } else if (e.key.keysym.sym == SDLK_d) {
                    debugView = !debugView;
                } else if (e.key.keysym.sym == SDLK_a) {
                    debugViewIdx++;
                } else if (e.key.keysym.sym == SDLK_SPACE) {
                    SDL_Point measurement = samplePyramid(&g_pdfPyramid);
                    initPhysics(dx*(float)measurement.x, dx*(float)measurement.y);
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
