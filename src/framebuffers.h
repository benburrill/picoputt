#ifndef PICOPUTT_FRAMEBUFFERS_H
#define PICOPUTT_FRAMEBUFFERS_H
#include <GL/glew.h>
typedef struct {
    GLuint fbo;
    GLuint texture;
    GLsizei width;
    GLsizei height;
} TexturedFrameBuffer;

// There are a few different kinds of pyramid buffer, with subtle
// differences from each other and from mipmap levels.
//
// Currently, I have a distinction between padded pyramid layers (which
// have a different padded size from their true size).  I might just
// make it so that all TFBs/pyramids can be padded in this way.
//
// initCeilPyramidBuffer initializes a padded pyramid buffer using
// ceiling division by 2 going up layers (as opposed to floor division
// for mipmap layers), for a top layer of 1x1.  The padding ensures that
// each texel on a layer has 4 child texels on the layer beneath it.
// This is currently used for reductions.
//
// initRoofPyramidBuffer initializes a regular pyramid buffer using
// floor division by 2 + 1 (hence "roof division", since it's a bit
// above the ceiling) going up layers, for a top layer of 2x2.
// This is currently used for line integral pyramids.

typedef struct {
    TexturedFrameBuffer buf;
    GLsizei dataWidth;
    GLsizei dataHeight;
} PaddedPyramidLayer;

typedef struct {
    size_t numLayers;
    PaddedPyramidLayer *layers;
} PaddedPyramidBuffer;

typedef struct {
    size_t numLayers;
    TexturedFrameBuffer *layers;
} PyramidBuffer;

void emptyTexImage2D(GLenum target, GLint level, GLsizei width, GLsizei height, GLint internalformat, int uninitialized);

int initTexturedFrameBuffer(TexturedFrameBuffer *tfb, GLsizei width, GLsizei height, GLint internalformat, int uninitialized);
void deleteTexturedFrameBuffer(TexturedFrameBuffer *tfb);

int initCeilPyramidBuffer(PaddedPyramidBuffer *pbuf, GLsizei width, GLsizei height, GLint internalformat, int uninitialized);
void deletePaddedPyramidBuffer(PaddedPyramidBuffer *pbuf);

int initRoofPyramidBuffer(PyramidBuffer *pbuf, GLsizei width, GLsizei height, GLint internalformat, int uninitialized);
void deletePyramidBuffer(PyramidBuffer *pbuf);

#endif //PICOPUTT_FRAMEBUFFERS_H
