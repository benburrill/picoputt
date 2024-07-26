#include "game.h"
#include <GL/glew.h>
#include <SDL.h>
#include <time.h>

#include "resources.h"
#include "utils.h"
#include "config.h"

char *g_basePath = NULL;
SDL_Window *g_window = NULL;
SDL_GLContext g_GLContext = NULL;
int g_width;
int g_height;


// startGame should set a SDL error on failure.
// We are also adding additional context to SDL errors to try to make
// them more descriptive.
int startGame() {
    srand(time(NULL));
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_SetError("SDL_Init() failed: %s", SDL_GetError());
        return 1;
    }


    if ((g_basePath = getEnvDir("PICOPUTT_BASE_PATH")) != NULL) {
        SDL_Log("Using base path %s set by $PICOPUTT_BASE_PATH", g_basePath);
    } else if ((g_basePath = SDL_GetBasePath()) == NULL) {
        SDL_Log("Failed to find base path, using current working directory");
        if (SET_ERR_IF_TRUE((g_basePath = SDL_strdup("./")) == NULL)) return 1;
    }


    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, TARGET_GL_MAJOR_VERSION);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, TARGET_GL_MINOR_VERSION);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    // TODO: g_width and g_height probably should be from SDL_GL_GetDrawableSize
    g_width = 960;
    g_height = 640;
    g_window = SDL_CreateWindow(
        "picoputt",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        g_width, g_height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL
    );

    if (g_window == NULL) {
        SDL_SetError("SDL_CreateWindow() failed: %s", SDL_GetError());
        return 1;
    }


    if ((g_GLContext = SDL_GL_CreateContext(g_window)) == NULL) {
        SDL_SetError("SDL_GL_CreateContext() failed: %s", SDL_GetError());
        return 1;
    }

    glewExperimental = GL_TRUE;
    {
        GLenum err = glewInit();
        if (err != GLEW_OK) {
            SDL_SetError("glewInit() failed: %s", glewGetErrorString(err));
            return 1;
        }
    }

    SDL_version linked;
    SDL_GetVersion(&linked);
    SDL_Log("Using SDL version %u.%u.%u", linked.major, linked.minor, linked.patch);

    SDL_Log("GL_VENDOR: %s", glGetString(GL_VENDOR));
    SDL_Log("GL_RENDERER: %s", glGetString(GL_RENDERER));
    SDL_Log("GL_VERSION: %s", glGetString(GL_VERSION));
    SDL_Log("GL_SHADING_LANGUAGE_VERSION: %s", glGetString(GL_SHADING_LANGUAGE_VERSION));

    if (SDL_GL_SetSwapInterval(-1) != 0) {
        SDL_Log("Adaptive vsync not supported, using regular vsync");
        SDL_GL_SetSwapInterval(1);
    }

    return loadResources();
}



void quitGame() {
    logGlErrors();
    freeResources();

    if (g_GLContext != NULL) {
        SDL_GL_DeleteContext(g_GLContext);
        g_GLContext = NULL;
    }

    if (g_window != NULL) {
        SDL_DestroyWindow(g_window);
        g_window = NULL;
    }

    if (g_basePath != NULL) {
        SDL_free(g_basePath);
        g_basePath = NULL;
    }

    SDL_Quit();
}



void showCritError(const char *fmt, ...) {
    va_list ap;
    char *message;
    int msgResult;
    va_start(ap, fmt);
    // Try to avoid allocation for literal string, as it may not be
    // reliable if showCritError is being called due to a memory error.
    if (strcmp(fmt, "%s") == 0) {
        message = va_arg(ap, char*);
        msgResult = -1;
    } else {
        msgResult = SDL_vasprintf(&message, fmt, ap);
        // If SDL_vasprintf fails, try at least to log something.
        if (msgResult == -1) message = "Something went horribly wrong!";
    }
    va_end(ap);

    // Extra goofiness -- empty error string probably means bugged error
    // reporting, such as forgetting to call SDL_SetError in a function
    // which signals errors with SDL errors.  It's better to display
    // something in that case than a totally empty dialog box.
    if (message[0] == '\0') {
        if (msgResult != -1) SDL_free(message);
        message = "(empty error message)";
        msgResult = -1;
    }

    SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "%s", message);
    // Note: it's fine for g_window to be NULL here.  But if window is created, message box will be modal.
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Critical error", message, g_window);

    // TODO: SDL_ShowSimpleMessageBox seems to be worse than MessageBoxA
    //  on windows.  With MessageBoxA text wraps, and you can copy the
    //  message with ctrl-c.
    //  Not sure how to get the window handle from SDL though.
    // MessageBoxA(NULL, message, "Critical error", MB_OK | MB_ICONERROR);

    if (msgResult != -1) SDL_free(message);
}
