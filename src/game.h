#ifndef PICOPUTT_GAME_H
#define PICOPUTT_GAME_H
#include <GL/glew.h>
#include <SDL.h>

extern char *g_basePath;
extern SDL_Window *g_window;
extern SDL_GLContext g_GLContext;
// Width and height of the window in screen units (ie "fake" pixels)
extern int g_scWidth;
extern int g_scHeight;
// Width and height of the window in draw units (ie "real" pixels)
// For use in glViewport, etc
extern int g_drWidth;
extern int g_drHeight;

int startGame();
void quitGame();
void showCritError(const char *fmt, ...);
#endif //PICOPUTT_GAME_H
