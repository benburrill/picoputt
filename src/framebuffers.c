#include "framebuffers.h"
#include <GL/glew.h>
#include <SDL.h>
#include <assert.h>
#include "utils.h"

// If uninitialized is 0, it is 0-initialized.  Otherwise, it is uninitialized.
void emptyTexImage2D(GLenum target, GLint level, GLsizei width, GLsizei height, GLint internalformat, int uninitialized) {
    // From https://registry.khronos.org/OpenGL-Refpages/gl4/html/glTexImage2D.xhtml:
    //  GL_INVALID_OPERATION is generated if internalformat is GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT16,
    //  GL_DEPTH_COMPONENT24, or GL_DEPTH_COMPONENT32F, and format is not GL_DEPTH_COMPONENT.
    // There's nothing that says you won't get an error if data is NULL.
    // I tested it and yup, you really do get an error.
    // I'm not planning to use GL_DEPTH_COMPONENT, but for correctness
    // we must handle this case!  Can't just do format = internalformat
    // because there are more internalformats than there are formats.

    GLenum format = GL_RED;
    switch (internalformat) {
        case GL_DEPTH_COMPONENT:
        case GL_DEPTH_COMPONENT16:
        case GL_DEPTH_COMPONENT24:
        case GL_DEPTH_COMPONENT32F:
            format = GL_DEPTH_COMPONENT;  // Are you happy now OpenGL???
            break;
    }

    void *data = NULL;
    if (!uninitialized) {
        // There are multiple ways to initialize a texture to 0, most of
        // which are in some way affected by global GL state -- calloc'd
        // image data feels cleanest to me, but we do need to be careful
        // of alignment.

        data = calloc((width+3)/4 * 4 * height, sizeof(GLbyte));

        // Don't create an uninitialized texture when we wanted it zero
        // initialized.  Some error eventually is better than no error.
        if (!data) return;

        // Ensure relevant GL state is actually set to default values.
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);

        // "These values are provided as a convenience to the programmer"
        // They are so "convenient" that unless you ALWAYS set them to 0
        // before calling glTexImage2D, there is a small possibility
        // that they could have been left set to some non-zero value by
        // some previous code and result in memory access violations!
        // (yeah ok, that'd really be a bug with whatever code set them,
        // but still though...)
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    }

    // We must use GL_UNSIGNED_BYTE rather than GL_BYTE to ensure that
    // the alternate mapping of signed integers (0 -> 1/255) is not used
    glTexImage2D(target, level, internalformat, width, height, 0, format, GL_UNSIGNED_BYTE, data);

    if (!uninitialized) free(data);
}


// TODO: I think we should support multiple textures for each
//  framebuffer, since my current plan is that levels will have
//  different color attachments for different features.  That means also
//  that struct will need array of textures.
//  Actually though, I don't think I want all the textures to have the
//  same type, so perhaps this is the wrong approach.  But one way or
//  another we will need them all in one FBO.
//  Side note: even though I will have many FBOs with same textures of
//  same width/height, I think there should only ever be one FBO per TFB
int initTexturedFrameBuffer(TexturedFrameBuffer *tfb, GLsizei width, GLsizei height, GLint internalformat, int uninitialized) {
    glGenTextures(1, &tfb->texture);
    glBindTexture(GL_TEXTURE_2D, tfb->texture);

    emptyTexImage2D(GL_TEXTURE_2D, 0, width, height, internalformat, uninitialized);

    // Necessary to set MIN_FILTER as we have no mipmap layers
    // MAG_FILTER is linear by default, but might as well set both to be
    // explicit about it.
    // It seems another, probably better option would be to specify that
    // we actually only have one mipmap layer, not the default of 1000:
    // https://www.khronos.org/opengl/wiki/Common_Mistakes#Creating_a_complete_texture
    // TODO: I'm curious if glGenerateMipmap sets GL_TEXTURE_MAX_LEVEL,
    //  or does it create a bunch of extra 1x1 textures?
    //  Also, what's the difference between GL_TEXTURE_MAX_LEVEL and
    //  GL_TEXTURE_MAX_LOD?
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // GL_CLAMP_TO_EDGE is a better default for the few times I actually use texture()
    // Possibly for some cases a zero-boundary would be a bit better, so maybe in the future
    // I'll want to make texture parameters easily customizable with a struct or something.
    // But for now, I don't think there's a pressing need.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    tfb->width = width;
    tfb->height = height;

    glGenFramebuffers(1, &tfb->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, tfb->fbo);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tfb->texture, 0);
    int err = 0;
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        SDL_SetError(
            "Incomplete framebuffer, status code: 0x%X, GL error: %s",
            status, getGlErrorString(glGetError())
        );
        err = 1;
        deleteTexturedFrameBuffer(tfb);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    return err;
}


void deleteTexturedFrameBuffer(TexturedFrameBuffer *tfb) {
    if (tfb == NULL) return;
    glDeleteFramebuffers(1, &tfb->fbo);
    tfb->fbo = 0;
    glDeleteTextures(1, &tfb->texture);
    tfb->texture = 0;
}


int initCeilPyramidBuffer(PaddedPyramidBuffer *pbuf, GLsizei width, GLsizei height, GLint internalformat, int uninitialized) {
    PaddedPyramidLayer layers[32];  // GLsizei is specified to be 32 bits
    size_t numLayers = 0;

    if (SET_ERR_IF_TRUE(width < 1 || height < 1)) return 1;
    while (1) {
        assert(numLayers < 32);
        GLsizei paddedWidth = width & 1? width + 1 : width;
        GLsizei paddedHeight = height & 1? height + 1 : height;
        if (width == 1 && height == 1) paddedWidth = paddedHeight = 1;

        int err = initTexturedFrameBuffer(
            &layers[numLayers].buf, paddedWidth, paddedHeight, internalformat,
            uninitialized && paddedWidth == width && paddedHeight == height
        );

        if (err) return err;

        layers[numLayers].dataWidth = width;
        layers[numLayers].dataHeight = height;

        numLayers++;

        if (width <= 1 && height <= 1) break;

        width = (width + 1) / 2;
        height = (height + 1) / 2;
    };

    size_t size = numLayers * sizeof(PaddedPyramidLayer);
    pbuf->layers = malloc(size);
    memcpy(pbuf->layers, layers, size);
    pbuf->numLayers = numLayers;
    return 0;
}

void deletePaddedPyramidBuffer(PaddedPyramidBuffer *pbuf) {
    if (pbuf == NULL || pbuf->layers == NULL) return;
    for (int i = 0; i < pbuf->numLayers; i++) {
        deleteTexturedFrameBuffer(&pbuf->layers[i].buf);
    }
    free(pbuf->layers);
    pbuf->layers = NULL;
    pbuf->numLayers = 0;
}


int initRoofPyramidBuffer(PyramidBuffer *pbuf, GLsizei width, GLsizei height, GLint internalformat, int uninitialized) {
    TexturedFrameBuffer layers[32];  // GLsizei is specified to be 32 bits
    size_t numLayers = 0;

    if (SET_ERR_IF_TRUE(width < 2 || height < 2)) return 1;
    while (1) {
        assert(numLayers < 32);

        int err = initTexturedFrameBuffer(&layers[numLayers], width, height, internalformat, uninitialized);
        if (err) return err;
        numLayers++;

        if (width <= 2 && height <= 2) break;
        width = width / 2 + 1;
        height = height / 2 + 1;
    }

    size_t size = numLayers * sizeof(TexturedFrameBuffer);
    pbuf->layers = malloc(size);
    memcpy(pbuf->layers, layers, size);
    pbuf->numLayers = numLayers;
    return 0;
}

void deletePyramidBuffer(PyramidBuffer *pbuf) {
    if (pbuf == NULL || pbuf->layers == NULL) return;
    for (int i = 0; i < pbuf->numLayers; i++) {
        deleteTexturedFrameBuffer(&pbuf->layers[i]);
    }
    free(pbuf->layers);
    pbuf->layers = NULL;
    pbuf->numLayers = 0;
}
