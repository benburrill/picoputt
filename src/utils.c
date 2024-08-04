#include "utils.h"

#include <GL/glew.h>
#include <SDL.h>


// Get a directory from an environment variable.
// This is meant to return paths like SDL_GetBasePath/SDL_GetPrefPath, so for consistency:
//  * The path always ends in a path separator (however, unlike the SDL functions, it will always be /)
//  * It is allocated with SDL_malloc, so should be freed with SDL_free
char *getEnvDir(const char *var) {
    char *val = getenv(var);
    if (val == NULL) return NULL;
    size_t len = strlen(val);

    if (val[len - 1] == '/') len -= 1;
#ifdef _WIN32
        // On windows, also strip trailing backslash
    else if (val[len - 1] == '\\') len -= 1;
#endif

    char *result = SDL_malloc(len + 2);
    if (result == NULL) return NULL;
    memcpy(result, val, len);
    result[len] = '/';
    result[len + 1] = 0;
    return result;
}


char *getGlErrorString(GLenum errCode) {
    switch (errCode) {
        case GL_NO_ERROR:                       return "(no error)";
        case GL_INVALID_ENUM:                   return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE:                  return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION:              return "GL_INVALID_OPERATION";
        case GL_STACK_OVERFLOW:                 return "GL_STACK_OVERFLOW";
        case GL_STACK_UNDERFLOW:                return "GL_STACK_UNDERFLOW";
        case GL_OUT_OF_MEMORY:                  return "GL_OUT_OF_MEMORY";
        case GL_INVALID_FRAMEBUFFER_OPERATION:  return "GL_INVALID_FRAMEBUFFER_OPERATION";
        case GL_CONTEXT_LOST:                   return "GL_CONTEXT_LOST";
        default: {
            static char errBuffer[20 + 8 + 1];
            snprintf(errBuffer, sizeof errBuffer, "Unknown GL error: 0x%08X", errCode);
            return errBuffer;
        }
    }
}

#define KNOWN_GL_ERRORS 8
static unsigned int glErrorStatus = 0;
int processGlErrors(const char *info) {
    // In debug mode, this prints (not logs, so that if there are errors
    // every frame, GL error spam can be separated from possibly more
    // useful log info) any GL errors that are in the error queue.
    // It also always records errors that have occurred as bit flags for
    // later logging at program exit.
    // This is last-resort error handling for errors that failed to be
    // properly handled elsewhere.

    // NOTE: on my OpenGL implementation, it seems the GL error "queue"
    // has a size of 1.  If there is an error in the queue, all further
    // errors of any kind are ignored.

    int numErrors = 0;
    for (GLenum err; (err = glGetError()) != GL_NO_ERROR;) {
#ifndef NDEBUG
        if (info == NULL) info = "OpenGL error";
        printf("%s: %s\n", info, getGlErrorString(err));
#endif
        unsigned int errIdx = err - GL_INVALID_ENUM;
        if (errIdx < KNOWN_GL_ERRORS) glErrorStatus |= 1 << errIdx;
        else glErrorStatus |= 1 << KNOWN_GL_ERRORS;
        numErrors++;
    }
    return numErrors;
}

// Log and clear unique GL errors that were observed by processGlErrors
void logGlErrors() {
    processGlErrors(NULL);
    if (!glErrorStatus) return;
    SDL_LogError(
        SDL_LOG_CATEGORY_APPLICATION,
        "The following OpenGL errors have occurred:"
    );

    if (glErrorStatus & (1 << KNOWN_GL_ERRORS)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, " * Unknown GL error");
        glErrorStatus ^= 1 << KNOWN_GL_ERRORS;
    }

    for (GLenum err = GL_INVALID_ENUM; glErrorStatus; err++) {
        if (glErrorStatus & 1) {
            SDL_LogError(
                SDL_LOG_CATEGORY_APPLICATION,
                " * %s", getGlErrorString(err)
            );
        }

        glErrorStatus >>= 1;
    }
}


static GLuint quadVAO = 0;
static GLuint quadVBO = 0;

void drawQuad() {
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void initQuad() {
    if (quadVAO) return;
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

void destroyQuad() {
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &quadVBO);
    quadVAO = 0;
}
