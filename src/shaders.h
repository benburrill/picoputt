#ifndef PICOPUTT_SHADERS_H
#define PICOPUTT_SHADERS_H
#include <GL/glew.h>

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
#endif //PICOPUTT_SHADERS_H
