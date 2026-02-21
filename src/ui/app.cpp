#include "ui/app.h"
#include "core/log.h"
#include <SDL.h>

int app_run(Project* project, Runtime* runtime) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        log_write(LogRecord{0,0,"SDL","Init failed", SDL_GetError(), LOG_ERROR});
        return 0;
    }

    SDL_Window* win = SDL_CreateWindow("project_scratch",
                                       SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       1200, 700, SDL_WINDOW_SHOWN);
    if (!win) {
        log_write(LogRecord{0,0,"SDL","CreateWindow failed", SDL_GetError(), LOG_ERROR});
        SDL_Quit();
        return 0;
    }

    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) {
        log_write(LogRecord{0,0,"SDL","CreateRenderer failed", SDL_GetError(), LOG_ERROR});
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 0;
    }

    int running = 1;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;

            // Temporary keyboard controls for engine testing:
            // P = pause/resume, S = toggle step mode, N = do one step
            // G = green flag start, X = stop all
            if (e.type == SDL_KEYDOWN && e.key.repeat == 0) {
                // NEW: forward every keydown to engine
                runtime_post_key(runtime, (int)e.key.keysym.sym);

                if (e.key.keysym.sym == SDLK_p) runtime_set_paused(runtime, !runtime->paused);
                if (e.key.keysym.sym == SDLK_s) runtime_set_step_mode(runtime, !runtime->step_mode);
                if (e.key.keysym.sym == SDLK_n) runtime_request_step(runtime);

                if (e.key.keysym.sym == SDLK_g) runtime_green_flag(runtime);
                if (e.key.keysym.sym == SDLK_x) runtime_stop_all(runtime);
            }
        }

        runtime_tick(runtime, project);

        SDL_SetRenderDrawColor(ren, 20, 20, 20, 255);
        SDL_RenderClear(ren);

        // Placeholder: draw sprite as a square
        if (project->sprite_count > 0 && project->sprites[0].visible) {
            int cx = 600 + (int)project->sprites[0].x;
            int cy = 350 - (int)project->sprites[0].y;
            SDL_Rect r{cx - 20, cy - 20, 40, 40};
            SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
            SDL_RenderFillRect(ren, &r);
        }

        SDL_RenderPresent(ren);
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 1;
}