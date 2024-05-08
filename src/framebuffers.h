#ifndef PICOPUTT_FRAMEBUFFERS_H
#define PICOPUTT_FRAMEBUFFERS_H
#include <GL/glew.h>
typedef struct {
    GLuint fbo;
    GLuint texture;
    GLsizei width;
    GLsizei height;
} TexturedFrameBuffer;

// Pyramid buffers are similar to mipmap layers, but they round up
// rather than down and have padding where necessary so that each texel
// on a layer has 4 child texels on the layer beneath it.

typedef struct {
    TexturedFrameBuffer buf;
    GLsizei dataWidth;
    GLsizei dataHeight;
} PyramidLayer;

typedef struct {
    size_t numLevels;
    PyramidLayer *layers;
} PyramidBuffer;

void uninitializedTexImage2D(GLenum target, GLint level, GLsizei width, GLsizei height, GLint internalformat);

int initTexturedFrameBuffer(TexturedFrameBuffer *tfb, GLsizei width, GLsizei height, GLint internalformat);
void deleteTexturedFrameBuffer(TexturedFrameBuffer *tfb);

int initPyramidBuffer(PyramidBuffer *pbuf, GLsizei width, GLsizei height, GLint internalformat);
void deletePyramidBuffer(PyramidBuffer *pbuf);

#endif //PICOPUTT_FRAMEBUFFERS_H
