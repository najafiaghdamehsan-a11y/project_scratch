#include "ui/app.h"
#include "core/log.h"
#include "model/model_ops.h"   // NEW: sprite/project ops + dirty flag
#include <SDL.h>
#include <cstdio>

static int pt_in_rect(int x, int y, const SDL_Rect& r) {
    return x >= r.x && x < (r.x + r.w) && y >= r.y && y < (r.y + r.h);
}

static void update_window_title(SDL_Window* win, const Project* p) {
    const char* name = "None";
    if (p && p->sprite_count > 0) {
        int a = p->active_sprite_index;
        if (a < 0) a = 0;
        if (a >= p->sprite_count) a = 0;
        name = p->sprites[a].name;
    }
    char title[256];
    std::snprintf(title, sizeof(title),
                  "project_scratch | Active: %s%s",
                  name,
                  project_is_dirty(p) ? " *" : "");
    SDL_SetWindowTitle(win, title);
}

static void draw_rect(SDL_Renderer* ren, const SDL_Rect& r, uint8_t rr, uint8_t gg, uint8_t bb) {
    SDL_SetRenderDrawColor(ren, rr, gg, bb, 255);
    SDL_RenderFillRect(ren, &r);
}

static void draw_rect_outline(SDL_Renderer* ren, const SDL_Rect& r, uint8_t rr, uint8_t gg, uint8_t bb) {
    SDL_SetRenderDrawColor(ren, rr, gg, bb, 255);
    SDL_RenderDrawRect(ren, &r);
}

int app_run(Project* project, Runtime* runtime) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        log_write(LogRecord{0,0,"SDL","Init failed", SDL_GetError(), LOG_ERROR});
        return 0;
    }

    const int WIN_W = 1200;
    const int WIN_H = 700;

    SDL_Window* win = SDL_CreateWindow("project_scratch",
                                       SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       WIN_W, WIN_H, SDL_WINDOW_SHOWN);
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

    update_window_title(win, project);

    // UI layout
    const int PANEL_W = 300;
    const int STAGE_W = WIN_W - PANEL_W;
    const int STAGE_H = WIN_H;

    // Panel buttons
    SDL_Rect btnAdd   { STAGE_W + 10, 10,  (PANEL_W - 40) / 3, 34 };
    SDL_Rect btnDel   { STAGE_W + 20 + btnAdd.w, 10, (PANEL_W - 40) / 3, 34 };
    SDL_Rect btnVis   { STAGE_W + 30 + btnAdd.w + btnDel.w, 10, (PANEL_W - 40) / 3, 34 };

    const int listTop = 60;
    const int itemH = 36;

    int running = 1;
    while (running) {
        SDL_Event e;
        int titleNeedsUpdate = 0;

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;

            // Mouse: panel click select / buttons
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                int mx = e.button.x;
                int my = e.button.y;

                // Only handle clicks inside panel
                if (mx >= STAGE_W) {
                    if (pt_in_rect(mx, my, btnAdd)) {
                        // Add sprite and make it active
                        char nm[64];
                        std::snprintf(nm, sizeof(nm), "Sprite%d", project->sprite_count + 1);
                        int idx = project_add_sprite(project, nm);
                        if (idx >= 0) {
                            project_set_active_sprite(project, idx);
                            titleNeedsUpdate = 1;
                        }
                    } else if (pt_in_rect(mx, my, btnDel)) {
                        // Delete active sprite
                        if (project->sprite_count > 0) {
                            project_delete_sprite(project, project->active_sprite_index);
                            titleNeedsUpdate = 1;
                        }
                    } else if (pt_in_rect(mx, my, btnVis)) {
                        // Toggle visible on active
                        if (project->sprite_count > 0) {
                            int a = project->active_sprite_index;
                            if (a < 0) a = 0;
                            if (a >= project->sprite_count) a = 0;
                            int newVis = !project->sprites[a].visible;
                            sprite_set_visible(project, a, newVis);
                            titleNeedsUpdate = 1;
                        }
                    } else {
                        // Click list item to select
                        int idx = (my - listTop) / itemH;
                        if (idx >= 0 && idx < project->sprite_count) {
                            if (project_set_active_sprite(project, idx)) {
                                titleNeedsUpdate = 1;
                            }
                        }
                    }
                }
            }

            // Keyboard
            if (e.type == SDL_KEYDOWN) {
                SDL_Keycode key = e.key.keysym.sym;

                // Engine debug keys:
                if (key == SDLK_p) runtime_set_paused(runtime, !runtime->paused);
                if (key == SDLK_s) runtime_set_step_mode(runtime, !runtime->step_mode);
                if (key == SDLK_n) runtime_request_step(runtime);

                // Forward key to engine (green flag / broadcast logic lives in runtime)
                runtime_post_key(runtime, (int)key);

                // UI sprite edit keys (active sprite)
                if (project->sprite_count > 0) {
                    int a = project->active_sprite_index;
                    if (a < 0) a = 0;
                    if (a >= project->sprite_count) a = 0;

                    double x = project->sprites[a].x;
                    double y = project->sprites[a].y;

                    const double step = 5.0;
                    if (key == SDLK_LEFT)  { x -= step; sprite_set_pos(project, a, x, y); titleNeedsUpdate = 1; }
                    if (key == SDLK_RIGHT) { x += step; sprite_set_pos(project, a, x, y); titleNeedsUpdate = 1; }
                    if (key == SDLK_UP)    { y += step; sprite_set_pos(project, a, x, y); titleNeedsUpdate = 1; }
                    if (key == SDLK_DOWN)  { y -= step; sprite_set_pos(project, a, x, y); titleNeedsUpdate = 1; }

                    // Optional: size with -/= and rotation with [ ]
                    if (key == SDLK_LEFTBRACKET)  { sprite_set_dir(project, a, project->sprites[a].dir - 10.0); titleNeedsUpdate = 1; }
                    if (key == SDLK_RIGHTBRACKET) { sprite_set_dir(project, a, project->sprites[a].dir + 10.0); titleNeedsUpdate = 1; }
                    if (key == SDLK_MINUS)        { sprite_set_size(project, a, project->sprites[a].size - 5.0); titleNeedsUpdate = 1; }
                    if (key == SDLK_EQUALS)       { sprite_set_size(project, a, project->sprites[a].size + 5.0); titleNeedsUpdate = 1; }
                }
            }
        }

        if (titleNeedsUpdate) update_window_title(win, project);

        // Engine tick
        runtime_tick(runtime, project);

        // Clear whole window
        SDL_SetRenderDrawColor(ren, 20, 20, 20, 255);
        SDL_RenderClear(ren);

        // Draw stage background
        SDL_Rect stageRect{0, 0, STAGE_W, STAGE_H};
        draw_rect(ren, stageRect, 25, 25, 25);
        draw_rect_outline(ren, stageRect, 60, 60, 60);

        // Draw center crosshair (axis hint)
        SDL_SetRenderDrawColor(ren, 70, 70, 70, 255);
        SDL_RenderDrawLine(ren, STAGE_W/2, 0, STAGE_W/2, STAGE_H);
        SDL_RenderDrawLine(ren, 0, STAGE_H/2, STAGE_W, STAGE_H/2);

        // Draw sprites (all)
        for (int i = 0; i < project->sprite_count; i++) {
            Sprite* s = &project->sprites[i];
            if (!s->visible) continue;

            int cx = (STAGE_W / 2) + (int)s->x;
            int cy = (STAGE_H / 2) - (int)s->y;

            // size scales square
            double sc = s->size / 100.0;
            if (sc < 0.2) sc = 0.2;
            if (sc > 3.0) sc = 3.0;
            int half = (int)(20 * sc);

            SDL_Rect r{cx - half, cy - half, half * 2, half * 2};

            // Active sprite highlight
            if (i == project->active_sprite_index) {
                draw_rect(ren, r, 255, 255, 255);
                SDL_Rect outline{r.x - 2, r.y - 2, r.w + 4, r.h + 4};
                draw_rect_outline(ren, outline, 255, 215, 0);
            } else {
                // slightly gray for non-active
                draw_rect(ren, r, 200, 200, 200);
            }
        }

        // Draw panel background
        SDL_Rect panelRect{STAGE_W, 0, PANEL_W, WIN_H};
        draw_rect(ren, panelRect, 30, 30, 30);
        draw_rect_outline(ren, panelRect, 70, 70, 70);

        // Draw buttons (no text yet, but distinct colors)
        draw_rect(ren, btnAdd,  50, 120, 50);   // add
        draw_rect(ren, btnDel,  140, 50, 50);   // delete
        draw_rect(ren, btnVis,  60, 60, 140);   // toggle visible
        draw_rect_outline(ren, btnAdd,  200, 200, 200);
        draw_rect_outline(ren, btnDel,  200, 200, 200);
        draw_rect_outline(ren, btnVis,  200, 200, 200);

        // Draw sprite list items (rectangles)
        for (int i = 0; i < project->sprite_count; i++) {
            SDL_Rect item{STAGE_W + 10, listTop + i * itemH, PANEL_W - 20, itemH - 6};

            if (i == project->active_sprite_index) {
                draw_rect(ren, item, 80, 80, 80);
                draw_rect_outline(ren, item, 255, 215, 0);
            } else {
                draw_rect(ren, item, 55, 55, 55);
                draw_rect_outline(ren, item, 120, 120, 120);
            }

            // Small "visible" indicator dot (left)
            SDL_Rect dot{item.x + 6, item.y + 8, 16, 16};
            if (project->sprites[i].visible) draw_rect(ren, dot, 200, 200, 200);
            else draw_rect(ren, dot, 80, 80, 80);

            // Color swatch for sprite index (no text yet)
            SDL_Rect sw{item.x + 26, item.y + 8, 16, 16};
            uint8_t rr = (uint8_t)(50 + (i * 40) % 200);
            uint8_t gg = (uint8_t)(50 + (i * 80) % 200);
            uint8_t bb = (uint8_t)(50 + (i * 120) % 200);
            draw_rect(ren, sw, rr, gg, bb);
        }

        SDL_RenderPresent(ren);
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 1;
}