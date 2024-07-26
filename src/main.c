#include "utils.h"
#include "game.h"
#include "loop.h"


int main(int argc, char *argv[]) {
    int err = 0;
    if ((err = startGame()) != 0 || (err = gameLoop()) != 0) {
        showCritError("%s", SDL_GetError());
    }

    quitGame();
    return err;
}
