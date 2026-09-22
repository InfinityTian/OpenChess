#include "gui.h"
#include "paths.h"
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <string.h>

#ifndef OPENCHESS_VERSION
#define OPENCHESS_VERSION "dev"
#endif

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--version") == 0) {
        printf("OpenChess %s\n", OPENCHESS_VERSION);
        return 0;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    if (TTF_Init() != 0) {
        fprintf(stderr, "TTF_Init: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }
    if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == 0) {
        fprintf(stderr, "IMG_Init: %s\n", IMG_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    if (!net_init())
        fprintf(stderr, "net: local multiplayer unavailable\n");

    SDL_Window *win = SDL_CreateWindow(
        "OpenChess", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIN_W, WIN_H,
        SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);
    if (!win) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    /* Window/taskbar icon: the same artwork as the .app and .dmg. */
    {
        char icon_path[1100];
        snprintf(icon_path, sizeof icon_path, "%s/openchess.png", path_assets());
        SDL_Surface *icon = IMG_Load(icon_path);
        if (icon) {
            SDL_SetWindowIcon(win, icon);
            SDL_FreeSurface(icon);
        }
    }

    SDL_Renderer *ren = SDL_CreateRenderer(
        win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) {
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    Gui *gui = gui_create();
    if (!gui) {
        fprintf(stderr, "gui_create failed\n");
        return 1;
    }
    gui_load_config(gui, path_config());
    gui_init_assets(gui, ren);
    SDL_StartTextInput();

    SDL_Event e;
    while (!gui_quit(gui)) {
        while (SDL_PollEvent(&e))
            gui_handle_event(gui, &e);
        gui_tick(gui, SDL_GetTicks());
        gui_render(gui, ren);
        SDL_Delay(8);
    }

    SDL_StopTextInput();
    gui_save_config(gui);
    gui_destroy(gui);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    net_shutdown();
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
    return 0;
}
