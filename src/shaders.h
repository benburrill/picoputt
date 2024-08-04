#ifndef PICOPUTT_SHADERS_H
#define PICOPUTT_SHADERS_H
#include <GL/glew.h>

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
    const char *varName;
    GLuint location;
} VariableBinding;

// A shader with required explicit variable bindings.
// They're not actually required, see buildProgramFromShaders for rant.
// For vertex shaders, reqVars are attribute input variables.
// For fragment shaders, reqVars are fragment data output variables.
typedef struct {
    GLuint id;
    const char *name;
    size_t numReqVars;
    VariableBinding *reqVars;
} Shader;

GLuint loadShader(GLenum shaderType, const char *basePath, const char *path);
GLuint compileShaderOrDelete(GLuint shader, const char *name);
GLuint linkProgramOrDelete(GLuint program, const char *name);
GLuint buildProgramFromShaders(Shader *vert, Shader *frag);
GLuint compileAndLinkFragProgram(Shader *vert, const char *basePath, const char *fragPath, const char *fragOutVar);
GLuint compileAndLinkCompProgram(const char *basePath, const char *compPath);
#endif //PICOPUTT_SHADERS_H
