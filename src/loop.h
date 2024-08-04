#ifndef PICOPUTT_LOOP_H
#define PICOPUTT_LOOP_H
#include "resources.h"
int gameLoop();
float clubPixSize();
void updateDisplayInfo();
void uniformDisplayRelative(ProgSurface prog, float scale, SDL_Point drCenter);
#endif //PICOPUTT_LOOP_H
