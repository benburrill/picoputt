#include "shaders.h"

#include <GL/glew.h>
#include <SDL.h>
#include "utils.h"


// Return a shader ID on successful compilation, or 0 on failure.
// (Valid shader IDs returned by glCreateShader are always non-zero)
// The full path used to load the shader is simply basePath + path
// TODO: If I have in-memory shaders, maybe load with basePath = NULL
// If compilation fails, will set SDL error
GLuint loadShader(GLenum shaderType, const char *basePath, const char *path) {
    char *fullPath;
    if (SET_ERR_IF_TRUE(SDL_asprintf(&fullPath, "%s%s", basePath, path) == -1))
        return 0;

    // Not using SDL_LoadFile for better error messages
    SDL_RWops *file = SDL_RWFromFile(fullPath, "r");
    SDL_free(fullPath);
    if (file == NULL) return 0;
    const char *shaderSource = SDL_LoadFile_RW(file, NULL, 1);
    if (shaderSource == NULL) return 0;

    GLuint result = glCreateShader(shaderType);
    if (result == 0) {
        SDL_SetError("glCreateShader() returned %s", getGlErrorString(glGetError()));
        return 0;
    }

    glShaderSource(result, 1, &shaderSource, NULL);
    SDL_free((void *) shaderSource);
    return compileShaderOrDelete(result, path);
}


// glCompileShader but with error handling
// Returns back the passed shader on success.  On failure, the shader is
// deleted and 0 is returned (the null shader).
// Compilation errors are set as the SDL error, and shader is deleted.
GLuint compileShaderOrDelete(GLuint shader, const char *name) {
    glCompileShader(shader);

    GLint isCompiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
    if(isCompiled == GL_FALSE) {
        char errLog[1024];
        glGetShaderInfoLog(shader, sizeof errLog, NULL, errLog);
        glDeleteShader(shader);
        if (name == NULL) name = "GLSL shader";
        SDL_SetError("Compilation of %s failed:\n%s", name, errLog);
        return 0;
    }

    return shader;
}


// Same idea as compileShaderOrDelete, but for programs
GLuint linkProgramOrDelete(GLuint program, const char *name) {
    glLinkProgram(program);

    GLint isLinked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
    if (isLinked == GL_FALSE) {
        char errLog[1024];
        glGetProgramInfoLog(program, sizeof errLog, NULL, errLog);
        glDeleteProgram(program);
        if (name == NULL) name = "GLSL program";
        SDL_SetError("Linking of %s failed:\n%s", name, errLog);
        return 0;
    }

    return program;
}


// Create a program from a vertex and fragment shader, binding required
// attribute/fragment output variables to locations specified as reqVars
// in the Shader struct using glBind(Attrib|FragData)Location.
// A warning is produced if the "required" variables are not active
// (either they are not declared or were unused/optimized out).
//
// Bindings given to glBind(Attrib|FragData)Location are normally
// completely ignored if the variables are inactive, which means a
// location you tried to bind an inactive variable to might instead be
// silently bound to any other variable.  For instance, if you try to
// bind attribute variables a_ndc to 0 and a_uv to 1, but the shader
// instead declares as inputs b_ndc and b_uv, b_uv might be bound to 0
// and b_ndc to 1.  Ideally we would like an error if the vertex shader
// declares any inputs other than the expected a_ndc and a_uv, but
// there's not a great way to do this.  Iterating through the active
// attributes with glGetActiveAttrib would probably mostly accomplish
// that (although declaration != activeness), but it'd be a bit awkward
// and I don't think there's even an equivalent of glGetActiveAttrib for
// fragment output variables.  So instead we take a simpler approach and
// just check glGet(Attrib|FragData)Location to see if the variables we
// tried to bind are actually active, and warn otherwise.
//
// We could produce an error in that case, but it's entirely dependent
// on the implementation which variables are considered active if they
// can be optimized out of the program as a whole, so I'm not brave
// enough to actually treat it as an error.
//
// I wish glBind(Attrib|FragData)Location reserved a slot, preventing
// any other variables from binding to the specified location.  That at
// least would be a little bit better...
GLuint buildProgramFromShaders(Shader *vert, Shader *frag) {
    if (SET_ERR_IF_TRUE(vert == NULL)) return 0;
    // NULL fragment shader is sensible in principle, but not worth
    // implementing for picoputt.
    if (SET_ERR_IF_TRUE(frag == NULL)) return 0;

    GLuint program = glCreateProgram();
    for (int i = 0; i < vert->numReqVars; i++)
        glBindAttribLocation(program, vert->reqVars[i].location, vert->reqVars[i].varName);
    for (int i = 0; i < frag->numReqVars; i++)
        glBindFragDataLocation(program, frag->reqVars[i].location, frag->reqVars[i].varName);

    glAttachShader(program, vert->id);
    glAttachShader(program, frag->id);

    program = linkProgramOrDelete(program, NULL);
    if (program == 0) {
        SDL_SetError(
            "%s\n> Vertex shader: %s\n> Fragment shader: %s",
            SDL_GetError(), vert->name, frag->name
        );
        return 0;
    }

    for (int i = 0; i < vert->numReqVars; i++) {
        if (glGetAttribLocation(program, vert->reqVars[i].varName) == -1) {
            SDL_LogWarn(
                SDL_LOG_CATEGORY_APPLICATION,
                "Vertex shader %s may be improperly linked: "
                "attribute variable %s is inactive!",
                vert->name, vert->reqVars[i].varName
            );
        }
    }

    for (int i = 0; i < frag->numReqVars; i++) {
        if (glGetFragDataLocation(program, frag->reqVars[i].varName) == -1) {
            SDL_LogWarn(
                SDL_LOG_CATEGORY_APPLICATION,
                "Fragment shader %s may be improperly linked: "
                "fragment data variable %s is inactive!",
                frag->name, frag->reqVars[i].varName
            );
        }
    }

    return program;
}

// Helper for loading simple kinds of fragment programs
// Lifetime of loaded fragment shader will be bound to the program
// fragOutVar will be bound to fragment data location 0
GLuint compileAndLinkFragProgram(Shader *vert, const char *basePath, const char *fragPath, const char *fragOutVar) {
    GLuint fragId = loadShader(GL_FRAGMENT_SHADER, basePath, fragPath);
    if (fragId == 0) return 0;
    Shader frag = {
        .id = fragId, .name = fragPath, .numReqVars = 1,
        .reqVars = (VariableBinding[]){{fragOutVar, 0}}
    };

    GLuint program = buildProgramFromShaders(vert, &frag);
    glDeleteShader(fragId);  // Lifetime of fragment shader is bound to program
    return program;
}

GLuint compileAndLinkCompProgram(const char *basePath, const char *compPath) {
    GLuint compId = loadShader(GL_COMPUTE_SHADER, basePath, compPath);
    if (compId == 0) return 0;

    GLuint program = glCreateProgram();
    glAttachShader(program, compId);
    program = linkProgramOrDelete(program, compPath);
    if (program == 0) {
        SDL_SetError("%s\n> Compute shader: %s", SDL_GetError(), compPath);
        return 0;
    }

    return program;
}
