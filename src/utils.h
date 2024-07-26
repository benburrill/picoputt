#ifndef PICOPUTT_UTILS_H
#define PICOPUTT_UTILS_H
#include <GL/glew.h>
#include <SDL.h>

char *getEnvDir(const char *var);
char *getGlErrorString(GLenum errCode);
int processGlErrors(const char *info);
void logGlErrors();

// Intended usage: if (SET_ERR_IF_TRUE(bad stuff)) return error code;
// If the condition is true, it calls SDL_SetError with some debug info.
// This is intended for errors the player probably can't fix (so don't
// need to be very user-friendly).  I am using SDL's error system for
// keeping track of error messages, so when my functions fail, I always
// want to set an SDL error (which the caller can deal with as desired).
// This function makes it convenient to set an error message that is at
// least more useful than a poorly-written or empty error message.
// If you want to report the error immediately, you can of course do
// if (SET_ERR_IF_TRUE(cond)) showCritError("%s", SDL_GetError());
#define SET_ERR_IF_TRUE(cond) ((cond)? (SDL_SetError("Error: %s @ %s:%d", #cond, __func__, __LINE__), 1) : 0)
#endif //PICOPUTT_UTILS_H
