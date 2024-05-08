#ifndef PICOPUTT_GAME_H
#define PICOPUTT_GAME_H
#include <GL/glew.h>
#include <SDL.h>

extern char *g_basePath;
extern SDL_Window *g_window;
extern SDL_GLContext g_GLContext;
extern int g_width;
extern int g_height;

int startGame();
void quitGame();
void showCritError(const char *fmt, ...);
#endif //PICOPUTT_GAME_H
